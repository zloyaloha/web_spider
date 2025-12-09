package main

import (
	"log"
	"web_spider/internal/app"
	"web_spider/internal/config"
)

func main() {
	config, err := config.LoadConfig("config.yaml")

	if err != nil {
		panic(err)
	}

	app, err := app.NewSpiderApp(config)

	if err != nil {
		panic(err)
	}

	err = app.Run()

	if err != nil {
		panic(err)
	}

	log.Println("Spider succesfully run")

	// queue := urlqueue.UrlQueue{}
	// queue.ParseSitemap("https://matchtv.ru/sitemap.xml")
}
