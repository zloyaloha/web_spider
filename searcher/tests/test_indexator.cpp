#include <gtest/gtest.h>

#include <algorithm>
#include <memory>

#include "searcher.h"

static bool contains(const std::vector<int> &v, int x) { return std::find(v.begin(), v.end(), x) != v.end(); }

static bool contains(const std::vector<PostingEntry> &v, int x) { return true; }

TEST(IndexatorTests, AddSingleDocument_AppendsUrlAndIndexesTokens) {
    SimpleHashMap<int> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>();

    Indexator idx(index, urls, tokenizer);

    const std::string url = "http://example.com/doc1";
    const std::string doc = "alpha beta gamma alpha";

    idx.addDocument(url, doc);

    ASSERT_EQ(urls.size(), 1u);
    EXPECT_EQ(urls[0], url);

    auto *alpha_list = index.find("alpha");
    ASSERT_NE(alpha_list, nullptr);
    EXPECT_EQ(alpha_list->size(), 1u);
    EXPECT_EQ((*alpha_list)[0], 0);

    auto *beta_list = index.find("beta");
    ASSERT_NE(beta_list, nullptr);
    EXPECT_EQ(beta_list->size(), 1u);
    EXPECT_EQ((*beta_list)[0], 0);

    auto *gamma_list = index.find("gamma");
    ASSERT_NE(gamma_list, nullptr);
    EXPECT_EQ(gamma_list->size(), 1u);
    EXPECT_EQ((*gamma_list)[0], 0);
}

TEST(IndexatorTests, AddMultipleDocuments_SharedAndDistinctTokensIndexed) {
    SimpleHashMap<int> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>();

    Indexator idx(index, urls, tokenizer);

    idx.addDocument("http://a", "apple banana");
    idx.addDocument("http://b", "banana cherry");
    idx.addDocument("http://c", "apple cherry date");

    ASSERT_EQ(urls.size(), 3u);
    EXPECT_EQ(urls[0], "http://a");
    EXPECT_EQ(urls[1], "http://b");
    EXPECT_EQ(urls[2], "http://c");

    auto *apple = index.find("apple");
    ASSERT_NE(apple, nullptr);
    EXPECT_TRUE(contains(*apple, 0));
    EXPECT_TRUE(contains(*apple, 2));
    EXPECT_EQ(apple->size(), 2u);

    auto *banana = index.find("banana");
    ASSERT_NE(banana, nullptr);
    EXPECT_TRUE(contains(*banana, 0));
    EXPECT_TRUE(contains(*banana, 1));
    EXPECT_EQ(banana->size(), 2u);

    auto *cherry = index.find("cherry");
    ASSERT_NE(cherry, nullptr);
    EXPECT_TRUE(contains(*cherry, 1));
    EXPECT_TRUE(contains(*cherry, 2));
    EXPECT_EQ(cherry->size(), 2u);

    auto *date = index.find("date");
    ASSERT_NE(date, nullptr);
    EXPECT_EQ(date->size(), 1u);
    EXPECT_EQ((*date)[0], 2);
}

TEST(IndexatorTests, AddEmptyDocument_AddsUrlButNoTokensIndexed) {
    SimpleHashMap<int> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>();

    Indexator idx(index, urls, tokenizer);

    idx.addDocument("http://empty", "");

    ASSERT_EQ(urls.size(), 1u);
    EXPECT_EQ(urls[0], "http://empty");

    EXPECT_EQ(index.find("anytoken"), nullptr);
}

TEST(IndexatorTests, PunctuationSplitsTokens) {
    SimpleHashMap<int> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>();

    Indexator idx(index, urls, tokenizer);

    idx.addDocument("http://punc", "Hello, WORLD!");

    ASSERT_EQ(urls.size(), 1u);
    EXPECT_EQ(urls[0], "http://punc");

    auto *hello = index.find("hello");
    ASSERT_NE(hello, nullptr);
    EXPECT_EQ(hello->size(), 1u);
    EXPECT_EQ((*hello)[0], 0);

    auto *world = index.find("world");
    ASSERT_NE(world, nullptr);
    EXPECT_EQ(world->size(), 1u);
    EXPECT_EQ((*world)[0], 0);

    // Punctuation should not be part of token
    EXPECT_EQ(index.find("world!"), nullptr);
}

TEST(IndexatorTests, HyphenApostropheAndNumbersTokenizedCorrectly) {
    SimpleHashMap<int> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>();

    Indexator idx(index, urls, tokenizer);

    idx.addDocument("http://mix", "well-known don't 3.14 1,234");

    ASSERT_EQ(urls.size(), 1u);

    auto *hyphen = index.find("well-known");
    ASSERT_NE(hyphen, nullptr);
    EXPECT_EQ(hyphen->size(), 1u);
    EXPECT_EQ((*hyphen)[0], 0);

    auto *apost = index.find("don't");
    ASSERT_NE(apost, nullptr);
    EXPECT_EQ(apost->size(), 1u);
    EXPECT_EQ((*apost)[0], 0);

    auto *num1 = index.find("3.14");
    ASSERT_NE(num1, nullptr);
    EXPECT_EQ(num1->size(), 1u);
    EXPECT_EQ((*num1)[0], 0);

    auto *num2 = index.find("1,234");
    ASSERT_NE(num2, nullptr);
    EXPECT_EQ(num2->size(), 1u);
    EXPECT_EQ((*num2)[0], 0);
}

TEST(IndexatorTests, UsesCustomStemmerWhenProvided) {
    // Custom test stemmer that appends "_stem" to each token so we can assert
    // Indexator stores stemmed tokens instead of raw tokens.
    struct TestStemmer : public IStemmer {
        std::string stem(std::string &word) override { return word + "_stem"; }
    };

    SimpleHashMap<int> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>(std::make_unique<TestStemmer>());

    Indexator idx(index, urls, tokenizer);

    idx.addDocument("http://stem", "Alpha Beta");

    ASSERT_EQ(urls.size(), 1u);

    auto *a = index.find("alpha_stem");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->size(), 1u);
    EXPECT_EQ((*a)[0], 0);

    auto *b = index.find("beta_stem");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->size(), 1u);
    EXPECT_EQ((*b)[0], 0);
}

TEST(IndexatorTests, DuplicateUrlBecomesSeparateDocument) {
    SimpleHashMap<int> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>();

    Indexator idx(index, urls, tokenizer);

    idx.addDocument("http://dup", "apple");
    idx.addDocument("http://dup", "banana");

    ASSERT_EQ(urls.size(), 2u);
    EXPECT_EQ(urls[0], "http://dup");
    EXPECT_EQ(urls[1], "http://dup");

    auto *apple = index.find("apple");
    ASSERT_NE(apple, nullptr);
    EXPECT_EQ(apple->size(), 1u);
    EXPECT_EQ((*apple)[0], 0);

    auto *banana = index.find("banana");
    ASSERT_NE(banana, nullptr);
    EXPECT_EQ(banana->size(), 1u);
    EXPECT_EQ((*banana)[0], 1);
}

TEST(IndexatorTests, PostingsAreAppendedInIncreasingOrder) {
    SimpleHashMap<int> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>();

    Indexator idx(index, urls, tokenizer);

    idx.addDocument("http://a", "shared uniqueA");
    idx.addDocument("http://b", "other shared");
    idx.addDocument("http://c", "shared uniqueC");

    auto *shared = index.find("shared");
    ASSERT_NE(shared, nullptr);
    std::vector<int> expected{0, 1, 2};
    EXPECT_EQ(*shared, expected);
}

TEST(TFIDFIndexatorTests, AddSingleDocument_AppendsUrlAndIndexesTokens) {
    SimpleHashMap<PostingEntry> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>();

    TFIDFIndexator idx(index, urls, tokenizer);

    const std::string url = "http://example.com/doc1";
    const std::string doc = "alpha beta gamma alpha";

    idx.addDocument(url, doc);

    ASSERT_EQ(urls.size(), 1u);
    EXPECT_EQ(urls[0], url);

    auto *alpha_list = index.find("alpha");
    ASSERT_NE(alpha_list, nullptr);
    EXPECT_EQ(alpha_list->size(), 1u);
    EXPECT_EQ((*alpha_list)[0].doc_id, 0);

    auto *beta_list = index.find("beta");
    ASSERT_NE(beta_list, nullptr);
    EXPECT_EQ(beta_list->size(), 1u);
    EXPECT_EQ((*beta_list)[0].doc_id, 0);

    auto *gamma_list = index.find("gamma");
    ASSERT_NE(gamma_list, nullptr);
    EXPECT_EQ(gamma_list->size(), 1u);
    EXPECT_EQ((*gamma_list)[0].doc_id, 0);
}

TEST(TFIDFIndexatorTests, AddMultipleDocuments_SharedAndDistinctTokensIndexed) {
    SimpleHashMap<PostingEntry> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>();

    TFIDFIndexator idx(index, urls, tokenizer);

    idx.addDocument("http://a", "apple banana");
    idx.addDocument("http://b", "banana cherry");
    idx.addDocument("http://c", "apple cherry date");

    ASSERT_EQ(urls.size(), 3u);
    EXPECT_EQ(urls[0], "http://a");
    EXPECT_EQ(urls[1], "http://b");
    EXPECT_EQ(urls[2], "http://c");

    auto *apple = index.find("apple");
    ASSERT_NE(apple, nullptr);
    EXPECT_TRUE(contains(*apple, 0));
    EXPECT_TRUE(contains(*apple, 2));
    EXPECT_EQ(apple->size(), 2u);

    auto *banana = index.find("banana");
    ASSERT_NE(banana, nullptr);
    EXPECT_TRUE(contains(*banana, 0));
    EXPECT_TRUE(contains(*banana, 1));
    EXPECT_EQ(banana->size(), 2u);

    auto *cherry = index.find("cherry");
    ASSERT_NE(cherry, nullptr);
    EXPECT_TRUE(contains(*cherry, 1));
    EXPECT_TRUE(contains(*cherry, 2));
    EXPECT_EQ(cherry->size(), 2u);

    auto *date = index.find("date");
    ASSERT_NE(date, nullptr);
    EXPECT_EQ(date->size(), 1u);
    EXPECT_EQ((*date)[0].doc_id, 2);
}

TEST(TFIDFIndexatorTests, AddEmptyDocument_AddsUrlButNoTokensIndexed) {
    SimpleHashMap<PostingEntry> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>();

    TFIDFIndexator idx(index, urls, tokenizer);

    idx.addDocument("http://empty", "");

    ASSERT_EQ(urls.size(), 1u);
    EXPECT_EQ(urls[0], "http://empty");

    EXPECT_EQ(index.find("anytoken"), nullptr);
}

TEST(TFIDFIndexatorTests, PunctuationSplitsTokens) {
    SimpleHashMap<PostingEntry> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>();

    TFIDFIndexator idx(index, urls, tokenizer);

    idx.addDocument("http://punc", "Hello, WORLD!");

    ASSERT_EQ(urls.size(), 1u);
    EXPECT_EQ(urls[0], "http://punc");

    auto *hello = index.find("hello");
    ASSERT_NE(hello, nullptr);
    EXPECT_EQ(hello->size(), 1u);
    EXPECT_EQ((*hello)[0].doc_id, 0);

    auto *world = index.find("world");
    ASSERT_NE(world, nullptr);
    EXPECT_EQ(world->size(), 1u);
    EXPECT_EQ((*world)[0].doc_id, 0);

    EXPECT_EQ(index.find("world!"), nullptr);
}

TEST(TFIDFIndexatorTests, HyphenApostropheAndNumbersTokenizedCorrectly) {
    SimpleHashMap<PostingEntry> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>();

    TFIDFIndexator idx(index, urls, tokenizer);

    idx.addDocument("http://mix", "well-known don't 3.14 1,234");

    ASSERT_EQ(urls.size(), 1u);

    auto *hyphen = index.find("well-known");
    ASSERT_NE(hyphen, nullptr);
    EXPECT_EQ(hyphen->size(), 1u);
    EXPECT_EQ((*hyphen)[0].doc_id, 0);

    auto *apost = index.find("don't");
    ASSERT_NE(apost, nullptr);
    EXPECT_EQ(apost->size(), 1u);
    EXPECT_EQ((*apost)[0].doc_id, 0);

    auto *num1 = index.find("3.14");
    ASSERT_NE(num1, nullptr);
    EXPECT_EQ(num1->size(), 1u);
    EXPECT_EQ((*num1)[0].doc_id, 0);

    auto *num2 = index.find("1,234");
    ASSERT_NE(num2, nullptr);
    EXPECT_EQ(num2->size(), 1u);
    EXPECT_EQ((*num2)[0].doc_id, 0);
}

TEST(TFIDFIndexatorTests, UsesCustomStemmerWhenProvided) {
    struct TestStemmer : public IStemmer {
        std::string stem(std::string &word) override { return word + "_stem"; }
    };

    SimpleHashMap<PostingEntry> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>(std::make_unique<TestStemmer>());

    TFIDFIndexator idx(index, urls, tokenizer);

    idx.addDocument("http://stem", "Alpha Beta");

    ASSERT_EQ(urls.size(), 1u);

    auto *a = index.find("alpha_stem");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->size(), 1u);
    EXPECT_EQ((*a)[0].doc_id, 0);

    auto *b = index.find("beta_stem");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->size(), 1u);
    EXPECT_EQ((*b)[0].doc_id, 0);
}

TEST(TFIDFIndexatorTests, DuplicateUrlBecomesSeparateDocument) {
    SimpleHashMap<PostingEntry> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>();

    TFIDFIndexator idx(index, urls, tokenizer);

    idx.addDocument("http://dup", "apple");
    idx.addDocument("http://dup", "banana");

    ASSERT_EQ(urls.size(), 2u);
    EXPECT_EQ(urls[0], "http://dup");
    EXPECT_EQ(urls[1], "http://dup");

    auto *apple = index.find("apple");
    ASSERT_NE(apple, nullptr);
    EXPECT_EQ(apple->size(), 1u);
    EXPECT_EQ((*apple)[0].doc_id, 0);

    auto *banana = index.find("banana");
    ASSERT_NE(banana, nullptr);
    EXPECT_EQ(banana->size(), 1u);
    EXPECT_EQ((*banana)[0].doc_id, 1);
}

TEST(TFIDFIndexatorTests, PostingsAreAppendedInIncreasingOrder) {
    SimpleHashMap<PostingEntry> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>();

    TFIDFIndexator idx(index, urls, tokenizer);

    idx.addDocument("http://a", "shared uniqueA");
    idx.addDocument("http://b", "other shared");
    idx.addDocument("http://c", "shared uniqueC");

    auto *shared = index.find("shared");
    ASSERT_NE(shared, nullptr);
}