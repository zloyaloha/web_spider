package models

type ExtractedArticle struct {
	Title   string
	Text    string
	HTML    string
	Excerpt string
}

type Document struct {
	ID            string    `bson:"_id"`
	URL           string    `bson:"url"`
	NormalizedURL string    `bson:"normalized_url"`
	Source        string    `bson:"source"`
	HTMLContent   string    `bson:"html_content"`
	Title         string    `bson:"title"`
	ContentHash   string    `bson:"content_hash"`
	FirstScraped  int64     `bson:"first_scraped"`
	LastScraped   int64     `bson:"last_scraped"`
	ContentLength int       `bson:"content_length"`
}