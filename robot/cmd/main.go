package main

import (
	"fmt"
	"web_spider/internal/app"
	"web_spider/internal/config"
)

func generateArchiveURLs(base string, fin_page int) []string {
    var urls []string
    for page := 0; page < fin_page; page++ {
		urls = append(urls, fmt.Sprintf("%s?page=%d", base, page))
    }
    return urls
}

func main() {
	config, err := config.LoadConfig("config.yaml")

	if err != nil {
		panic(err)
	}

	crawler := app.NewNewsCrawler(config)

	startURLGuardian := "https://www.theguardian.com/sport"
	// startURLWiki := "https://en.wikipedia.org/wiki/Category:Sports"
	urls := generateArchiveURLs(startURLGuardian, 100);
	urls = append(urls, startURLGuardian)
	// urls = append(urls, startURLWiki)


	crawler.Start(urls, 1000000000)
}
