package app

import (
	"context"
	"fmt"
	"log"
	"net/url"
	"regexp"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"time"
	"web_spider/internal/config"
	"web_spider/internal/db"
	"web_spider/internal/models"
	"web_spider/internal/utils"

	"github.com/PuerkitoBio/goquery"
	"github.com/go-shiori/go-readability"
	"github.com/gocolly/colly"
	"github.com/gocolly/colly/extensions"
)

var (
	reWhitespace  = regexp.MustCompile(`\s+`)
	reArticleDate = regexp.MustCompile(`/\d{4}/[a-z]{3}/\d{1,2}/`)
)

type PageType int

const (
	PageTypeUnknown PageType = iota
	PageTypeCategory
	PageTypeArticle
)

// String representation for debugging
func (p PageType) String() string {
	switch p {
	case PageTypeCategory:
		return "Category"
	case PageTypeArticle:
		return "Article"
	default:
		return "Unknown"
	}
}

type Spider struct {
	collector    *colly.Collector
	db           *db.MongoDB
	visited      sync.Map

	pageCount    int64
	articleCount int64

	maxPages     int64
	maxDepth     int
	recrawlHours int

	ctx        context.Context
	cancelFunc context.CancelFunc
	paused     int32

	saveChan chan *models.Document
	wgSave   sync.WaitGroup

	doneChan chan struct{}
}

func NewNewsCrawler(cfg *config.SpiderConfig) *Spider {
	mongoDB, err := db.NewMongoDB(cfg.DB)
	if err != nil {
		log.Fatalf("Failed to connect to DB: %v", err)
		return nil
	}

	ctx, cancel := context.WithCancel(context.Background())

	c := colly.NewCollector(
		colly.AllowedDomains("www.theguardian.com", "theguardian.com", "en.wikipedia.org"),
		colly.UserAgent("Mozilla/5.0 (NewsBot/1.0)"),
		colly.Async(true),
	)

	c.IgnoreRobotsTxt = false

	extensions.RandomUserAgent(c)

	c.Limit(&colly.LimitRule{
		DomainGlob:  "*",
		Parallelism: cfg.Logic.MaxConcurrentWorkers,
		Delay:       time.Duration(cfg.Logic.DelayMS) * time.Millisecond,
		RandomDelay: 500 * time.Millisecond,
	})

	return &Spider{
		collector:    c,
		db:           mongoDB,
		ctx:          ctx,
		cancelFunc:   cancel,
		maxDepth:     cfg.Logic.MaxDepth,
		recrawlHours: cfg.Logic.RecrawlThresholdHours,
		saveChan:     make(chan *models.Document, 1000),
		doneChan:     make(chan struct{}),
	}
}

func (w *Spider) Start(startURLs []string, maxPages int64) {
	w.maxPages = maxPages

	w.setupCollector()
	w.startWriter()

	go w.runPhases(startURLs)

	w.cliControlLoop()
}

func (w *Spider) runPhases(startURLs []string) {
	defer close(w.doneChan)

	fmt.Println("PHASE 1: Recrawl started...")

	staleURLs, err := w.db.GetStaleDocuments(w.recrawlHours, 200)
	if err == nil && len(staleURLs) > 0 {
		fmt.Printf("Found %d stale pages. Processing...\n", len(staleURLs))

		for _, u := range staleURLs {
			w.waitIfPaused()
			if w.isStopped() {
				return
			}

			ctx := colly.NewContext()
			ctx.Put("is_recrawl", "true")
			ctx.Put("depth", "0")

			w.collector.Request("GET", u, nil, ctx, nil)
		}

		w.collector.Wait()
		fmt.Println("PHASE 1: Recrawl finished.")
	} else {
		fmt.Println("No stale pages found. Skipping Phase 1.")
	}

	if w.isStopped() {
		return
	}

	fmt.Println("PHASE 2: Discovery started...")

	startCtx := colly.NewContext()
	startCtx.Put("depth", "0")
	startCtx.Put("is_recrawl", "false")

	for _, u := range startURLs {
		if w.isStopped() {
			return
		}
		w.collector.Request("GET", u, nil, startCtx, nil)
	}

	w.collector.Wait()
	fmt.Println("All phases completed.")
}

func (w *Spider) isStopped() bool {
	select {
	case <-w.ctx.Done():
		return true
	default:
		return false
	}
}

func (w *Spider) setupCollector() {
	w.collector.OnRequest(func(r *colly.Request) {
		select {
		case <-w.ctx.Done():
			r.Abort()
			return
		default:
		}

		for atomic.LoadInt32(&w.paused) == 1 {
			select {
			case <-w.ctx.Done():
				r.Abort()
				return
			default:
				time.Sleep(500 * time.Millisecond)
			}
		}

		isRecrawl := r.Ctx.Get("is_recrawl") == "true"
		if !isRecrawl && atomic.LoadInt64(&w.pageCount) >= w.maxPages {
			r.Abort()
			return
		}
	})

	w.collector.OnHTML("html", func(e *colly.HTMLElement) {
		urlStr := e.Request.URL.String()
		depth, _ := strconv.Atoi(e.Request.Ctx.Get("depth"))
		isRecrawl := e.Request.Ctx.Get("is_recrawl") == "true"

		var pageType PageType
		var source string
		if strings.Contains(urlStr, "en.wikipedia.org") {
			pageType = w.determinePageTypeWiki(urlStr)
			source = "wikipedia"
		} else if strings.Contains(urlStr, "theguardian.com") {
			pageType = w.determinePageTypeGuardian(urlStr)
			source = "theguardian"
		} else {
			pageType = PageTypeUnknown
		}

		if pageType == PageTypeArticle {
			article, err := w.extractContent(string(e.Response.Body), urlStr)
			if err == nil && len(article.Text) > 200 {
				doc := models.Document{
					Title:         article.Title,
					HTMLContent:   article.HTML,
					Source: source,
					ContentHash:   utils.ComputeContentHash(article.HTML),
					NormalizedURL: utils.NormalizeURL(urlStr),
					LastScraped:   time.Now().Unix(),
					ContentLength: len(article.HTML),
				}
				select {
				case w.saveChan <- &doc:
				default:
					log.Println("Save queue full, dropping document")
				}
			}
		}

		if !isRecrawl {
			w.processLinks(e, depth, pageType)
		}
	})

	w.collector.OnError(func(r *colly.Response, err error) {
        if err == colly.ErrRobotsTxtBlocked {
            log.Printf("Skipped (Robots.txt): %s", r.Request.URL)
            return
        }

        if r.StatusCode != 429 {
            log.Printf("Error on %s: %v", r.Request.URL, err)
        }
    })
}

func getURLPath(href string) string {
	u, err := url.Parse(href)
	if err != nil {
		return ""
	}
	return u.Path
}

func (w *Spider) determinePageTypeGuardian(href string) PageType {
	path := getURLPath(href)
	if path == "" {
		return PageTypeUnknown
	}

	if strings.Contains(href, "/video/") || strings.Contains(href, "/audio/") ||
		strings.Contains(href, "/gallery/") || strings.Contains(href, "/crosswords/") {
		return PageTypeUnknown
	}

	if reArticleDate.MatchString(path) {
		return PageTypeArticle
	}

	segments := strings.Split(strings.Trim(path, "/"), "/")
	if len(segments) >= 1 && len(segments) <= 3 {
		return PageTypeCategory
	}

	return PageTypeUnknown
}

func (w *Spider) determinePageTypeWiki(href string) PageType {
	path := getURLPath(href)
	if path == "" {
		return PageTypeUnknown
	}

	excluded := []string{
		"File:", "Special:", "Talk:", "User:", "Template:",
		"Help:", "Wikipedia:", "Draft:", "Portal:", "Main_Page",
		"Template_talk:", "Category_talk:",
		"action=", "diff=", "oldid=",
	}
	for _, exc := range excluded {
		if strings.Contains(path, exc) {
			return PageTypeUnknown
		}
	}
	if strings.Contains(path, "/Category:") {
		return PageTypeCategory
	}

	if strings.HasPrefix(path, "/wiki/") {
		return PageTypeArticle
	}
	return PageTypeUnknown
}

func (sc *Spider) extractContent(rawHTML string, pageURL string) (*models.ExtractedArticle, error) {
	parsedURL, err := url.Parse(pageURL)
	if err != nil {
		return nil, err
	}

	reader := strings.NewReader(rawHTML)
	article, err := readability.FromReader(reader, parsedURL)
	if err != nil {
		return nil, err
	}

	doc, err := goquery.NewDocumentFromReader(strings.NewReader(article.Content))
	if err != nil {
		return nil, err
	}

	doc.Find("figure, .dcr-citation, aside, .element-atom, script, style, .mw-editsection").Remove()

	cleanText := doc.Text()
	cleanText = reWhitespace.ReplaceAllString(cleanText, " ")
	cleanText = strings.TrimSpace(strings.Split(cleanText, "Reuse this content")[0])

	return &models.ExtractedArticle{
		Title:   article.Title,
		Text:    cleanText,
		HTML:    article.Content,
		Excerpt: article.Excerpt,
	}, nil
}

func (w *Spider) processLinks(e *colly.HTMLElement, currentDepth int, currentPageType PageType) {
	if currentDepth >= w.maxDepth {
		return
	}

	currentURL := e.Request.URL

	if strings.Contains(currentURL.Host, "wikipedia.org") {
		if currentPageType == PageTypeArticle {
			return
		}
	}

	targetSelector := "main a[href], section a[href], .fc-container a[href], #mw-pages a[href], #mw-subcategories a[href]"

	e.ForEach(targetSelector, func(_ int, el *colly.HTMLElement) {
		href := el.Attr("href")
		absoluteURL := el.Request.AbsoluteURL(href)

		u, err := url.Parse(absoluteURL)
		if err != nil {
			return
		}

		u.RawQuery = ""
		u.Fragment = ""
		cleanURL := u.String()

		if _, loaded := w.visited.LoadOrStore(cleanURL, true); loaded {
			return
		}

		var targetPageType PageType
		if strings.Contains(cleanURL, "en.wikipedia.org") {
			targetPageType = w.determinePageTypeWiki(cleanURL)
		} else if strings.Contains(cleanURL, "theguardian.com") {
			targetPageType = w.determinePageTypeGuardian(cleanURL)
		} else {
			targetPageType = PageTypeUnknown
		}

		if targetPageType == PageTypeUnknown {
			return
		}

		ctx := colly.NewContext()
		ctx.Put("depth", strconv.Itoa(currentDepth+1))

		w.collector.Request("GET", cleanURL, nil, ctx, nil)
	})
}

func (w *Spider) cliControlLoop() {
	fmt.Println("Controls: [p]ause, [r]esume, [s]top")

	go func() {
		<-w.doneChan
		fmt.Println("\nCrawling finished. Press Enter to exit report...")
	}()

	var input string
	for {
		select {
		case <-w.ctx.Done():
			w.shutdown()
			return
		default:
		}
		_, err := fmt.Scanln(&input)
		if err != nil {
			select {
			case <-w.doneChan:
				w.shutdown()
				return
			default:
				time.Sleep(100 * time.Millisecond)
				continue
			}
		}

		switch strings.ToLower(input) {
		case "p":
			if atomic.CompareAndSwapInt32(&w.paused, 0, 1) {
				fmt.Println("PAUSED. Waiting for workers to sleep...")
			}
		case "r":
			if atomic.CompareAndSwapInt32(&w.paused, 1, 0) {
				fmt.Println("RESUMED")
			}
		case "s", "q":
			fmt.Println("STOPPING initiated...")
			w.cancelFunc()
			return
		}
	}
}

func (w *Spider) shutdown() {
	if w.saveChan != nil {
		close(w.saveChan)
	}
	w.wgSave.Wait()
	fmt.Printf("\n📊 REPORT: Scraped Pages: %d | Articles Saved: %d\n", w.pageCount, w.articleCount)
	fmt.Println("👋 Bye!")
}

func (w *Spider) startWriter() {
	w.wgSave.Add(1)
	go func() {
		defer w.wgSave.Done()
		for doc := range w.saveChan {
			w.waitIfPaused()
			if err := w.db.SaveDocument(doc); err != nil {
				log.Printf("Error saving to DB: %v", err)
			} else {
				atomic.AddInt64(&w.articleCount, 1)
				count := atomic.AddInt64(&w.pageCount, 1)
				if count%5 == 0 {
					fmt.Printf("Saved: %d ... \r", count)
				}
			}
		}
	}()
}

func (w *Spider) waitIfPaused() {
	if atomic.LoadInt32(&w.paused) == 0 {
		return
	}

	for atomic.LoadInt32(&w.paused) == 1 {
		select {
		case <-w.ctx.Done():
			return
		case <-time.After(200 * time.Millisecond):
			continue
		}
	}
}