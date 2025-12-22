#pragma once

#include <vector>

namespace BinaryFormat {
struct Header {
    uint32_t magic;
    uint32_t version;
    uint32_t num_docs;
    uint32_t num_terms;
};

struct TermEntry {
    size_t term_hash;
    uint64_t term_offset;
    uint64_t data_offset;
    uint32_t doc_count;
};

const uint32_t MAGIC = 0xABC1234;
}  // namespace BinaryFormat

template <typename T>
struct HashNode {
    std::string key;
    T value;
    std::unique_ptr<HashNode> next;

    HashNode(const std::string& k) : key(k), next(nullptr), value{} {}
};

template <typename T>
class HashMap {
private:
    static const size_t BUCKET_COUNT = 100000;

    size_t getHash(const std::string& key) const { return std::hash<std::string>{}(key) % BUCKET_COUNT; }

public:
    std::vector<std::unique_ptr<HashNode<T>>> buckets;
    HashMap() { buckets.resize(BUCKET_COUNT); }

    T& get(const std::string& key) {
        size_t h = getHash(key);

        HashNode<T>* curr = buckets[h].get();
        while (curr) {
            if (curr->key == key) {
                return curr->value;
            }
            curr = curr->next.get();
        }

        auto newNode = std::make_unique<HashNode<T>>(key);
        newNode->next = std::move(buckets[h]);
        buckets[h] = std::move(newNode);

        return buckets[h]->value;
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

    T* find(const std::string& key) {
        size_t h = getHash(key);
        HashNode<T>* curr = buckets[h].get();
        while (curr) {
            if (curr->key == key) {
                return &curr->value;
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
                callback(curr->key, curr->value);
                curr = curr->next.get();
            }
        }
    }
};

struct TermInfo {
    uint32_t doc_id;
    uint32_t tf;
};

class IIndexSource {
public:
    virtual ~IIndexSource() = default;

    virtual std::vector<TermInfo> getPostings(const std::string& term) = 0;

    virtual std::string getUrl(int doc_id) const = 0;

    virtual uint32_t getTotalDocs() const = 0;
};

class RamIndexSource : public IIndexSource {
public:
    std::vector<std::string> urls;
    HashMap<std::vector<TermInfo>> index;

    std::vector<TermInfo> getPostings(const std::string& term) override {
        auto* node = index.getNode(term);
        return node ? node->value : std::vector<TermInfo>{};
    }

    void addUrl(std::string_view url);
    void addDocument(const std::string& token, uint32_t doc_id);

    std::string getUrl(int doc_id) const override {
        if (doc_id >= 0 && doc_id < (int)urls.size()) return urls[doc_id];
        return "";
    }

    uint32_t getTotalDocs() const override { return (int)urls.size(); }
    void dump(const std::string& file, bool zip);
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
    uint32_t getTotalDocs() const override { return (int)urls.size(); }
};