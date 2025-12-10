package app

import (
	"fmt"
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
	urlqueue "web_spider/internal/url_queue"

	"github.com/PuerkitoBio/goquery"
	"github.com/go-shiori/go-readability"
	"github.com/gocolly/colly"
)

var (
    reWhitespace = regexp.MustCompile(`\s+`)
)

type PageType int

const (
	PageTypeUnknown PageType = iota
	PageTypeCategory
	PageTypeArticle
)

type WikiPage struct {
	URL      string
	Type     PageType
	Title    string
	Depth    int
}

type WikiCrawler struct {
	collector  *colly.Collector
	visited    map[string]bool
	db         *db.MongoDB
	queue      chan WikiPage
	mutex      sync.Mutex
	pageCount  int
	articleCount int
	categoryCount int
	workers    int
	paused         int32
    shouldStop     int32
	startTime  time.Time
	maxDepth   int
	tasksAdded     int64
    tasksCompleted int64
	recrawlInHours int
}

func NewWikiCrawler(cfg *config.SpiderConfig) *WikiCrawler {
	mongoDB, err := db.NewMongoDB(cfg.DB)

	if err != nil {
		return nil
	}

	collector := colly.NewCollector(
		colly.AllowedDomains("en.wikipedia.org", "wikipedia.org"),
		colly.UserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"),
		colly.Async(true),
	)

	collector.Limit(&colly.LimitRule{
		DomainGlob:  "*",
		Parallelism: cfg.Logic.MaxConcurrentWorkers,
		Delay:       time.Duration(cfg.Logic.DelayMS),
		RandomDelay: time.Duration(cfg.Logic.DelayMS) - 300,
	})

	return &WikiCrawler{
		collector:     collector,
		db: 		   mongoDB,
		visited:       make(map[string]bool),
		queue:         make(chan WikiPage, 1000),
		pageCount:     0,
		articleCount:  0,
		categoryCount: 0,
		workers:       cfg.Logic.MaxConcurrentWorkers,
		startTime:     time.Now(),
		maxDepth:      cfg.Logic.MaxDepth,
		paused:        0,
        shouldStop:    0,
		recrawlInHours: cfg.Logic.RecrawlThresholdHours,
	}
}

func (w *WikiCrawler) determinePageType(href string) PageType {
	if strings.Contains(href, "/wiki/Category:") {
		return PageTypeCategory
	}
	if strings.HasPrefix(href, "/wiki/") {
		excluded := []string{
			"File:", "Special:", "Talk:", "User:", "Template:",
			"Help:", "Wikipedia:", "Draft:",
		}
		for _, exc := range excluded {
			if strings.Contains(href, exc) {
				return PageTypeUnknown
			}
		}
		return PageTypeArticle
	}
	return PageTypeUnknown
}

func normalizeText(text string) string {
	text = reWhitespace.ReplaceAllString(text, " ")
    return strings.TrimSpace(text)
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

func (sc *WikiCrawler) extractContent(rawHTML string, pageURL string) (*models.ExtractedArticle, error) {
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

func (w *WikiCrawler) processLinks(e *colly.HTMLElement, currentDepth int) {
    if currentDepth >= w.maxDepth {
        return
    }

    urlStr := e.Request.URL.String()
    isCategory := strings.Contains(urlStr, "/wiki/Category:")

    if !isCategory {
        return
    }

    targetSelector := "#mw-subcategories a[href], #mw-pages a[href]"

    e.ForEach(targetSelector, func(_ int, el *colly.HTMLElement) {
        href := el.Attr("href")
        pageType := w.determinePageType(href)

        if pageType == PageTypeArticle || pageType == PageTypeCategory {
            absoluteURL := e.Request.AbsoluteURL(href)

            w.mutex.Lock()
			exists, _ := w.db.DocumentExists(urlqueue.NormalizeURL(absoluteURL))
            if !exists && w.pageCount < 100000 {
                w.mutex.Unlock()

                ctx := colly.NewContext()
                ctx.Put("depth", strconv.Itoa(currentDepth+1))
				atomic.AddInt64(&w.tasksAdded, 1)

                w.collector.Request("GET", absoluteURL, nil, ctx, nil)
            } else {
                w.mutex.Unlock()
            }
        }
    })
}

func (w *WikiCrawler) GetQueueSize() int64 {
    added := atomic.LoadInt64(&w.tasksAdded)
    completed := atomic.LoadInt64(&w.tasksCompleted)
    return added - completed
}

func (w *WikiCrawler) Crawl(startURL string, maxPages int) error {
	w.StartRecrawlRoutine(w.recrawlInHours)

	w.collector.OnScraped(func(r *colly.Response) {
		atomic.AddInt64(&w.tasksCompleted, 1)
	})

	w.collector.OnError(func(r *colly.Response, err error) {
		atomic.AddInt64(&w.tasksCompleted, 1)
	})

	atomic.StoreInt64(&w.tasksAdded, 1)

	w.listenForControl()

    w.collector.OnRequest(func(r *colly.Request) {
		if atomic.LoadInt32(&w.shouldStop) == 1 {
            r.Abort()
            return
        }

        for atomic.LoadInt32(&w.paused) == 1 {
            if atomic.LoadInt32(&w.shouldStop) == 1 {
                r.Abort()
                return
            }
            time.Sleep(1000 * time.Millisecond)
        }

        depth := r.Ctx.Get("depth")
        if depth == "" { depth = "0" }
        w.mutex.Lock()
        if w.pageCount >= maxPages {
            r.Abort()
            w.mutex.Unlock()
            return
        }
        w.mutex.Unlock()
    })

    w.collector.OnHTML("html", func(e *colly.HTMLElement) {
		for atomic.LoadInt32(&w.paused) == 1 {
            if atomic.LoadInt32(&w.shouldStop) == 1 {
                return
            }
            time.Sleep(500 * time.Millisecond)
        }

        depthStr := e.Request.Ctx.Get("depth")
        currentDepth, _ := strconv.Atoi(depthStr)

        urlStr := e.Request.URL.String()
        pageType := PageTypeArticle
        if strings.Contains(urlStr, "/wiki/Category:") {
            pageType = PageTypeCategory
        }

		queueSize := w.GetQueueSize()
		fmt.Printf("[%s] Depth: %d | Queue: %d | URL: %s\n",
			map[PageType]string{PageTypeArticle: "ART", PageTypeCategory: "CAT"}[pageType], currentDepth, queueSize, urlStr)

        if pageType == PageTypeArticle {
            article, err := w.extractContent(string(e.Response.Body), urlStr)
            if err == nil {
                doc := models.Document{
                    Content:       article.Text,
                    ContentLength: len(article.Text),
                    Title:         article.Title,
                    HTMLContent:   article.HTML,
                    ContentHash:   urlqueue.ComputeContentHash(article.Text),
                    NormalizedURL: urlqueue.NormalizeURL(urlStr),
                    LastScraped:   time.Now().Unix(),
                    IsValid:       true,
                }

                if err := w.db.SaveDocument(&doc); err == nil {
                    w.mutex.Lock()
                    w.articleCount++
                    w.pageCount++
                    w.mutex.Unlock()
                }
            } else {
                fmt.Printf("Ошибка экстракции: %v\n", err)
            }
        } else {
            w.mutex.Lock()
            w.categoryCount++
            w.mutex.Unlock()
        }
		isRecrawl := e.Request.Ctx.Get("is_recrawl") == "true"
		if isRecrawl {
			return
		}
        w.processLinks(e, currentDepth)
    })

    w.collector.OnError(func(r *colly.Response, err error) {
        fmt.Printf("⚠️ Ошибка (%s): %v\n", r.Request.URL, err)
    })

    fmt.Println("🚀 Запуск краулера...")

    w.mutex.Lock()
    w.visited[startURL] = true
    w.mutex.Unlock()

    startCtx := colly.NewContext()
    startCtx.Put("depth", "0")

    err := w.collector.Request("GET", startURL, nil, startCtx, nil)
    if err != nil {
        return err
    }

    w.collector.Wait()
	elapsed := time.Since(w.startTime)
    fmt.Printf("\n✅ Краулинг завершён!\n")
    fmt.Printf("Скачано статей: %d\n", w.articleCount)
    fmt.Printf("Посещено категорий: %d\n", w.categoryCount)
    fmt.Printf("Всего уникальных URL: %d\n", len(w.visited))
    fmt.Printf("Время выполнения: %v\n", elapsed)

    for {
        if atomic.LoadInt32(&w.shouldStop) == 1 {
            break
        }
        w.collector.Wait()
        time.Sleep(1 * time.Second)
    }

    return nil
}

func (w *WikiCrawler) listenForControl() {
    go func() {
        fmt.Println("\n-- КОНТРОЛЬ КРАУЛЕРА --")
        fmt.Println(" 'p' + Enter: Пауза | 'r' + Enter: Возобновить | 's' + Enter: Стоп")
        var input string
        for {
            fmt.Print(">> ")
            if _, err := fmt.Scanln(&input); err != nil {
                return
            }
            switch strings.ToLower(input) {
            case "p":
                if atomic.CompareAndSwapInt32(&w.paused, 0, 1) {
                    fmt.Println("⏸  Команда принята: ПАУЗА (ожидание завершения текущих запросов...)")
                } else {
                    fmt.Println("ℹ️  Уже на паузе")
                }
            case "r":
                if atomic.CompareAndSwapInt32(&w.paused, 1, 0) {
                    fmt.Println("▶  Команда принята: ПРОДОЛЖИТЬ")
                } else {
                    fmt.Println("ℹ️  Краулер и так работает")
                }
            }
        }
    }()
}

func (w *WikiCrawler) StartRecrawlRoutine(thresholdHours int) {
    ticker := time.NewTicker(5 * time.Minute)
    go func() {
        for {
            select {
            case <-ticker.C:
                if atomic.LoadInt32(&w.paused) == 1 || atomic.LoadInt32(&w.shouldStop) == 1 {
                    continue
                }

                staleURLs, err := w.db.GetStaleDocuments(thresholdHours, 50)
                if err != nil {
                    fmt.Printf("Ошибка получения устаревших URL: %v\n", err)
                    continue
                }

                for _, u := range staleURLs {
                    ctx := colly.NewContext()
                    ctx.Put("is_recrawl", "true")
                    ctx.Put("depth", "0")

                    w.collector.Request("GET", u, nil, ctx, nil)
                    atomic.AddInt64(&w.tasksAdded, 1)
                }
                if len(staleURLs) > 0 {
                    fmt.Printf("♻️ Добавлено на переобход: %d страниц\n", len(staleURLs))
                }
            }
        }
    }()
}