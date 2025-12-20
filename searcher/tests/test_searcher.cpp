#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "indexator.h"
#include "searcher.h"

static bool vec_eq(const std::vector<std::pair<std::string, double>> &a, const std::vector<std::pair<std::string, double>> &b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i] != b[i]) return false;
    return true;
}

TEST(SearcherTests, SingleTermReturnsMatchingUrlsInOrder) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    BinaryIndexator idx(src, tokenizer);
    idx.addDocument("http://a", "apple banana");
    idx.addDocument("http://b", "banana cherry");
    idx.addDocument("http://c", "apple cherry date");

    BinarySearcher s(src, tokenizer);
    auto res = s.findDocument("apple");

    std::vector<std::pair<std::string, double>> expected = {{"http://a", 0.}, {"http://c", 0.}};
    EXPECT_TRUE(vec_eq(res, expected));
}

TEST(SearcherTests, ImplicitANDBetweenTerms) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    BinaryIndexator idx(src, tokenizer);
    idx.addDocument("http://a", "apple banana");
    idx.addDocument("http://b", "banana cherry");
    idx.addDocument("http://c", "apple cherry date");

    BinarySearcher s(src, tokenizer);
    auto res = s.findDocument("apple cherry");

    std::vector<std::pair<std::string, double>> expected = {{"http://c", 0.}};
    EXPECT_TRUE(vec_eq(res, expected));
}

TEST(SearcherTests, OROperatorProducesUnion) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    BinaryIndexator idx(src, tokenizer);
    idx.addDocument("http://a", "apple banana");
    idx.addDocument("http://b", "banana cherry");
    idx.addDocument("http://c", "apple cherry date");

    BinarySearcher s(src, tokenizer);
    auto res = s.findDocument("apple | banana");

    std::vector<std::pair<std::string, double>> expected = {{"http://a", 0.}, {"http://b", 0.}, {"http://c", 0}};
    EXPECT_TRUE(vec_eq(res, expected));
}

TEST(SearcherTests, NOTOperatorNegatesSet) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    BinaryIndexator idx(src, tokenizer);
    idx.addDocument("http://a", "apple banana");
    idx.addDocument("http://b", "banana cherry");
    idx.addDocument("http://c", "apple cherry date");

    BinarySearcher s(src, tokenizer);
    auto res = s.findDocument("!banana");

    std::vector<std::pair<std::string, double>> expected = {{"http://c", 0.}};
    EXPECT_TRUE(vec_eq(res, expected));
}

TEST(SearcherTests, ParenthesesAndPrecedenceHandling) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    BinaryIndexator idx(src, tokenizer);
    idx.addDocument("http://a", "a b");
    idx.addDocument("http://b", "b c");
    idx.addDocument("http://c", "a c");

    BinarySearcher s(src, tokenizer);

    auto res1 = s.findDocument("a | b & c");
    std::vector<std::pair<std::string, double>> expected1 = {{"http://a", 0.}, {"http://b", 0.}, {"http://c", 0.}};
    EXPECT_TRUE(vec_eq(res1, expected1));

    auto res2 = s.findDocument("(a | b) & c");
    EXPECT_EQ(res2.size(), 2u);
    EXPECT_TRUE((res2[0].first == "http://b" && res2[1].first == "http://c") ||
                (res2[0].first == "http://c" && res2[1].first == "http://b"));
}

TEST(SearcherTests, ComplexImplicitAndWithNot) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    BinaryIndexator idx(src, tokenizer);
    idx.addDocument("http://a", "apple");
    idx.addDocument("http://b", "apple banana");
    idx.addDocument("http://c", "banana");

    BinarySearcher s(src, tokenizer);

    auto res = s.findDocument("apple !banana");
    std::vector<std::pair<std::string, double>> expected = {{"http://a", 0.}};
    EXPECT_TRUE(vec_eq(res, expected));
}

TEST(SearcherTests, NonexistentTokenReturnsEmpty) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    BinaryIndexator idx(src, tokenizer);
    idx.addDocument("http://a", "apple");

    BinarySearcher s(src, tokenizer);
    auto res = s.findDocument("nonexistent");
    EXPECT_TRUE(res.empty());
}

TEST(TFIDFSearcherTests, SingleTermReturnsMatchingUrlsInOrder) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    TFIDFIndexator idx(src, tokenizer);
    idx.addDocument("http://a", "apple banana");
    idx.addDocument("http://b", "banana cherry");
    idx.addDocument("http://c", "apple cherry date");

    TFIDFSearcher s(src, tokenizer);
    auto res = s.findDocument("apple");

    std::vector<std::pair<std::string, double>> expected = {{"http://a", 0.}, {"http://c", 0.}};
    EXPECT_TRUE(vec_eq(res, expected));
}

TEST(TFIDFSearcherTests, ImplicitANDBetweenTerms) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    TFIDFIndexator idx(src, tokenizer);
    idx.addDocument("http://a", "apple banana");
    idx.addDocument("http://b", "banana cherry");
    idx.addDocument("http://c", "apple cherry date");

    TFIDFSearcher s(src, tokenizer);
    auto res = s.findDocument("apple cherry");

    std::vector<std::pair<std::string, double>> expected = {{"http://c", 0.}};
    EXPECT_TRUE(vec_eq(res, expected));
}

TEST(TFIDFSearcherTests, OROperatorProducesUnion) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    TFIDFIndexator idx(src, tokenizer);
    idx.addDocument("http://a", "apple banana");
    idx.addDocument("http://b", "banana cherry");
    idx.addDocument("http://c", "apple cherry date");

    TFIDFSearcher s(src, tokenizer);
    auto res = s.findDocument("apple | banana");

    std::vector<std::pair<std::string, double>> expected = {{"http://a", 0.}, {"http://b", 0.}, {"http://c", 0.}};
    EXPECT_TRUE(vec_eq(res, expected));
}

TEST(TFIDFSearcherTests, NOTOperatorNegatesSet) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    TFIDFIndexator idx(src, tokenizer);
    idx.addDocument("http://a", "apple banana");
    idx.addDocument("http://b", "banana cherry");
    idx.addDocument("http://c", "apple cherry date");

    TFIDFSearcher s(src, tokenizer);
    auto res = s.findDocument("!banana");

    std::vector<std::pair<std::string, double>> expected = {{"http://c", 0.}};
    EXPECT_TRUE(vec_eq(res, expected));
}

TEST(TFIDFSearcherTests, ParenthesesAndPrecedenceHandling) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    TFIDFIndexator idx(src, tokenizer);
    idx.addDocument("http://a", "a b");
    idx.addDocument("http://b", "b c");
    idx.addDocument("http://c", "a c");

    TFIDFSearcher s(src, tokenizer);

    auto res1 = s.findDocument("a | b & c");
    std::vector<std::pair<std::string, double>> expected1 = {{"http://a", 0.}, {"http://b", 0.}, {"http://c", 0}};
    EXPECT_TRUE(vec_eq(res1, expected1));

    auto res2 = s.findDocument("(a | b) & c");
    EXPECT_EQ(res2.size(), 2u);
    EXPECT_TRUE((res2[0].first == "http://b" && res2[1].first == "http://c") ||
                (res2[0].first == "http://c" && res2[1].first == "http://b"));
}

TEST(TFIDFSearcherTests, ComplexImplicitAndWithNot) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    BinaryIndexator idx(src, tokenizer);
    idx.addDocument("http://a", "apple");
    idx.addDocument("http://b", "apple banana");
    idx.addDocument("http://c", "banana");

    BinarySearcher s(src, tokenizer);

    auto res = s.findDocument("apple !banana");
    std::vector<std::pair<std::string, double>> expected = {{"http://a", 0.}};
    EXPECT_TRUE(vec_eq(res, expected));
}

TEST(TFIDFSearcherTests, NonexistentTokenReturnsEmpty) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    TFIDFIndexator idx(src, tokenizer);
    idx.addDocument("http://a", "apple");

    TFIDFSearcher s(src, tokenizer);
    auto res = s.findDocument("nonexistent");
    EXPECT_TRUE(res.empty());
}

TEST(TokenizerRawTest, GetRawTokensOperatorsAndParentheses) {
    auto tokenizer = std::make_unique<Tokenizer>();

    auto tokens = tokenizer->getRawTokens("!(apple|banana) & cherry");
    std::vector<std::string> expected = {"!", "(", "apple", "|", "banana", ")", "&", "cherry"};
    EXPECT_EQ(tokens, expected);

    auto tokens2 = tokenizer->getRawTokens("apple&banana");
    std::vector<std::string> expected2 = {"apple", "&", "banana"};
    EXPECT_EQ(tokens2, expected2);
}

TEST(TFIDFTests, TFIDFScoreOrdersByTermFrequency) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    TFIDFIndexator idx(src, tokenizer);
    idx.addDocument("http://a", "apple apple banana");
    idx.addDocument("http://b", "apple cherry");

    TFIDFSearcher s(src, tokenizer);
    auto res = s.findDocument("apple");

    ASSERT_EQ(res.size(), 2);

    EXPECT_TRUE(std::isfinite(res[0].second));
    EXPECT_TRUE(std::isfinite(res[1].second));
    EXPECT_GT(res[0].second, res[1].second);
}

TEST(Integration, FullFlow) {
    auto source = std::make_shared<RamIndexSource>();
    auto tok = std::make_shared<Tokenizer>(std::make_unique<PorterStemmer>());
    TFIDFIndexator indexator(source, tok);

    indexator.addDocument("url1", "The quick brown fox");
    indexator.addDocument("url2", "Jumps over the lazy dog");

    source->dump("test.idx", true);
    MappedIndexSource mapped("test.idx", 2);

    BinarySearcher searcher(std::make_shared<MappedIndexSource>(std::move(mapped)), tok);
    auto results = searcher.findDocument("quick fox");

    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results[0].first, "url1");
}