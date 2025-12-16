#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "tokenizer.h"

struct PostingEntry {
    int doc_id;
    int term_frequency;
    std::vector<int> positions;
};

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
    static const size_t BUCKET_COUNT = 10000;

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

class IIndexator {
protected:
    std::shared_ptr<Tokenizer> tokenizer;
    std::vector<std::string>& urls;

public:
    IIndexator(std::vector<std::string>& urls, std::shared_ptr<Tokenizer> tokenizer);
    virtual void addDocument(const std::string_view& url_view, const std::string_view& doc_view) = 0;
};

class Indexator : public IIndexator {
private:
    SimpleHashMap<int>& index;

public:
    Indexator(SimpleHashMap<int>& index, std::vector<std::string>& urls, std::shared_ptr<Tokenizer> tokenizer);
    void addDocument(const std::string_view& url_view, const std::string_view& doc_view) override;
};

class TFIDFIndexator : public IIndexator {
private:
    SimpleHashMap<PostingEntry>& index;

public:
    TFIDFIndexator(SimpleHashMap<PostingEntry>& index, std::vector<std::string>& urls, std::shared_ptr<Tokenizer> tokenizer);
    void addDocument(const std::string_view& url_view, const std::string_view& doc_view) override;
};

std::vector<int> intersect_lists(const std::vector<int>& l1, const std::vector<int>& l2);
std::vector<int> union_lists(const std::vector<int>& l1, const std::vector<int>& l2);
std::vector<int> not_list(const std::vector<int>& l, int total_docs);

class ISearcher {
protected:
    std::shared_ptr<Tokenizer> tokenizer;
    std::vector<std::string>& urls;

public:
    ISearcher(std::vector<std::string>& urls, std::shared_ptr<Tokenizer> tokenizer);
    virtual std::vector<std::string> findDocument(const std::string& query);

protected:
    int getPriority(const std::string& op);
    bool isOperator(const std::string& token);
    std::vector<int> evaluate(const std::vector<std::string>& tokens, int total_docs);

    virtual std::vector<int> fetchDocIds(const std::string& term) = 0;
    virtual std::vector<std::string> processResults(const std::vector<int>& docIds, const std::vector<std::string>& terms) = 0;

    std::vector<std::string> parseQuery(const std::string& query);
    std::vector<std::string> sortingStation(const std::vector<std::string>& tokens);
};

class Searcher : public ISearcher {
private:
    SimpleHashMap<int>& index;

public:
    Searcher(SimpleHashMap<int>& index, std::vector<std::string>& urls, std::shared_ptr<Tokenizer> tokenizer);

private:
    std::vector<int> fetchDocIds(const std::string& term) override;
    std::vector<std::string> processResults(const std::vector<int>& docIds, const std::vector<std::string>& terms) override;
};

class TFIDFSearcher : public ISearcher {
private:
    SimpleHashMap<PostingEntry>& index;

public:
    TFIDFSearcher(SimpleHashMap<PostingEntry>& index, std::vector<std::string>& urls, std::shared_ptr<Tokenizer> tokenizer);
    std::vector<int> evaluate(const std::vector<std::string>& tokens, int total_docs);

private:
    std::vector<int> fetchDocIds(const std::string& term) override;
    std::vector<std::string> processResults(const std::vector<int>& docIds, const std::vector<std::string>& terms) override;
    std::vector<std::pair<int, double>> rankResults(const std::vector<int>& doc_ids, const std::vector<std::string>& terms);
};