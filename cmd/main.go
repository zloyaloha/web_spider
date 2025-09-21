package main

import (
	"github.com/zloyaloha/web_spider/internal/url_queue/urlqueue"
)

func main() {
	queue := urlqueue.UrlQueue{}
	queue.ParseSitemap("https://matchtv.ru/sitemap.xml")
}
