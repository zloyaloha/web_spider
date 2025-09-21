package urlqueue

import (
	"encoding/xml"
	"fmt"
	"io"
	"log"
	"net/http"
)


type UrlQueue struct {
	Queue chan string
}

type SitemapIndex struct {
	Sitemaps []SitemapEntry `xml:"sitemap"`
}

type SitemapEntry struct {
	Loc string `xml:"loc"`
}

func (q *UrlQueue) ParseSitemap(sitemap_url string) error {
	resp, err := http.Get(sitemap_url)
	if err != nil {
		log.Fatal(err)
	}
	defer resp.Body.Close()

	data, err := io.ReadAll(resp.Body)
	if err != nil {
		log.Fatal(err)
	}

	// Парсим XML
	var si SitemapIndex
	if err := xml.Unmarshal(data, &si); err != nil {
		log.Fatal("Ошибка парсинга XML:", err)
	}

	// Выводим все sitemap файлы
	fmt.Printf("Найдено %d sitemap файлов:\n", len(si.Sitemaps))
	for _, s := range si.Sitemaps {
		fmt.Println("Sitemap:", s.Loc)
	}
}