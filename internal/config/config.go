package config

import (
	"os"

	"gopkg.in/yaml.v2"
)

type SourceConfig struct {
	Name             string   `yaml:"name"`
	BaseURLs         []string `yaml:"base_urls"`
	StartURLs        []string `yaml:"start_urls"`
	FollowPatterns   []string `yaml:"follow_patterns"`
	ExcludePatterns  []string `yaml:"exclude_patterns"`
	CSSSelectors     map[string]string `yaml:"css_selectors"`
	MaxPages         int      `yaml:"max_pages"`
	Priority         int      `yaml:"priority"`
}

type DBConfig struct {
	Connection  string `yaml:"connection"`
	Database    string `yaml:"database"`
	Collections struct {
		Documents    string `yaml:"documents"`
		SpiderState   string `yaml:"spider_state"`
		SpiderHistory string `yaml:"spider_history"`
	} `yaml:"collections"`
}

type LogicConfig struct {
	DelayMS                int `yaml:"delay_ms"`
	TimeoutSec             int `yaml:"timeout_sec"`
	MaxRetries             int `yaml:"max_retries"`
	RecrawlIntervalDays    int `yaml:"recrawl_interval_days"`
	RecrawlThresholdHours  int `yaml:"recrawl_threshold_hours"`
	MaxConcurrentWorkers   int `yaml:"max_concurrent_workers"`
	UserAgent              string `yaml:"user_agent"`
}

type SpiderConfig struct {
	DB     DBConfig                    `yaml:"db"`
	Logic  LogicConfig                 `yaml:"logic"`
	Sources map[string]SourceConfig    `yaml:"sources"`
}

func LoadConfig(path string) (*SpiderConfig, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	var cfg SpiderConfig
	if err := yaml.Unmarshal(data, &cfg); err != nil {
		return nil, err
	}
	return &cfg, nil
}
