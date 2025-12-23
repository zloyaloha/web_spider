#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>

#include "indexator.h"
#include "searcher.h"

static bool vec_eq(const std::vector<std::pair<std::string, double>>& a, const std::vector<std::pair<std::string, double>>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i] != b[i]) return false;
    return true;
}

TEST(SearcherTests, SingleTermReturnsMatchingUrlsInOrder) {
    auto src = std::make_shared<RamIndexSource>();
    auto tokenizer = std::make_shared<Tokenizer>();

    BooleanIndexator idx(src, tokenizer);
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

    BooleanIndexator idx(src, tokenizer);
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

    BooleanIndexator idx(src, tokenizer);
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

    BooleanIndexator idx(src, tokenizer);
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

    BooleanIndexator idx(src, tokenizer);
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

    BooleanIndexator idx(src, tokenizer);
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

    BooleanIndexator idx(src, tokenizer);
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

    BooleanIndexator idx(src, tokenizer);
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

    indexator.addDocument("url1", "The quick quick brown fox");
    indexator.addDocument("url2", "Jumps over the lazy dog");

    source->dump("test.idx", true);
    MappedIndexSource mapped("test.idx");

    BinarySearcher searcher(std::make_shared<MappedIndexSource>(std::move(mapped)), tok);
    auto results = searcher.findDocument("quick");

    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results[0].first, "url1");
}

TEST(Integration, RemovedSingleOccurrenceTerm_Zipped) {
    auto source = std::make_shared<RamIndexSource>();
    auto tok = std::make_shared<Tokenizer>(std::make_unique<PorterStemmer>());
    TFIDFIndexator indexator(source, tok);

    indexator.addDocument("url1", "unique");
    indexator.addDocument("url2", "other");

    // In-memory search should find the term before dumping
    BinarySearcher inmem_searcher(source, tok);
    auto inmem_res = inmem_searcher.findDocument("unique");
    ASSERT_EQ(inmem_res.size(), 1);
    EXPECT_EQ(inmem_res[0].first, "url1");

    // After dump/mapping the term should be removed (single occurrence, tf==1)
    const char* fname = "test_single.idx";
    source->dump(fname, true);
    auto mapped_ptr = std::make_shared<MappedIndexSource>(std::string(fname));
    BinarySearcher mapped_searcher(mapped_ptr, tok);
    auto mapped_res = mapped_searcher.findDocument("unique");
    EXPECT_TRUE(mapped_res.empty());

    std::remove(fname);
}

TEST(Integration, RepeatedTermInSingleDocIsPreserved_NonZipped) {
    auto source = std::make_shared<RamIndexSource>();
    auto tok = std::make_shared<Tokenizer>(std::make_unique<PorterStemmer>());
    TFIDFIndexator indexator(source, tok);

    indexator.addDocument("url1", "repeat repeat");
    indexator.addDocument("url2", "other");

    const char* fname = "test_repeat.idx";
    source->dump(fname, false);  // non-zipped
    auto mapped_ptr = std::make_shared<MappedIndexSource>(std::string(fname));
    BinarySearcher mapped_searcher(mapped_ptr, tok);
    auto res = mapped_searcher.findDocument("repeat");

    ASSERT_EQ(res.size(), 1);
    EXPECT_EQ(res[0].first, "url1");

    std::remove(fname);
}

TEST(Integration, TermInMultipleDocsIsPreserved_Zipped) {
    auto source = std::make_shared<RamIndexSource>();
    auto tok = std::make_shared<Tokenizer>(std::make_unique<PorterStemmer>());
    TFIDFIndexator indexator(source, tok);

    indexator.addDocument("a", "common");
    indexator.addDocument("b", "common");

    const char* fname = "test_common.idx";
    source->dump(fname, true);
    auto mapped_ptr = std::make_shared<MappedIndexSource>(std::string(fname));
    BinarySearcher mapped_searcher(mapped_ptr, tok);

    auto res = mapped_searcher.findDocument("common");
    ASSERT_EQ(res.size(), 2);
    auto names = std::vector<std::string>{res[0].first, res[1].first};
    EXPECT_TRUE(std::find(names.begin(), names.end(), "a") != names.end());
    EXPECT_TRUE(std::find(names.begin(), names.end(), "b") != names.end());

    std::remove(fname);
}

TEST(Integration, RemovedTermInNotClause_Zipped) {
    auto source = std::make_shared<RamIndexSource>();
    auto tok = std::make_shared<Tokenizer>(std::make_unique<PorterStemmer>());
    TFIDFIndexator indexator(source, tok);

    indexator.addDocument("d1", "apple apple");
    indexator.addDocument("d2", "unique");

    const char* fname = "test_removed_not.idx";
    source->dump(fname, true);
    auto mapped_ptr = std::make_shared<MappedIndexSource>(std::string(fname));
    BinarySearcher mapped_searcher(mapped_ptr, tok);

    auto res = mapped_searcher.findDocument("apple !unique");
    ASSERT_EQ(res.size(), 1);
    EXPECT_EQ(res[0].first, "d1");

    std::remove(fname);
}

TEST(Integration, TFIDFRankingPreservedAfterDump_Zipped) {
    auto source = std::make_shared<RamIndexSource>();
    auto tok = std::make_shared<Tokenizer>(std::make_unique<PorterStemmer>());
    TFIDFIndexator indexator(source, tok);

    indexator.addDocument("doc1", "apple apple apple");
    indexator.addDocument("doc2", "apple");
    indexator.addDocument("doc3", "apple apple");

    const char* fname = "test_tfidf.idx";
    source->dump(fname, true);
    auto mapped_ptr = std::make_shared<MappedIndexSource>(std::string(fname));
    TFIDFSearcher s(mapped_ptr, tok);

    auto res = s.findDocument("apple");
    ASSERT_GE(res.size(), 3u);
    EXPECT_GT(res[0].second, res[1].second);
    EXPECT_GT(res[1].second, res[2].second);

    std::remove(fname);
}

TEST(Integration, ComplexQueryWithRemovedAndPreservedTerms) {
    auto source = std::make_shared<RamIndexSource>();
    auto tok = std::make_shared<Tokenizer>(std::make_unique<PorterStemmer>());
    TFIDFIndexator indexator(source, tok);

    indexator.addDocument("x1", "preserved");
    indexator.addDocument("x2", "removed");
    indexator.addDocument("x3", "preserved other");

    const char* fname = "test_complex.idx";
    source->dump(fname, true);
    auto mapped_ptr = std::make_shared<MappedIndexSource>(std::string(fname));
    BinarySearcher mapped_searcher(mapped_ptr, tok);

    auto res = mapped_searcher.findDocument("preserved | removed");
    ASSERT_EQ(res.size(), 2);
    auto ids = std::vector<std::string>{res[0].first, res[1].first};
    EXPECT_TRUE(std::find(ids.begin(), ids.end(), "x1") != ids.end());
    EXPECT_TRUE(std::find(ids.begin(), ids.end(), "x3") != ids.end());

    std::remove(fname);
}

TEST(Integration, MultiTermAndWithRemovedTerm) {
    auto source = std::make_shared<RamIndexSource>();
    auto tok = std::make_shared<Tokenizer>(std::make_unique<PorterStemmer>());
    TFIDFIndexator indexator(source, tok);

    indexator.addDocument("m1", "a b");
    indexator.addDocument("m2", "a");
    indexator.addDocument("m3", "c");

    const char* fname = "test_multi_and.idx";
    source->dump(fname, true);
    auto mapped_ptr = std::make_shared<MappedIndexSource>(std::string(fname));
    BinarySearcher mapped_searcher(mapped_ptr, tok);

    auto res = mapped_searcher.findDocument("a & c");
    EXPECT_TRUE(res.empty());

    std::remove(fname);
}

TEST(Integration, LargeIndex_ManyDocsAndWords_Zipped) {
    auto source = std::make_shared<RamIndexSource>();
    auto tok = std::make_shared<Tokenizer>(std::make_unique<PorterStemmer>());
    TFIDFIndexator indexator(source, tok);

    const int ND = 100;  // number of documents
    const int NW = 100;  // words per document

    for (int i = 0; i < ND; ++i) {
        std::string doc;
        doc.reserve(NW * 8);
        for (int j = 0; j < NW; ++j) {
            doc += "w" + std::to_string(i) + "_" + std::to_string(j) + " ";
        }
        for (int k = 0; k < 5; ++k) doc += "common ";
        doc += "group" + std::string(1, 'A' + (i % 10)) + " ";

        indexator.addDocument("doc" + std::to_string(i), doc);
    }

    const char* fname = "test_large.idx";
    source->dump(fname, true);
    auto mapped_ptr = std::make_shared<MappedIndexSource>(std::string(fname));
    BinarySearcher s(mapped_ptr, tok);

    auto res_common = s.findDocument("common");
    EXPECT_EQ(res_common.size(), ND);

    auto res_group5 = s.findDocument("group" + std::string(1, 'A' + 5));
    EXPECT_EQ(res_group5.size(), ND / 10);

    auto union_res = s.findDocument("group" + std::string(1, 'A' + 1) + " | " + "group" + std::string(1, 'A' + 2));
    EXPECT_GE(union_res.size(), (size_t)(ND / 10 * 2));

    std::remove(fname);
}

TEST(Integration, LargeIndex_ManyDocsAndWords_NonZipped) {
    auto source = std::make_shared<RamIndexSource>();
    auto tok = std::make_shared<Tokenizer>(std::make_unique<PorterStemmer>());
    TFIDFIndexator indexator(source, tok);

    const int ND = 100;  // number of documents
    const int NW = 100;  // words per document

    for (int i = 0; i < ND; ++i) {
        std::string doc;
        doc.reserve(NW * 8);
        for (int j = 0; j < NW; ++j) {
            doc += "w" + std::to_string(i) + "_" + std::to_string(j) + " ";
        }
        for (int k = 0; k < 3; ++k) {
            doc += "shared ";
        }
        doc += "group" + std::string(1, 'A' + (i % 5)) + " ";

        indexator.addDocument("d" + std::to_string(i), doc);
    }

    const char* fname = "test_large_nz.idx";
    source->dump(fname, false);
    auto mapped_ptr = std::make_shared<MappedIndexSource>(std::string(fname));
    BinarySearcher s(mapped_ptr, tok);

    auto res_shared = s.findDocument("shared");
    EXPECT_EQ(res_shared.size(), ND);

    auto res_group3 = s.findDocument("group" + std::string(1, 'A' + 3));
    EXPECT_EQ(res_group3.size(), ND / 5);

    std::remove(fname);
}