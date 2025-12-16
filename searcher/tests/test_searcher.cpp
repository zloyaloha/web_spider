#include <gtest/gtest.h>

#include <memory>

#include "searcher.h"

static bool vec_eq(const std::vector<std::string>& a, const std::vector<std::string>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i] != b[i]) return false;
    return true;
}

TEST(SearcherTests, SingleTermReturnsMatchingUrlsInOrder) {
    SimpleHashMap<int> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>();

    Indexator idx(index, urls, tokenizer);
    idx.addDocument("http://a", "apple banana");
    idx.addDocument("http://b", "banana cherry");
    idx.addDocument("http://c", "apple cherry date");

    Searcher s(index, urls, tokenizer);
    auto res = s.findDocument("apple");

    std::vector<std::string> expected = {"http://a", "http://c"};
    EXPECT_TRUE(vec_eq(res, expected));
}

TEST(SearcherTests, ImplicitANDBetweenTerms) {
    SimpleHashMap<int> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>();

    Indexator idx(index, urls, tokenizer);
    idx.addDocument("http://a", "apple banana");
    idx.addDocument("http://b", "banana cherry");
    idx.addDocument("http://c", "apple cherry date");

    Searcher s(index, urls, tokenizer);
    auto res = s.findDocument("apple cherry");

    std::vector<std::string> expected = {"http://c"};
    EXPECT_TRUE(vec_eq(res, expected));
}

TEST(SearcherTests, OROperatorProducesUnion) {
    SimpleHashMap<int> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>();

    Indexator idx(index, urls, tokenizer);
    idx.addDocument("http://a", "apple banana");
    idx.addDocument("http://b", "banana cherry");
    idx.addDocument("http://c", "apple cherry date");

    Searcher s(index, urls, tokenizer);
    auto res = s.findDocument("apple | banana");

    std::vector<std::string> expected = {"http://a", "http://b", "http://c"};
    EXPECT_TRUE(vec_eq(res, expected));
}

TEST(SearcherTests, NOTOperatorNegatesSet) {
    SimpleHashMap<int> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>();

    Indexator idx(index, urls, tokenizer);
    idx.addDocument("http://a", "apple banana");
    idx.addDocument("http://b", "banana cherry");
    idx.addDocument("http://c", "apple cherry date");

    Searcher s(index, urls, tokenizer);
    auto res = s.findDocument("!banana");

    std::vector<std::string> expected = {"http://c"};
    EXPECT_TRUE(vec_eq(res, expected));
}

TEST(SearcherTests, ParenthesesAndPrecedenceHandling) {
    SimpleHashMap<int> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>();

    Indexator idx(index, urls, tokenizer);
    idx.addDocument("http://a", "a b");
    idx.addDocument("http://b", "b c");
    idx.addDocument("http://c", "a c");

    Searcher s(index, urls, tokenizer);

    auto res1 = s.findDocument("a | b & c");
    std::vector<std::string> expected1 = {"http://a", "http://b", "http://c"};
    EXPECT_TRUE(vec_eq(res1, expected1));

    auto res2 = s.findDocument("(a | b) & c");
    EXPECT_EQ(res2.size(), 2u);
    EXPECT_TRUE((res2[0] == "http://b" && res2[1] == "http://c") || (res2[0] == "http://c" && res2[1] == "http://b"));
}

TEST(SearcherTests, ComplexImplicitAndWithNot) {
    SimpleHashMap<int> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>();

    Indexator idx(index, urls, tokenizer);
    idx.addDocument("http://a", "apple");
    idx.addDocument("http://b", "apple banana");
    idx.addDocument("http://c", "banana");

    Searcher s(index, urls, tokenizer);

    auto res = s.findDocument("apple !banana");
    std::vector<std::string> expected = {"http://a"};
    EXPECT_TRUE(vec_eq(res, expected));
}

TEST(SearcherTests, NonexistentTokenReturnsEmpty) {
    SimpleHashMap<int> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>();

    Indexator idx(index, urls, tokenizer);
    idx.addDocument("http://a", "apple");

    Searcher s(index, urls, tokenizer);
    auto res = s.findDocument("nonexistent");
    EXPECT_TRUE(res.empty());
}

TEST(TFIDFSearcherTests, SingleTermReturnsMatchingUrlsInOrder) {
    SimpleHashMap<PostingEntry> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>();

    TFIDFIndexator idx(index, urls, tokenizer);
    idx.addDocument("http://a", "apple banana");
    idx.addDocument("http://b", "banana cherry");
    idx.addDocument("http://c", "apple cherry date");

    TFIDFSearcher s(index, urls, tokenizer);
    auto res = s.findDocument("apple");

    std::vector<std::string> expected = {"http://a", "http://c"};
    EXPECT_TRUE(vec_eq(res, expected));
}

TEST(TFIDFSearcherTests, ImplicitANDBetweenTerms) {
    SimpleHashMap<PostingEntry> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>();

    TFIDFIndexator idx(index, urls, tokenizer);
    idx.addDocument("http://a", "apple banana");
    idx.addDocument("http://b", "banana cherry");
    idx.addDocument("http://c", "apple cherry date");

    TFIDFSearcher s(index, urls, tokenizer);
    auto res = s.findDocument("apple cherry");

    std::vector<std::string> expected = {"http://c"};
    EXPECT_TRUE(vec_eq(res, expected));
}

TEST(TFIDFSearcherTests, OROperatorProducesUnion) {
    SimpleHashMap<PostingEntry> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>();

    TFIDFIndexator idx(index, urls, tokenizer);
    idx.addDocument("http://a", "apple banana");
    idx.addDocument("http://b", "banana cherry");
    idx.addDocument("http://c", "apple cherry date");

    TFIDFSearcher s(index, urls, tokenizer);
    auto res = s.findDocument("apple | banana");

    std::vector<std::string> expected = {"http://a", "http://b", "http://c"};
    EXPECT_TRUE(vec_eq(res, expected));
}

TEST(TFIDFSearcherTests, NOTOperatorNegatesSet) {
    SimpleHashMap<PostingEntry> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>();

    TFIDFIndexator idx(index, urls, tokenizer);
    idx.addDocument("http://a", "apple banana");
    idx.addDocument("http://b", "banana cherry");
    idx.addDocument("http://c", "apple cherry date");

    TFIDFSearcher s(index, urls, tokenizer);
    auto res = s.findDocument("!banana");

    std::vector<std::string> expected = {"http://c"};
    EXPECT_TRUE(vec_eq(res, expected));
}

TEST(TFIDFSearcherTests, ParenthesesAndPrecedenceHandling) {
    SimpleHashMap<PostingEntry> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>();

    TFIDFIndexator idx(index, urls, tokenizer);
    idx.addDocument("http://a", "a b");
    idx.addDocument("http://b", "b c");
    idx.addDocument("http://c", "a c");

    TFIDFSearcher s(index, urls, tokenizer);

    auto res1 = s.findDocument("a | b & c");
    std::vector<std::string> expected1 = {"http://a", "http://b", "http://c"};
    EXPECT_TRUE(vec_eq(res1, expected1));

    auto res2 = s.findDocument("(a | b) & c");
    EXPECT_EQ(res2.size(), 2u);
    EXPECT_TRUE((res2[0] == "http://b" && res2[1] == "http://c") || (res2[0] == "http://c" && res2[1] == "http://b"));
}

TEST(TFIDFSearcherTests, ComplexImplicitAndWithNot) {
    SimpleHashMap<int> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>();

    Indexator idx(index, urls, tokenizer);
    idx.addDocument("http://a", "apple");
    idx.addDocument("http://b", "apple banana");
    idx.addDocument("http://c", "banana");

    Searcher s(index, urls, tokenizer);

    auto res = s.findDocument("apple !banana");
    std::vector<std::string> expected = {"http://a"};
    EXPECT_TRUE(vec_eq(res, expected));
}

TEST(TFIDFSearcherTests, NonexistentTokenReturnsEmpty) {
    SimpleHashMap<PostingEntry> index;
    std::vector<std::string> urls;
    auto tokenizer = std::make_shared<Tokenizer>();

    TFIDFIndexator idx(index, urls, tokenizer);
    idx.addDocument("http://a", "apple");

    TFIDFSearcher s(index, urls, tokenizer);
    auto res = s.findDocument("nonexistent");
    EXPECT_TRUE(res.empty());
}
