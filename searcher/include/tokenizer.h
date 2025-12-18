#pragma once

#include <string>
#include <string_view>
#include <vector>

class IStemmer {
public:
    virtual ~IStemmer() = default;
    virtual std::string stem(std::string& word) = 0;
};

class PorterStemmer : public IStemmer {
private:
    std::string word;
    int end;
    int j;

    bool isVowel(int i) const;

    int getMeasure(int limit) const;

    bool m_condition(int minM) const;

    bool hasVowel() const;

    bool isDoubleConsonant(int i) const;

    bool isCVC(int i) const;

    bool endsWith(const std::string& w, const std::string& suffix) const;

    std::string replaceSuffixIfMeasure(std::string& w, const std::string& suffix, const std::string& replacement, int minM);

    std::string replaceSuffixIfVowel(std::string& w, const std::string& suffix, const std::string& replacement);

    void step1();

    void step2();

    void step3();

    void step4();

    void step5();

public:
    std::string stem(std::string& input_word) override;
    PorterStemmer() = default;
};

class DummyStemmer : public IStemmer {
public:
    std::string stem(std::string& word) override;
    ~DummyStemmer() = default;
};

class Tokenizer {
private:
    // store tokens as owned strings to avoid string_view lifetime issues
    std::vector<std::string> tokens;
    uint64_t total_len;
    std::unique_ptr<IStemmer> stemmer;

public:
    Tokenizer(std::unique_ptr<IStemmer> stemmer);
    Tokenizer();
    virtual void tokenize(const std::string_view& text);
    // Return token copies (std::string) so callers don't get dangling views
    virtual std::vector<std::string> getRawTokens(const std::string_view& text) const;

    std::vector<std::string> getTokens() const;
    size_t tokensAmount() const;
    double avgTokenLen() const;
    char* nextToken();
};