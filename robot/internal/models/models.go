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
	LastModified  int64     `bson:"last_modified"`
	ScrapedCount  int       `bson:"scraped_count"`
	ContentLength int       `bson:"content_length"`
	StatusCode    int       `bson:"status_code"`
	IsValid       bool      `bson:"is_valid"`
	ErrorMessage  string    `bson:"error_message,omitempty"`
}

type SpiderState struct {
	ID                string    `bson:"_id"`
	Source            string    `bson:"source"`
	LastProcessedURL  string    `bson:"last_processed_url"`
	TotalProcessed    int       `bson:"total_processed"`
	TotalErrors       int       `bson:"total_errors"`
	LastTimestamp     int64     `bson:"last_timestamp"`
	PagesInQueue      int       `bson:"pages_in_queue"`
	IsPaused          bool      `bson:"is_paused"`
	LastResumeTime    int64     `bson:"last_resume_time"`
}

type SpiderHistory struct {
	ID            string `bson:"_id"`
	Source        string `bson:"source"`
	URL           string `bson:"url"`
	Status        string `bson:"status"`
	StatusCode    int    `bson:"status_code"`
	ContentHash   string `bson:"content_hash"`
	Timestamp     int64  `bson:"timestamp"`
	Duration      int    `bson:"duration_ms"`
	ErrorMessage  string `bson:"error_message,omitempty"`
}