#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "tokenizer.h"

template <typename T>
struct HashNode {
    std::string key;
    std::vector<T> postings;
    int document_frequency;
    std::unique_ptr<HashNode> next;

    HashNode(const std::string& k) : key(k), document_frequency(0), next(nullptr) {}
};

template <typename T>
class SimpleHashMap {
private:
    static const size_t BUCKET_COUNT = 100000;

    size_t getHash(const std::string& key) const { return std::hash<std::string>{}(key) % BUCKET_COUNT; }

public:
    std::vector<std::unique_ptr<HashNode<T>>> buckets;
    SimpleHashMap() { buckets.resize(BUCKET_COUNT); }

    std::vector<T>& get(const std::string& key) {
        size_t h = getHash(key);

        HashNode<T>* curr = buckets[h].get();
        while (curr) {
            if (curr->key == key) {
                return curr->postings;
            }
            curr = curr->next.get();
        }

        auto newNode = std::make_unique<HashNode<T>>(key);
        newNode->next = std::move(buckets[h]);
        buckets[h] = std::move(newNode);

        return buckets[h]->postings;
    }

    HashNode<T>* getNode(const std::string& key) {
        size_t h = getHash(key);

        HashNode<T>* curr = buckets[h].get();
        while (curr) {
            if (curr->key == key) {
                return curr;
            }
            curr = curr->next.get();
        }

        auto newNode = std::make_unique<HashNode<T>>(key);
        HashNode<T>* rawPtr = newNode.get();

        newNode->next = std::move(buckets[h]);
        buckets[h] = std::move(newNode);

        return rawPtr;
    }

    std::vector<T>* find(const std::string& key) {
        size_t h = getHash(key);
        HashNode<T>* curr = buckets[h].get();
        while (curr) {
            if (curr->key == key) {
                return &curr->postings;
            }
            curr = curr->next.get();
        }
        return nullptr;
    }

    template <typename Func>
    void traverse(Func callback) {
        for (const auto& bucket : buckets) {
            HashNode<T>* curr = bucket.get();
            while (curr) {
                callback(curr->key, curr->postings);
                curr = curr->next.get();
            }
        }
    }
};

namespace BinaryFormat {
struct Header {
    uint32_t magic;
    uint32_t version;
    uint32_t num_docs;
    uint32_t num_terms;
};

struct TermEntry {
    size_t term_hash;      // Хеш строки для быстрого сравнения
    uint64_t term_offset;  // Смещение строки термина в файле
    uint64_t data_offset;  // Смещение списка doc_ids
    uint32_t doc_count;    // Количество документов
};

const uint32_t MAGIC = 0xABC1234;
}  // namespace BinaryFormat

struct TermInfo {
    uint32_t doc_id;
    uint32_t tf;
};

class IIndexSource {
public:
    virtual ~IIndexSource() = default;

    virtual std::vector<TermInfo> getPostings(const std::string& term) = 0;

    virtual std::string getUrl(int doc_id) const = 0;

    virtual int getTotalDocs() const = 0;
};

class RamIndexSource : public IIndexSource {
public:
    std::vector<std::string> urls;
    SimpleHashMap<TermInfo> index;

    std::vector<TermInfo> getPostings(const std::string& term) override {
        auto* node = index.getNode(term);
        return node ? node->postings : std::vector<TermInfo>{};
    }

    std::string getUrl(int doc_id) const override {
        if (doc_id >= 0 && doc_id < (int)urls.size()) return urls[doc_id];
        return "";
    }

    int getTotalDocs() const override { return (int)urls.size(); }
    void dump(const std::string& file);
};

class MappedIndexSource : public IIndexSource {
    int fd = -1;
    size_t file_size = 0;
    const char* map_addr = nullptr;

    const BinaryFormat::TermEntry* term_directory = nullptr;
    uint32_t num_terms = 0;
    std::vector<std::string> urls;
    uint32_t file_version = 0;

    const BinaryFormat::TermEntry* findTermEntry(const std::string& term) const;

public:
    MappedIndexSource(const std::string& filename, int expected_version) { load(filename, expected_version); }
    ~MappedIndexSource();

    void load(const std::string& filename, int expected_version);

    std::vector<TermInfo> getPostings(const std::string& term) override;
    std::string getUrl(int doc_id) const override;
    int getTotalDocs() const override { return (int)urls.size(); }
};

class IIndexator {
protected:
    std::shared_ptr<Tokenizer> tokenizer;
    std::shared_ptr<RamIndexSource> source;

public:
    IIndexator(std::shared_ptr<RamIndexSource> src, std::shared_ptr<Tokenizer> tok);
    virtual void addDocument(const std::string_view& url_view, const std::string_view& doc_view) = 0;
};

class BinaryIndexator : public IIndexator {
public:
    BinaryIndexator(std::shared_ptr<RamIndexSource> src, std::shared_ptr<Tokenizer> tok);
    void addDocument(const std::string_view& url_view, const std::string_view& doc_view) override;
};

class TFIDFIndexator : public IIndexator {
public:
    TFIDFIndexator(std::shared_ptr<RamIndexSource> src, std::shared_ptr<Tokenizer> tok);
    void addDocument(const std::string_view& url_view, const std::string_view& doc_view) override;
};

std::vector<int> intersect_lists(const std::vector<int>& l1, const std::vector<int>& l2);
std::vector<int> union_lists(const std::vector<int>& l1, const std::vector<int>& l2);
std::vector<int> not_list(const std::vector<int>& l, int total_docs);

class ISearcher {
protected:
    std::shared_ptr<Tokenizer> tokenizer;
    std::shared_ptr<IIndexSource> source;

public:
    ISearcher(std::shared_ptr<IIndexSource> src, std::shared_ptr<Tokenizer> tok);
    virtual ~ISearcher() = default;
    virtual std::vector<std::string> findDocument(const std::string& query);

protected:
    int getPriority(const std::string& op);
    bool isOperator(const std::string& token);
    std::vector<int> evaluate(const std::vector<std::string>& tokens, int total_docs);

    std::vector<int> fetchDocIds(const std::string& term);
    virtual std::vector<std::string> processResults(const std::vector<int>& docIds, const std::vector<std::string>& terms) = 0;

    std::vector<std::string> parseQuery(const std::string& query);
    std::vector<std::string> sortingStation(const std::vector<std::string>& tokens);
};

class BinarySearcher : public ISearcher {
public:
    BinarySearcher(std::shared_ptr<IIndexSource> src, std::shared_ptr<Tokenizer> tok);

private:
    std::vector<std::string> processResults(const std::vector<int>& docIds, const std::vector<std::string>& terms) override;
};

class TFIDFSearcher : public ISearcher {
public:
    TFIDFSearcher(std::shared_ptr<IIndexSource> src, std::shared_ptr<Tokenizer> tok);

private:
    std::vector<std::pair<int, double>> rankResults(const std::vector<int>& doc_ids, const std::vector<std::string>& terms);
    std::vector<std::string> processResults(const std::vector<int>& docIds, const std::vector<std::string>& terms) override;
};