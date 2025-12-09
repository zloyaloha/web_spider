package app

import (
	"context"
	"fmt"
	"io"
	"log"
	"net/http"
	"net/http/cookiejar"
	"net/url"
	"os"
	"os/signal"
	"regexp"
	"strings"
	"sync"
	"syscall"
	"time"
	"web_spider/internal/config"
	"web_spider/internal/db"
	"web_spider/internal/models"
	urlqueue "web_spider/internal/url_queue"

	"github.com/PuerkitoBio/goquery"
	"github.com/go-shiori/go-readability"
	"github.com/temoto/robotstxt"
	"golang.org/x/net/html/charset"
)

type SpiderApp struct {
	config  *config.SpiderConfig
	db      *db.MongoDB
	spiders map[string]*SourceSpider
	wg      sync.WaitGroup
	mu      sync.Mutex
	ctx     context.Context
	cancel  context.CancelFunc
}

type SourceSpider struct {
	source       string
	sourceConfig *config.SourceConfig
	db           *db.MongoDB
	spiderConfig *config.SpiderConfig
	queue        *urlqueue.URLQueue
	robotsGroup  *robotstxt.Group
	client       *http.Client
	stats        map[string]interface{}
	mu           sync.Mutex
}

const MaxHops = 15

func NewSpiderApp(cfg *config.SpiderConfig) (*SpiderApp, error) {
	mongoDB, err := db.NewMongoDB(cfg.DB)

	if err != nil {
		return nil, err
	}

	ctx, cancel := context.WithCancel(context.Background())

	spider_app := &SpiderApp{
		config:  cfg,
		spiders: make(map[string]*SourceSpider),
		db:      mongoDB,
		ctx:     ctx,
		cancel:  cancel,
	}

	for sourceName, sourceConfig := range cfg.Sources {
		jar, _ := cookiejar.New(nil)
		spider := &SourceSpider{
			source:       sourceName,
			sourceConfig: &sourceConfig,
			db:           mongoDB,
			spiderConfig: cfg,
			queue:        urlqueue.NewURLQueue(sourceName, sourceConfig.MaxPages),
			client: &http.Client{
				Transport: &http.Transport{
					DisableKeepAlives: true,
					MaxIdleConns:      0,
				},
				Jar:     jar,
				Timeout: time.Duration(cfg.Logic.TimeoutSec) * time.Second,
				CheckRedirect: func(req *http.Request, via []*http.Request) error {
					if len(via) >= MaxHops {
						return fmt.Errorf("stopped after %d redirects (MaxHops exceeded)", MaxHops)
					}
					return nil
				},
			},
			stats: make(map[string]interface{}),
		}

		spider_app.spiders[sourceName] = spider
	}
	return spider_app, nil
}

func (s *SpiderApp) Run() error {
	log.Println("\n🤖 Starting spiders...")
	log.Printf("📁 БД: %s\n", s.config.DB.Database)
	log.Printf("⚙️  Задержка между запросами: %d ms\n", s.config.Logic.DelayMS)
	log.Printf("🔄 Переобкачка старше чем: %d часов\n\n", s.config.Logic.RecrawlThresholdHours)

	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	for name := range s.config.Sources {
		s.wg.Add(1)
		go func(name string) {
			defer s.wg.Done()
			if sc, ok := s.spiders[name]; ok {
				if err := sc.Crawl(s.ctx); err != nil {
					log.Printf("❌ Ошибка при краулинге %s: %v\n", name, err)
				}
			}
		}(name)

		time.Sleep(1 * time.Second)
	}

	<-sigChan
	log.Println("\n\n⚠️  Получен сигнал прерывания, завершаю работу...")
	s.cancel()

	s.wg.Wait()

	return s.db.Close()
}

func (ss *SourceSpider) initRobotsTxt() {
	if len(ss.sourceConfig.StartURLs) == 0 {
		return
	}
	u, err := url.Parse(ss.sourceConfig.StartURLs[0])
	if err != nil {
		log.Printf("⚠️ Не могу распарсить URL для robots.txt: %v", err)
		return
	}

	robotsURL := fmt.Sprintf("%s://%s/robots.txt", u.Scheme, u.Host)
	log.Printf("🤖 Загружаю robots.txt: %s", robotsURL)

	resp, err := ss.client.Get(robotsURL)
	if err != nil {
		log.Printf("⚠️ Ошибка загрузки robots.txt (игнорируем): %v", err)
		return
	}
	defer resp.Body.Close()

	data, err := robotstxt.FromResponse(resp)
	if err != nil {
		log.Printf("⚠️ Ошибка парсинга robots.txt: %v", err)
		return
	}

	ss.robotsGroup = data.FindGroup(ss.spiderConfig.Logic.UserAgent)
	log.Println("✅ robots.txt успешно загружен и применен")
}

func (ss *SourceSpider) isAllowedURL(link string, followPatterns, excludePatterns []string) bool {
	u, err := url.Parse(link)
	if err != nil || u.Host == "" {
		return false
	}

	for _, pattern := range excludePatterns {
		if urlqueue.URLMatchesPattern(link, pattern) {
			return false
		}
	}

	if len(followPatterns) == 0 {
		return true
	}

	if len(followPatterns) > 0 {
		matched := false
		for _, pattern := range followPatterns {
			if urlqueue.URLMatchesPattern(link, pattern) {
				matched = true
				break
			}
		}
		if !matched {
			return false
		}
	}

	if ss.robotsGroup == nil {
		return true
	}

	return ss.robotsGroup.Test(u.Path)
}

func (ss *SourceSpider) Crawl(ctx context.Context) error {
	log.Printf("🚀 Начинаю краулинг %s\n", ss.source)

	ss.initRobotsTxt()

	for _, startURL := range ss.sourceConfig.StartURLs {
		ss.queue.Add(startURL)
	}

	maxWorkers := ss.spiderConfig.Logic.MaxConcurrentWorkers
	var workerWg sync.WaitGroup

	for i := 0; i < maxWorkers; i++ {
		workerWg.Add(1)
		go func(workerID int) {
			defer workerWg.Done()
			ss.worker(ctx, workerID)
		}(i)
	}

	workerWg.Wait()
	return nil
}

func (sc *SourceSpider) worker(ctx context.Context, workerID int) {
	for {
		select {
		case <-ctx.Done():
			return
		default:
		}

		urlStr, ok := sc.queue.Get()
		if !ok {
			time.Sleep(100 * time.Millisecond)
			continue
		}

		if !sc.isAllowedURL(urlStr, sc.sourceConfig.FollowPatterns, sc.sourceConfig.ExcludePatterns) {
			continue
		}

		time.Sleep(time.Duration(sc.spiderConfig.Logic.DelayMS) * time.Millisecond)

		doc, err := sc.Process(urlStr)

		if err != nil {
			log.Printf("Error while parsing or fetching HTML: %v", err)
			continue
		}

		links := sc.extractLinks(doc.HTMLContent, urlStr)

		count := 0
		for _, link := range links {
			if sc.isAllowedURL(link, sc.sourceConfig.FollowPatterns, sc.sourceConfig.ExcludePatterns) {
				sc.queue.Add(link)
				count++
			}
		}

		contentHash := urlqueue.ComputeContentHash(doc.Content)
		doc.ContentHash = contentHash
		doc.NormalizedURL = urlqueue.NormalizeURL(urlStr)
		doc.FirstScraped = time.Now().Unix()
		doc.LastScraped = time.Now().Unix()
		doc.StatusCode = 200
		doc.IsValid = true

		if len(doc.Content) > 100 {
			if err := sc.db.SaveDocument(doc); err != nil {
				log.Printf("❌ Ошибка при сохранении документа: %v\n", err)
				continue
			}
			log.Printf("document saved %s, added %d links, doc = %s\n", urlStr, count, doc.Content[:100])
		} else {
			log.Printf("document to small %s, added %d links\n", doc.NormalizedURL, count)
		}
	}
}

func (sc *SourceSpider) fetchURL(urlStr string) (string, int, error) {
	req, err := http.NewRequest("GET", urlStr, nil)
	if err != nil {
		return "", 0, err
	}

	req.Header.Set("User-Agent", sc.spiderConfig.Logic.UserAgent)
	req.Header.Set("Referer", "https://www.google.com/")

	resp, err := sc.client.Do(req)
	if err != nil {
		return "", 0, err
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return "", resp.StatusCode, fmt.Errorf("HTTP %d", resp.StatusCode)
	}

	utf8Reader, err := charset.NewReader(resp.Body, resp.Header.Get("Content-Type"))
	if err != nil {
		utf8Reader = resp.Body
	}

	bodyBytes, err := io.ReadAll(utf8Reader)
	if err != nil {
		return "", resp.StatusCode, err
	}

	bodyString := string(bodyBytes)
	lowerBody := strings.ToLower(bodyString)

	if strings.Contains(lowerBody, "captcha") ||
		strings.Contains(lowerBody, "security check") ||
		strings.Contains(lowerBody, "подтвердите, что вы человек") {
		return "", 200, fmt.Errorf("captcha detected")
	}

	return bodyString, resp.StatusCode, nil
}

func normalizeText(text string) string {
	text = regexp.MustCompile(`\s+`).ReplaceAllString(text, " ")
	text = strings.TrimSpace(text)
	return text
}

func addSpacesBeforeParsing(html string) string {
	blockElements := []string{"div", "p", "br", "li", "td", "tr", "h1", "h2", "h3", "h4", "h5", "h6"}
	result := html
	for _, tag := range blockElements {
		result = regexp.MustCompile(`<` + tag + `[^>]*>`).ReplaceAllString(result, " <"+tag+">")
		result = regexp.MustCompile(`</` + tag + `>`).ReplaceAllString(result, "</"+tag+"> ")
	}
	return result
}


func (sc *SourceSpider) extractContent(rawHTML string, pageURL string) (*models.ExtractedArticle, error) {
	parsedURL, err := url.Parse(pageURL)
	if err != nil {
		return nil, err
	}

	reader := strings.NewReader(rawHTML)

	article, err := readability.FromReader(reader, parsedURL)
	if err != nil {
		return nil, err
	}

	processedHTML := addSpacesBeforeParsing(article.Content)

	doc, err := goquery.NewDocumentFromReader(strings.NewReader(processedHTML))
	if err != nil {
		return nil, err
	}

	cleanText := doc.Text()

	return &models.ExtractedArticle{
		Title:   article.Title,
		Text:    normalizeText(cleanText),
		HTML:    article.Content,
		Excerpt: article.Excerpt,
	}, nil
}

func (sc *SourceSpider) Process(urlStr string) (*models.Document, error) {
	htmlBody, _, err := sc.fetchURL(urlStr)
	if err != nil {
		log.Printf("Ошибка скачивания %s: %v", urlStr, err)
		return nil, err
	}

	article, err := sc.extractContent(htmlBody, urlStr)
	if err != nil {
		log.Printf("Не удалось выделить текст %s: %v", urlStr, err)
		return nil, err
	}

	return &models.Document{
		Content:     article.Text,
		Title:       article.Title,
		HTMLContent: article.HTML,
	}, nil
}

func (sc *SourceSpider) extractLinks(rawHTML string, pageURL string) []string {
	var links []string
	reader := strings.NewReader(rawHTML)

	doc, err := goquery.NewDocumentFromReader(reader)
	if err != nil {
		fmt.Printf("Ошибка создания goquery документа: %v\n", err)
		return nil
	}

	baseURL, err := url.Parse(pageURL)
	if err != nil {
		fmt.Printf("Ошибка парсинга базового URL: %v\n", err)
		return nil
	}

	doc.Find("a").Each(func(i int, s *goquery.Selection) {
		href, exists := s.Attr("href")
		if !exists || href == "" || strings.HasPrefix(href, "#") || strings.HasPrefix(href, "mailto:") {
			return
		}
		parsedHref, err := url.Parse(href)
		if err != nil {
			return
		}

		resolvedURL := baseURL.ResolveReference(parsedHref)

		cleanURL := resolvedURL.String()
		if index := strings.Index(cleanURL, "?"); index != -1 {
			cleanURL = cleanURL[:index]
		}

		isDuplicate := false
		for _, l := range links {
			if l == cleanURL {
				isDuplicate = true
				break
			}
		}
		if !isDuplicate {
			links = append(links, cleanURL)
		}
	})

	return links
}
