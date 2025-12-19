#include <gtest/gtest.h>

#include <algorithm>
#include <memory>

#include "indexator.h"

static bool contains(const std::vector<int> &v, int x) { return std::find(v.begin(), v.end(), x) != v.end(); }

static bool contains(const std::vector<TermInfo> &v, int x) {
    return std::any_of(v.begin(), v.end(), [x](const TermInfo &p) { return p.doc_id == x; });
}

TEST(IndexatorTests, AddSingleDocument_AppendsUrlAndIndexesTokens) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    BinaryIndexator idx(src, tokenizer);

    const std::string url = "http://example.com/doc1";
    const std::string doc = "alpha beta gamma alpha";

    idx.addDocument(url, doc);

    ASSERT_EQ(src->urls.size(), 1u);
    EXPECT_EQ(src->urls[0], url);

    auto *alpha_list = src->index.find("alpha");
    ASSERT_NE(alpha_list, nullptr);
    EXPECT_EQ(alpha_list->size(), 1u);
    EXPECT_EQ((*alpha_list)[0].doc_id, 0);

    auto *beta_list = src->index.find("beta");
    ASSERT_NE(beta_list, nullptr);
    EXPECT_EQ(beta_list->size(), 1u);
    EXPECT_EQ((*beta_list)[0].doc_id, 0);

    auto *gamma_list = src->index.find("gamma");
    ASSERT_NE(gamma_list, nullptr);
    EXPECT_EQ(gamma_list->size(), 1u);
    EXPECT_EQ((*gamma_list)[0].doc_id, 0);
}

TEST(IndexatorTests, AddMultipleDocuments_SharedAndDistinctTokensIndexed) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    BinaryIndexator idx(src, tokenizer);

    idx.addDocument("http://a", "apple banana");
    idx.addDocument("http://b", "banana cherry");
    idx.addDocument("http://c", "apple cherry date");

    ASSERT_EQ(src->urls.size(), 3u);
    EXPECT_EQ(src->urls[0], "http://a");
    EXPECT_EQ(src->urls[1], "http://b");
    EXPECT_EQ(src->urls[2], "http://c");

    auto *apple = src->index.find("apple");
    ASSERT_NE(apple, nullptr);
    EXPECT_TRUE(contains(*apple, 0));
    EXPECT_TRUE(contains(*apple, 2));
    EXPECT_EQ(apple->size(), 2u);

    auto *banana = src->index.find("banana");
    ASSERT_NE(banana, nullptr);
    EXPECT_TRUE(contains(*banana, 0));
    EXPECT_TRUE(contains(*banana, 1));
    EXPECT_EQ(banana->size(), 2u);

    auto *cherry = src->index.find("cherry");
    ASSERT_NE(cherry, nullptr);
    EXPECT_TRUE(contains(*cherry, 1));
    EXPECT_TRUE(contains(*cherry, 2));
    EXPECT_EQ(cherry->size(), 2u);

    auto *date = src->index.find("date");
    ASSERT_NE(date, nullptr);
    EXPECT_EQ(date->size(), 1u);
    EXPECT_EQ((*date)[0].doc_id, 2);
}

TEST(IndexatorTests, AddEmptyDocument_AddsUrlButNoTokensIndexed) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    BinaryIndexator idx(src, tokenizer);

    idx.addDocument("http://empty", "");

    ASSERT_EQ(src->urls.size(), 1u);
    EXPECT_EQ(src->urls[0], "http://empty");

    EXPECT_EQ(src->index.find("anytoken"), nullptr);
}

TEST(IndexatorTests, PunctuationSplitsTokens) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    BinaryIndexator idx(src, tokenizer);

    idx.addDocument("http://punc", "Hello, WORLD!");

    ASSERT_EQ(src->urls.size(), 1u);
    EXPECT_EQ(src->urls[0], "http://punc");

    auto *hello = src->index.find("hello");
    ASSERT_NE(hello, nullptr);
    EXPECT_EQ(hello->size(), 1u);
    EXPECT_EQ((*hello)[0].doc_id, 0);

    auto *world = src->index.find("world");
    ASSERT_NE(world, nullptr);
    EXPECT_EQ(world->size(), 1u);
    EXPECT_EQ((*world)[0].doc_id, 0);

    // Punctuation should not be part of token
    EXPECT_EQ(src->index.find("world!"), nullptr);
}

TEST(IndexatorTests, HyphenApostropheAndNumbersTokenizedCorrectly) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    BinaryIndexator idx(src, tokenizer);

    idx.addDocument("http://mix", "well-known don't 3.14 1,234");

    ASSERT_EQ(src->urls.size(), 1u);

    auto *hyphen = src->index.find("well-known");
    ASSERT_NE(hyphen, nullptr);
    EXPECT_EQ(hyphen->size(), 1u);
    EXPECT_EQ((*hyphen)[0].doc_id, 0);

    auto *apost = src->index.find("don't");
    ASSERT_NE(apost, nullptr);
    EXPECT_EQ(apost->size(), 1u);
    EXPECT_EQ((*apost)[0].doc_id, 0);

    auto *num1 = src->index.find("3.14");
    ASSERT_NE(num1, nullptr);
    EXPECT_EQ(num1->size(), 1u);
    EXPECT_EQ((*num1)[0].doc_id, 0);

    auto *num2 = src->index.find("1,234");
    ASSERT_NE(num2, nullptr);
    EXPECT_EQ(num2->size(), 1u);
    EXPECT_EQ((*num2)[0].doc_id, 0);
}

TEST(IndexatorTests, UsesCustomStemmerWhenProvided) {
    // Custom test stemmer that appends "_stem" to each token so we can assert
    // Indexator stores stemmed tokens instead of raw tokens.
    struct TestStemmer : public IStemmer {
        std::string stem(std::string &word) override { return word + "_stem"; }
    };

    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>(std::make_unique<TestStemmer>());

    BinaryIndexator idx(src, tokenizer);

    idx.addDocument("http://stem", "Alpha Beta");

    ASSERT_EQ(src->urls.size(), 1u);

    auto *a = src->index.find("alpha_stem");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->size(), 1u);
    EXPECT_EQ((*a)[0].doc_id, 0);

    auto *b = src->index.find("beta_stem");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->size(), 1u);
    EXPECT_EQ((*b)[0].doc_id, 0);
}

TEST(IndexatorTests, DuplicateUrlBecomesSeparateDocument) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    BinaryIndexator idx(src, tokenizer);

    idx.addDocument("http://dup", "apple");
    idx.addDocument("http://dup", "banana");

    ASSERT_EQ(src->urls.size(), 2u);
    EXPECT_EQ(src->urls[0], "http://dup");
    EXPECT_EQ(src->urls[1], "http://dup");

    auto *apple = src->index.find("apple");
    ASSERT_NE(apple, nullptr);
    EXPECT_EQ(apple->size(), 1u);
    EXPECT_EQ((*apple)[0].doc_id, 0);

    auto *banana = src->index.find("banana");
    ASSERT_NE(banana, nullptr);
    EXPECT_EQ(banana->size(), 1u);
    EXPECT_EQ((*banana)[0].doc_id, 1);
}

TEST(IndexatorTests, PostingsAreAppendedInIncreasingOrder) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    BinaryIndexator idx(src, tokenizer);

    idx.addDocument("http://a", "shared uniqueA");
    idx.addDocument("http://b", "other shared");
    idx.addDocument("http://c", "shared uniqueC");

    auto *shared = src->index.find("shared");
    ASSERT_NE(shared, nullptr);
    std::vector<int> expected{0, 1, 2};
    std::vector<int> actual;
    for (const auto &p : *shared) actual.push_back(p.doc_id);
    EXPECT_EQ(actual, expected);
}

TEST(TFIDFIndexatorTests, AddSingleDocument_AppendsUrlAndIndexesTokens) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    TFIDFIndexator idx(src, tokenizer);

    const std::string url = "http://example.com/doc1";
    const std::string doc = "alpha beta gamma alpha";

    idx.addDocument(url, doc);

    ASSERT_EQ(src->urls.size(), 1u);
    EXPECT_EQ(src->urls[0], url);

    auto *alpha_list = src->index.find("alpha");
    ASSERT_NE(alpha_list, nullptr);
    EXPECT_EQ(alpha_list->size(), 1u);
    EXPECT_EQ((*alpha_list)[0].doc_id, 0);
    EXPECT_EQ((*alpha_list)[0].tf, 2);

    auto *beta_list = src->index.find("beta");
    ASSERT_NE(beta_list, nullptr);
    EXPECT_EQ(beta_list->size(), 1u);
    EXPECT_EQ((*beta_list)[0].doc_id, 0);

    auto *gamma_list = src->index.find("gamma");
    ASSERT_NE(gamma_list, nullptr);
    EXPECT_EQ(gamma_list->size(), 1u);
    EXPECT_EQ((*gamma_list)[0].doc_id, 0);
}

TEST(TFIDFIndexatorTests, AddMultipleDocuments_SharedAndDistinctTokensIndexed) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    TFIDFIndexator idx(src, tokenizer);

    idx.addDocument("http://a", "apple banana");
    idx.addDocument("http://b", "banana cherry");
    idx.addDocument("http://c", "apple cherry date");

    ASSERT_EQ(src->urls.size(), 3u);
    EXPECT_EQ(src->urls[0], "http://a");
    EXPECT_EQ(src->urls[1], "http://b");
    EXPECT_EQ(src->urls[2], "http://c");

    auto *apple = src->index.find("apple");
    ASSERT_NE(apple, nullptr);
    EXPECT_TRUE(contains(*apple, 0));
    EXPECT_TRUE(contains(*apple, 2));
    EXPECT_EQ(apple->size(), 2u);

    auto *banana = src->index.find("banana");
    ASSERT_NE(banana, nullptr);
    EXPECT_TRUE(contains(*banana, 0));
    EXPECT_TRUE(contains(*banana, 1));
    EXPECT_EQ(banana->size(), 2u);

    auto *cherry = src->index.find("cherry");
    ASSERT_NE(cherry, nullptr);
    EXPECT_TRUE(contains(*cherry, 1));
    EXPECT_TRUE(contains(*cherry, 2));
    EXPECT_EQ(cherry->size(), 2u);

    auto *date = src->index.find("date");
    ASSERT_NE(date, nullptr);
    EXPECT_EQ(date->size(), 1u);
    EXPECT_EQ((*date)[0].doc_id, 2);
}

TEST(TFIDFIndexatorTests, AddEmptyDocument_AddsUrlButNoTokensIndexed) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    TFIDFIndexator idx(src, tokenizer);

    idx.addDocument("http://empty", "");

    ASSERT_EQ(src->urls.size(), 1u);
    EXPECT_EQ(src->urls[0], "http://empty");

    EXPECT_EQ(src->index.find("anytoken"), nullptr);
}

TEST(TFIDFIndexatorTests, PunctuationSplitsTokens) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    TFIDFIndexator idx(src, tokenizer);

    idx.addDocument("http://punc", "Hello, WORLD!");

    ASSERT_EQ(src->urls.size(), 1u);
    EXPECT_EQ(src->urls[0], "http://punc");

    auto *hello = src->index.find("hello");
    ASSERT_NE(hello, nullptr);
    EXPECT_EQ(hello->size(), 1u);
    EXPECT_EQ((*hello)[0].doc_id, 0);

    auto *world = src->index.find("world");
    ASSERT_NE(world, nullptr);
    EXPECT_EQ(world->size(), 1u);
    EXPECT_EQ((*world)[0].doc_id, 0);

    EXPECT_EQ(src->index.find("world!"), nullptr);
}

TEST(TFIDFIndexatorTests, HyphenApostropheAndNumbersTokenizedCorrectly) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    TFIDFIndexator idx(src, tokenizer);

    idx.addDocument("http://mix", "well-known don't 3.14 1,234");

    ASSERT_EQ(src->urls.size(), 1u);

    auto *hyphen = src->index.find("well-known");
    ASSERT_NE(hyphen, nullptr);
    EXPECT_EQ(hyphen->size(), 1u);
    EXPECT_EQ((*hyphen)[0].doc_id, 0);

    auto *apost = src->index.find("don't");
    ASSERT_NE(apost, nullptr);
    EXPECT_EQ(apost->size(), 1u);
    EXPECT_EQ((*apost)[0].doc_id, 0);

    auto *num1 = src->index.find("3.14");
    ASSERT_NE(num1, nullptr);
    EXPECT_EQ(num1->size(), 1u);
    EXPECT_EQ((*num1)[0].doc_id, 0);

    auto *num2 = src->index.find("1,234");
    ASSERT_NE(num2, nullptr);
    EXPECT_EQ(num2->size(), 1u);
    EXPECT_EQ((*num2)[0].doc_id, 0);
}

TEST(TFIDFIndexatorTests, UsesCustomStemmerWhenProvided) {
    struct TestStemmer : public IStemmer {
        std::string stem(std::string &word) override { return word + "_stem"; }
    };

    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>(std::make_unique<TestStemmer>());

    TFIDFIndexator idx(src, tokenizer);

    idx.addDocument("http://stem", "Alpha Beta");

    ASSERT_EQ(src->urls.size(), 1u);

    auto *a = src->index.find("alpha_stem");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->size(), 1u);
    EXPECT_EQ((*a)[0].doc_id, 0);

    auto *b = src->index.find("beta_stem");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->size(), 1u);
    EXPECT_EQ((*b)[0].doc_id, 0);
}

TEST(TFIDFIndexatorTests, DuplicateUrlBecomesSeparateDocument) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    TFIDFIndexator idx(src, tokenizer);

    idx.addDocument("http://dup", "apple");
    idx.addDocument("http://dup", "banana");

    ASSERT_EQ(src->urls.size(), 2u);
    EXPECT_EQ(src->urls[0], "http://dup");
    EXPECT_EQ(src->urls[1], "http://dup");

    auto *apple = src->index.find("apple");
    ASSERT_NE(apple, nullptr);
    EXPECT_EQ(apple->size(), 1u);
    EXPECT_EQ((*apple)[0].doc_id, 0);

    auto *banana = src->index.find("banana");
    ASSERT_NE(banana, nullptr);
    EXPECT_EQ(banana->size(), 1u);
    EXPECT_EQ((*banana)[0].doc_id, 1);
}

TEST(TFIDFIndexatorTests, PostingsAreAppendedInIncreasingOrder) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    TFIDFIndexator idx(src, tokenizer);

    idx.addDocument("http://a", "shared uniqueA");
    idx.addDocument("http://b", "other shared");
    idx.addDocument("http://c", "shared uniqueC");

    auto *shared = src->index.find("shared");
    ASSERT_NE(shared, nullptr);
    // Ensure postings contain the documents in increasing doc_id order
    std::vector<int> expected{0, 1, 2};
    std::vector<int> actual;
    for (const auto &p : *shared) actual.push_back(p.doc_id);
    EXPECT_EQ(actual, expected);
}