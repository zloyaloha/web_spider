#include <fcntl.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "indexator.h"
#include "searcher.h"

static std::string create_temp_file() {
    std::string tmpl = "/tmp/web_spider_index_XXXXXX";
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    int fd = mkstemp(buf.data());
    if (fd == -1) throw std::runtime_error("mkstemp failed");
    close(fd);
    return std::string(buf.data());
}

static void set_file_version(const std::string &path, uint32_t version) {
    int fd = open(path.c_str(), O_RDWR);
    ASSERT_NE(fd, -1);
    off_t off = lseek(fd, sizeof(uint32_t), SEEK_SET);
    ASSERT_NE(off, -1);
    ssize_t written = write(fd, &version, sizeof(version));
    ASSERT_EQ(written, (ssize_t)sizeof(version));
    close(fd);
}

static bool vec_eq(const std::vector<std::pair<std::string, double>> &a, const std::vector<std::pair<std::string, double>> &b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].first != b[i].first) return false;
    return true;
}

TEST(MappedIndexSourceTests, LoadAndReadV2) {
    auto src = std::make_shared<RamIndexSource>();
    auto tok = std::make_shared<Tokenizer>();
    TFIDFIndexator idx(src, tok);

    idx.addDocument("http://a", "apple banana apple");
    idx.addDocument("http://b", "banana apple");
    idx.addDocument("http://c", "cherry");

    std::string path = create_temp_file();
    src->dump(path, 1);

    MappedIndexSource mapped(path);
    EXPECT_EQ(mapped.getTotalDocs(), 3);
    EXPECT_EQ(mapped.getUrl(0), "http://a");
    EXPECT_EQ(mapped.getUrl(1), "http://b");
    EXPECT_EQ(mapped.getUrl(2), "http://c");

    auto postings = mapped.getPostings("apple");
    ASSERT_EQ(postings.size(), 2u);

    bool found0 = false, found1 = false;
    for (const auto &p : postings) {
        if (p.doc_id == 0) {
            found0 = true;
            EXPECT_EQ(p.tf, 2u);
        }
        if (p.doc_id == 1) {
            found1 = true;
            EXPECT_EQ(p.tf, 1u);
        }
    }
    EXPECT_TRUE(found0 && found1);

    // also make sure searcher using mapped source works
    auto mapped_ptr = std::make_shared<MappedIndexSource>(path);
    TFIDFSearcher s(mapped_ptr, tok);
    auto res = s.findDocument("apple");
    std::vector<std::pair<std::string, double>> expected = {{"http://a", 0.}, {"http://b", 0}};
    EXPECT_TRUE(vec_eq(res, expected));

    unlink(path.c_str());
}

TEST(MappedIndexSourceTests, NonexistentTermReturnsEmpty) {
    auto src = std::make_shared<RamIndexSource>();
    auto tok = std::make_shared<Tokenizer>();
    BooleanIndexator idx(src, tok);

    idx.addDocument("http://a", "apple");

    std::string path = create_temp_file();
    src->dump(path, 1);

    MappedIndexSource mapped(path);
    auto p = mapped.getPostings("nope");
    EXPECT_TRUE(p.empty());

    unlink(path.c_str());
}

TEST(MappedIndexSourceTests, LoadAndReadV1) {
    auto src = std::make_shared<RamIndexSource>();
    auto tok = std::make_shared<Tokenizer>();
    TFIDFIndexator idx(src, tok);

    idx.addDocument("http://a", "apple banana apple");
    idx.addDocument("http://b", "banana apple");
    idx.addDocument("http://c", "cherry");

    std::string path = create_temp_file();
    src->dump(path, 0);  // version 1

    MappedIndexSource mapped(path);
    EXPECT_EQ(mapped.getTotalDocs(), 3);
    EXPECT_EQ(mapped.getUrl(0), "http://a");
    EXPECT_EQ(mapped.getUrl(1), "http://b");
    EXPECT_EQ(mapped.getUrl(2), "http://c");

    auto postings = mapped.getPostings("apple");
    ASSERT_EQ(postings.size(), 2u);

    bool found0 = false, found1 = false;
    for (const auto &p : postings) {
        if (p.doc_id == 0) {
            found0 = true;
            EXPECT_EQ(p.tf, 2u);
        }
        if (p.doc_id == 1) {
            found1 = true;
            EXPECT_EQ(p.tf, 1u);
        }
    }
    EXPECT_TRUE(found0 && found1);

    unlink(path.c_str());
}

TEST(MappedIndexSourceTests, DumpEmptyIndex) {
    auto src = std::make_shared<RamIndexSource>();

    std::string path = create_temp_file();
    src->dump(path, 0);

    MappedIndexSource mapped(path);
    EXPECT_EQ(mapped.getTotalDocs(), 0u);
    EXPECT_EQ(mapped.getUrl(0), "");
    EXPECT_TRUE(mapped.getPostings("any").empty());

    unlink(path.c_str());
}

TEST(MappedIndexSourceTests, DumpToDirectoryThrows) {
    auto src = std::make_shared<RamIndexSource>();
    auto tok = std::make_shared<Tokenizer>();
    BooleanIndexator idx(src, tok);

    idx.addDocument("http://a", "apple");

    char tmpl[] = "/tmp/web_spider_dir_XXXXXX";
    char *dir = mkdtemp(tmpl);
    ASSERT_NE(dir, nullptr);

    std::string dirpath(dir);

    EXPECT_THROW(src->dump(dirpath, 0), std::runtime_error);

    rmdir(dirpath.c_str());
}
