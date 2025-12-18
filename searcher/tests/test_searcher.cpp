#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "searcher.h"

static bool vec_eq(const std::vector<std::string> &a, const std::vector<std::string> &b) {
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

    std::vector<std::string> expected = {"http://a", "http://c"};
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

    std::vector<std::string> expected = {"http://c"};
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

    std::vector<std::string> expected = {"http://a", "http://b", "http://c"};
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

    std::vector<std::string> expected = {"http://c"};
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
    std::vector<std::string> expected1 = {"http://a", "http://b", "http://c"};
    EXPECT_TRUE(vec_eq(res1, expected1));

    auto res2 = s.findDocument("(a | b) & c");
    EXPECT_EQ(res2.size(), 2u);
    EXPECT_TRUE((res2[0] == "http://b" && res2[1] == "http://c") || (res2[0] == "http://c" && res2[1] == "http://b"));
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
    std::vector<std::string> expected = {"http://a"};
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

    std::vector<std::string> expected = {"http://a", "http://c"};
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

    std::vector<std::string> expected = {"http://c"};
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

    std::vector<std::string> expected = {"http://a", "http://b", "http://c"};
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

    std::vector<std::string> expected = {"http://c"};
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
    std::vector<std::string> expected1 = {"http://a", "http://b", "http://c"};
    EXPECT_TRUE(vec_eq(res1, expected1));

    auto res2 = s.findDocument("(a | b) & c");
    EXPECT_EQ(res2.size(), 2u);
    EXPECT_TRUE((res2[0] == "http://b" && res2[1] == "http://c") || (res2[0] == "http://c" && res2[1] == "http://b"));
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
    std::vector<std::string> expected = {"http://a"};
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

// Temporary debug test - remove once parsing issues are fixed
class DebugSearcher : public BinarySearcher {
public:
    using BinarySearcher::BinarySearcher;
    using ISearcher::evaluate;
    using ISearcher::parseQuery;
    using ISearcher::sortingStation;
};

TEST(DebugTests, ParseAndRPN) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();
    BinaryIndexator idx(src, tokenizer);
    idx.addDocument("http://a", "apple banana");
    idx.addDocument("http://b", "banana cherry");
    idx.addDocument("http://c", "apple cherry date");

    DebugSearcher s(src, tokenizer);
    auto tokens = s.parseQuery("apple cherry");
    std::vector<std::string> tks;
    for (auto &t : tokens) tks.push_back(std::string(t));
    auto rpn = s.sortingStation(tokens);
    std::vector<std::string> rpn_s;
    for (auto &r : rpn) rpn_s.push_back(std::string(r));
    auto res = s.evaluate(rpn, src->getTotalDocs());

    std::cerr << "DEBUG tokens: ";
    for (auto &x : tks) std::cerr << x << ", ";
    std::cerr << "\n";
    std::cerr << "DEBUG rpn: ";
    for (auto &x : rpn_s) std::cerr << x << ", ";
    std::cerr << "\n";
    std::cerr << "DEBUG res ids: ";
    for (auto &id : res) std::cerr << id << ", ";
    std::cerr << "\n";
}
