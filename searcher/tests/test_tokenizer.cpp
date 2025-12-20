#include <gtest/gtest.h>

#include <vector>

#include "tokenizer.h"

class DummyTokenizerTest : public ::testing::Test {
protected:
    std::unique_ptr<Tokenizer> CreateDummyTokenizer() {
        auto stemmer = std::make_unique<DummyStemmer>();
        return std::make_unique<Tokenizer>(std::move(stemmer));
    }
};

TEST_F(DummyTokenizerTest, SimpleTokenization) {
    auto tokenizer = CreateDummyTokenizer();

    tokenizer->tokenize("hello world test");
    auto tokens = tokenizer->getTokens();

    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0], "hello");
    EXPECT_EQ(tokens[1], "world");
    EXPECT_EQ(tokens[2], "test");
}

TEST_F(DummyTokenizerTest, SingleWord) {
    auto tokenizer = CreateDummyTokenizer();

    tokenizer->tokenize("hello");
    auto tokens = tokenizer->getTokens();

    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0], "hello");
}

TEST_F(DummyTokenizerTest, EmptyString) {
    auto tokenizer = CreateDummyTokenizer();

    tokenizer->tokenize("");
    auto tokens = tokenizer->getTokens();

    EXPECT_EQ(tokens.size(), 0);
}

TEST_F(DummyTokenizerTest, MultipleSpaces) {
    auto tokenizer = CreateDummyTokenizer();

    tokenizer->tokenize("hello    world    test");
    auto tokens = tokenizer->getTokens();

    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0], "hello");
    EXPECT_EQ(tokens[1], "world");
    EXPECT_EQ(tokens[2], "test");
}

TEST_F(DummyTokenizerTest, LeadingAndTrailingSpaces) {
    auto tokenizer = CreateDummyTokenizer();

    tokenizer->tokenize("   hello world   ");
    auto tokens = tokenizer->getTokens();

    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0], "hello");
    EXPECT_EQ(tokens[1], "world");
}

TEST_F(DummyTokenizerTest, TabsAsDelimiter) {
    auto tokenizer = CreateDummyTokenizer();

    tokenizer->tokenize("hello\tworld\ttest");
    auto tokens = tokenizer->getTokens();

    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0], "hello");
    EXPECT_EQ(tokens[1], "world");
    EXPECT_EQ(tokens[2], "test");
}

TEST_F(DummyTokenizerTest, NewlinesAsDelimiter) {
    auto tokenizer = CreateDummyTokenizer();

    tokenizer->tokenize("hello\nworld\ntest");
    auto tokens = tokenizer->getTokens();

    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0], "hello");
    EXPECT_EQ(tokens[1], "world");
    EXPECT_EQ(tokens[2], "test");
}

TEST_F(DummyTokenizerTest, MixedWhitespace) {
    auto tokenizer = CreateDummyTokenizer();

    tokenizer->tokenize("hello \t world \n test");
    auto tokens = tokenizer->getTokens();

    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0], "hello");
    EXPECT_EQ(tokens[1], "world");
    EXPECT_EQ(tokens[2], "test");
}

TEST_F(DummyTokenizerTest, PunctuationRemoved) {
    auto tokenizer = CreateDummyTokenizer();

    tokenizer->tokenize("hello, world! test?");
    auto tokens = tokenizer->getTokens();

    std::vector<std::string> expected = {"hello", "world", "test"};
    EXPECT_EQ(tokens, expected);
}

TEST_F(DummyTokenizerTest, HyphenatedWords) {
    auto tokenizer = CreateDummyTokenizer();

    tokenizer->tokenize("well-known test-case");
    auto tokens = tokenizer->getTokens();

    std::vector<std::string> expected = {"well-known", "test-case"};
    EXPECT_EQ(tokens, expected);
}

TEST_F(DummyTokenizerTest, NumbersInText) {
    auto tokenizer = CreateDummyTokenizer();

    tokenizer->tokenize("test 123 hello 456");
    auto tokens = tokenizer->getTokens();

    std::vector<std::string> expected = {"test", "123", "hello", "456"};
    EXPECT_EQ(tokens, expected);
}

TEST_F(DummyTokenizerTest, OnlyNumbers) {
    auto tokenizer = CreateDummyTokenizer();

    tokenizer->tokenize("123 456 789");
    auto tokens = tokenizer->getTokens();

    std::vector<std::string> expected = {"123", "456", "789"};
    EXPECT_EQ(tokens, expected);
}

TEST_F(DummyTokenizerTest, OnlySpecialCharacters) {
    auto tokenizer = CreateDummyTokenizer();

    tokenizer->tokenize("!@#$%^&*()");
    auto tokens = tokenizer->getTokens();

    EXPECT_TRUE(tokens.empty());
}

TEST_F(DummyTokenizerTest, MixedSpecialCharacters) {
    auto tokenizer = CreateDummyTokenizer();

    tokenizer->tokenize("hello@world.com test#tag");
    auto tokens = tokenizer->getTokens();

    std::vector<std::string> expected = {"hello", "world", "com", "test", "tag"};
    EXPECT_EQ(tokens, expected);
}

TEST_F(DummyTokenizerTest, UppercaseLetters) {
    auto tokenizer = CreateDummyTokenizer();

    tokenizer->tokenize("HELLO WORLD TEST");
    auto tokens = tokenizer->getTokens();

    std::vector<std::string> expected = {"hello", "world", "test"};
    EXPECT_EQ(tokens, expected);
}

TEST_F(DummyTokenizerTest, MixedCase) {
    auto tokenizer = CreateDummyTokenizer();

    tokenizer->tokenize("Hello WoRlD TeSt");
    auto tokens = tokenizer->getTokens();

    std::vector<std::string> expected = {"hello", "world", "test"};
    EXPECT_EQ(tokens, expected);
}

TEST_F(DummyTokenizerTest, TokensAmount) {
    auto tokenizer = CreateDummyTokenizer();

    tokenizer->tokenize("hello world test");
    EXPECT_EQ(tokenizer->tokensAmount(), 3);
}

TEST_F(DummyTokenizerTest, AverageTokenLength) {
    auto tokenizer = CreateDummyTokenizer();

    tokenizer->tokenize("hi hello world");
    double avg = tokenizer->avgTokenLen();
    double expected = (2.0 + 5.0 + 5.0) / 3.0;
    EXPECT_NEAR(avg, expected, 0.01);
}

TEST_F(DummyTokenizerTest, AverageTokenLengthEmptyString) {
    auto tokenizer = CreateDummyTokenizer();

    tokenizer->tokenize("");
    double avg = tokenizer->avgTokenLen();
    EXPECT_EQ(avg, 0.0);
}

TEST_F(DummyTokenizerTest, VeryLongWord) {
    auto tokenizer = CreateDummyTokenizer();

    std::string long_word(10000, 'a');
    tokenizer->tokenize(long_word);
    auto tokens = tokenizer->getTokens();

    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].length(), 10000);
}

TEST_F(DummyTokenizerTest, ManyShortWords) {
    auto tokenizer = CreateDummyTokenizer();

    std::string text;
    for (int i = 0; i < 1000; i++) {
        text += "a ";
    }

    tokenizer->tokenize(text);
    auto tokens = tokenizer->getTokens();

    EXPECT_EQ(tokens.size(), 1000);
}

class PorterStemmerTokenizerTest : public ::testing::Test {
protected:
    std::unique_ptr<Tokenizer> CreatePorterTokenizer() {
        auto stemmer = std::make_unique<PorterStemmer>();
        return std::make_unique<Tokenizer>(std::move(stemmer));
    }
};

TEST_F(PorterStemmerTokenizerTest, RunningBecomeRun) {
    auto tokenizer = CreatePorterTokenizer();

    tokenizer->tokenize("running");
    auto tokens = tokenizer->getTokens();

    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0], "run");
}

TEST_F(PorterStemmerTokenizerTest, PluralForms) {
    auto tokenizer = CreatePorterTokenizer();

    tokenizer->tokenize("books cats dogs");
    auto tokens = tokenizer->getTokens();

    std::vector<std::string> expected = {"book", "cat", "dog"};
    EXPECT_EQ(tokens, expected);
}

TEST_F(PorterStemmerTokenizerTest, VerbForms) {
    auto tokenizer = CreatePorterTokenizer();

    tokenizer->tokenize("running runs run");
    auto tokens = tokenizer->getTokens();

    std::vector<std::string> expected = {"run", "run", "run"};
    EXPECT_EQ(tokens, expected);
}

TEST_F(PorterStemmerTokenizerTest, VerbEndings) {
    auto tokenizer = CreatePorterTokenizer();

    tokenizer->tokenize("working works worked");
    auto tokens = tokenizer->getTokens();

    std::vector<std::string> expected = {"work", "work", "work"};
    EXPECT_EQ(tokens, expected);
}

TEST_F(PorterStemmerTokenizerTest, AdjectiveEndings) {
    auto tokenizer = CreatePorterTokenizer();

    tokenizer->tokenize("beauty beautiful");
    auto tokens = tokenizer->getTokens();

    std::vector<std::string> expected = {"beauti", "beauti"};
    EXPECT_EQ(tokens, expected);
}

TEST_F(PorterStemmerTokenizerTest, AdjectiveEndingsNotEq) {
    auto tokenizer = CreatePorterTokenizer();

    tokenizer->tokenize("beautifully beautiful");
    auto tokens = tokenizer->getTokens();

    std::vector<std::string> expected = {"beautifulli", "beauti"};
    EXPECT_EQ(tokens, expected);
}

TEST_F(PorterStemmerTokenizerTest, ShortWordDoesntChange) {
    auto tokenizer = CreatePorterTokenizer();

    tokenizer->tokenize("cat dog run");
    auto tokens = tokenizer->getTokens();

    std::vector<std::string> expected = {"cat", "dog", "run"};
    EXPECT_EQ(tokens, expected);
}

TEST_F(PorterStemmerTokenizerTest, MixedText) {
    auto tokenizer = CreatePorterTokenizer();

    tokenizer->tokenize("The quickly running foxes jumped");
    auto tokens = tokenizer->getTokens();

    std::vector<std::string> expected = {"th", "quickli", "run", "fox", "jump"};
    EXPECT_EQ(tokens, expected);
}

TEST_F(PorterStemmerTokenizerTest, StemmsConsistency) {
    auto tokenizer1 = CreatePorterTokenizer();
    auto tokenizer2 = CreatePorterTokenizer();

    tokenizer1->tokenize("running");
    tokenizer2->tokenize("running");

    auto tokens1 = tokenizer1->getTokens();
    auto tokens2 = tokenizer2->getTokens();

    EXPECT_EQ(tokens1[0], tokens2[0]);
}

class TokenizerComparisonTest : public ::testing::Test {};

TEST_F(TokenizerComparisonTest, DummyVsPorter) {
    auto dummy = std::make_unique<Tokenizer>(std::make_unique<DummyStemmer>());
    auto porter = std::make_unique<Tokenizer>(std::make_unique<PorterStemmer>());

    std::string text = "running books connection";

    dummy->tokenize(text);
    porter->tokenize(text);

    auto dummy_tokens = dummy->getTokens();
    auto porter_tokens = porter->getTokens();

    std::vector<std::string> expected_dummy = {"running", "books", "connection"};
    std::vector<std::string> expected_porter = {"run", "book", "connect"};

    EXPECT_EQ(dummy_tokens, expected_dummy);
    EXPECT_EQ(porter_tokens, expected_porter);
}

class TokenizerPerformanceTest : public ::testing::Test {};

TEST_F(TokenizerPerformanceTest, LargeDocument) {
    auto tokenizer = std::make_unique<Tokenizer>(std::make_unique<DummyStemmer>());

    std::string large_text;
    for (int i = 0; i < 100000; i++) {
        large_text += "word" + std::to_string(i % 1000) + " ";
    }

    auto start = std::chrono::high_resolution_clock::now();
    tokenizer->tokenize(large_text);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(tokenizer->tokensAmount(), 100000);
    EXPECT_LT(duration.count(), 5000) << "Tokenization too slow: " << duration.count() << "ms";
}

TEST_F(TokenizerPerformanceTest, PorterStemmerPerformance) {
    auto tokenizer = std::make_unique<Tokenizer>(std::make_unique<PorterStemmer>());

    std::string text;
    for (int i = 0; i < 10000; i++) {
        text += "running works connections ";
    }

    auto start = std::chrono::high_resolution_clock::now();
    tokenizer->tokenize(text);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 2000) << "Porter stemming too slow: " << duration.count() << "ms";
}

TEST_F(TokenizerPerformanceTest, RepeatedTokenization) {
    auto tokenizer = std::make_unique<Tokenizer>(std::make_unique<DummyStemmer>());

    std::string text = "hello world test";

    for (int i = 0; i < 10000; i++) {
        tokenizer->tokenize(text);
        auto tokens = tokenizer->getTokens();
        EXPECT_EQ(tokens.size(), 3);
    }
}

class TokenizerUnicodeTest : public ::testing::Test {};

TEST_F(TokenizerUnicodeTest, UTF8Characters) {
    auto tokenizer = std::make_unique<Tokenizer>(std::make_unique<DummyStemmer>());

    tokenizer->tokenize("café naïve résumé");
    auto tokens = tokenizer->getTokens();

    // Current tokenizer treats non-ASCII bytes as delimiters; expect ASCII fragments only
    std::vector<std::string> expected = {"caf", "na", "ve", "r", "sum"};
    EXPECT_EQ(tokens, expected);
}

TEST_F(TokenizerUnicodeTest, CyrillicLetters) {
    auto tokenizer = std::make_unique<Tokenizer>(std::make_unique<DummyStemmer>());

    tokenizer->tokenize("привет мир тест");
    auto tokens = tokenizer->getTokens();

    // Non-ASCII Cyrillic characters are not considered alnum by isalnum; expect no tokens
    EXPECT_TRUE(tokens.empty());
}

TEST_F(TokenizerUnicodeTest, ChineseCharacters) {
    auto tokenizer = std::make_unique<Tokenizer>(std::make_unique<DummyStemmer>());

    tokenizer->tokenize("你好 世界 测试");
    auto tokens = tokenizer->getTokens();

    EXPECT_TRUE(tokens.empty());
}

TEST_F(TokenizerUnicodeTest, Emojis) {
    auto tokenizer = std::make_unique<Tokenizer>(std::make_unique<DummyStemmer>());

    tokenizer->tokenize("hello 😀 world 🌍");
    auto tokens = tokenizer->getTokens();

    std::vector<std::string> expected = {"hello", "world"};
    EXPECT_EQ(tokens, expected);
}

class TokenizerIntegrationTest : public ::testing::Test {};

TEST_F(TokenizerIntegrationTest, DocumentProcessing) {
    auto tokenizer = std::make_unique<Tokenizer>(std::make_unique<PorterStemmer>());

    std::string document = R"(
        Information retrieval is the activity of obtaining information
        resources relevant to an information need from a collection of
        information resources. Searches can be based on full-text or other
        content-based indexing.
    )";

    tokenizer->tokenize(document);
    auto tokens = tokenizer->getTokens();

    EXPECT_GT(tokens.size(), 10);
    EXPECT_LT(tokenizer->avgTokenLen(), 20);
}

TEST_F(TokenizerIntegrationTest, MultipleDocuments) {
    auto tokenizer = std::make_unique<Tokenizer>(std::make_unique<DummyStemmer>());

    std::vector<std::string> documents = {"First document", "Second document", "Third document"};

    for (const auto& doc : documents) {
        tokenizer->tokenize(doc);
        auto tokens = tokenizer->getTokens();
        EXPECT_EQ(tokens.size(), 2);
    }
}

TEST_F(TokenizerIntegrationTest, CleanupBetweenTokenizations) {
    auto tokenizer = std::make_unique<Tokenizer>(std::make_unique<DummyStemmer>());

    tokenizer->tokenize("hello world");
    EXPECT_EQ(tokenizer->tokensAmount(), 2);

    tokenizer->tokenize("test");
    EXPECT_EQ(tokenizer->tokensAmount(), 1);

    auto tokens = tokenizer->getTokens();
    EXPECT_EQ(tokens[0], "test");
}

class TokenizerEdgeCasesTest : public ::testing::Test {};

TEST_F(TokenizerEdgeCasesTest, OnlyWhitespace) {
    auto tokenizer = std::make_unique<Tokenizer>(std::make_unique<DummyStemmer>());

    tokenizer->tokenize("     \t\n  \t  ");
    auto tokens = tokenizer->getTokens();

    EXPECT_EQ(tokens.size(), 0);
}

TEST_F(TokenizerEdgeCasesTest, VeryLongDocument) {
    auto tokenizer = std::make_unique<Tokenizer>(std::make_unique<DummyStemmer>());

    std::string text;
    for (int i = 0; i < 1000; i++) {
        text += "word ";
    }

    tokenizer->tokenize(text);
    EXPECT_EQ(tokenizer->tokensAmount(), 1000);
}

TEST_F(TokenizerEdgeCasesTest, ConsecutiveSpecialCharacters) {
    auto tokenizer = std::make_unique<Tokenizer>(std::make_unique<DummyStemmer>());

    tokenizer->tokenize("hello!!!world???test...");
    auto tokens = tokenizer->getTokens();

    std::vector<std::string> expected = {"hello", "world", "test"};
    EXPECT_EQ(tokens, expected);
}

TEST_F(TokenizerEdgeCasesTest, URLInText) {
    auto tokenizer = std::make_unique<Tokenizer>(std::make_unique<DummyStemmer>());

    tokenizer->tokenize("Visit https://example.com for more info");
    auto tokens = tokenizer->getTokens();

    std::vector<std::string> expected = {"visit", "https", "example", "com", "for", "more", "info"};
    EXPECT_EQ(tokens, expected);
}

TEST_F(TokenizerEdgeCasesTest, EmailInText) {
    auto tokenizer = std::make_unique<Tokenizer>(std::make_unique<DummyStemmer>());

    tokenizer->tokenize("Contact me at user@example.com today");
    auto tokens = tokenizer->getTokens();

    std::vector<std::string> expected = {"contact", "me", "at", "user", "example", "com", "today"};
    EXPECT_EQ(tokens, expected);
}

TEST_F(TokenizerEdgeCasesTest, NumberFormats) {
    auto tokenizer = std::make_unique<Tokenizer>(std::make_unique<DummyStemmer>());

    tokenizer->tokenize("Version 1.2.3 costs $99.99 (50% off)");
    auto tokens = tokenizer->getTokens();

    std::vector<std::string> expected = {"version", "1.2", "3", "costs", "99.99", "50", "off"};
    EXPECT_EQ(tokens, expected);
}
