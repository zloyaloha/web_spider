package main

import (
	"fmt"
	"os"
	"web_spider/internal/app"
	"web_spider/internal/config"
)
func main() {
	config, err := config.LoadConfig("config.yaml")

	if err != nil {
		panic(err)
	}

	crawler := app.NewWikiCrawler(config)

	startURL := "https://en.wikipedia.org/wiki/Category:Sports"

	if err := crawler.Crawl(startURL, 30000); err != nil {
		fmt.Printf("Ошибка: %v\n", err)
		os.Exit(1)
	}
}
