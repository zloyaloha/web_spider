package urlqueue

import (
	"crypto/md5"
	"fmt"
	"net/url"
	"regexp"
	"strings"
	"sync"
)

type URLQueue struct {
	URLs     map[string]bool
	Queue    []string
	Source   string
	MaxPages int
	mu       sync.Mutex
}


func (q *URLQueue) Add(urlStr string) bool {
	q.mu.Lock()
	defer q.mu.Unlock()

	normalized := NormalizeURL(urlStr)
	if !q.URLs[normalized] {
		q.URLs[normalized] = true
		q.Queue = append(q.Queue, normalized)
		return true
	}
	return false
}

func (q *URLQueue) Get() (string, bool) {
	q.mu.Lock()
	defer q.mu.Unlock()

	if len(q.Queue) == 0 {
		return "", false
	}
	url := q.Queue[0]
	q.Queue = q.Queue[1:]
	return url, true
}

func NewURLQueue(source string, maxPages int) *URLQueue {
	return &URLQueue{
		URLs:     make(map[string]bool),
		Queue:    make([]string, 0),
		Source:   source,
		MaxPages: maxPages,
	}
}

func (q *URLQueue) Size() int {
	return len(q.Queue)
}

func NormalizeURL(urlStr string) string {
	parsed, err := url.Parse(urlStr)
	if err != nil {
		return urlStr
	}

	parsed.Fragment = ""

	parsed.Host = strings.TrimPrefix(parsed.Host, "www.")

	if parsed.Scheme == "" {
		parsed.Scheme = "https"
	}

	return parsed.String()
}

func ExtractLinksFromHTML(htmlContent, baseURL string) []string {
	var links []string
	re := regexp.MustCompile(`href=["']([^"']+)["']`)
	matches := re.FindAllStringSubmatch(htmlContent, -1)

	for _, match := range matches {
		if len(match) > 1 {
			link := match[1]
			if strings.HasPrefix(link, "http") {
				links = append(links, link)
			} else if strings.HasPrefix(link, "/") {
				parsed, _ := url.Parse(baseURL)
				absURL := parsed.Scheme + "://" + parsed.Host + link
				links = append(links, absURL)
			}
		}
	}
	return links
}

func ComputeContentHash(content string) string {
	hash := md5.Sum([]byte(content))
	return fmt.Sprintf("%x", hash)
}

func URLShouldBeFollowed(urlStr string, followPatterns, excludePatterns []string) bool {
	for _, pattern := range excludePatterns {
		if URLMatchesPattern(urlStr, pattern) {
			return false
		}
	}

	if len(followPatterns) == 0 {
		return true
	}

	for _, pattern := range followPatterns {
		if URLMatchesPattern(urlStr, pattern) {
			return true
		}
	}

	return false
}

func URLMatchesPattern(urlStr string, pattern string) bool {
	re, err := regexp.Compile(pattern)
	if err != nil {
		return false
	}
	return re.MatchString(urlStr)
}
