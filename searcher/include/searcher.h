#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "tokenizer.h"

struct HashNode {
    std::string key;
    std::vector<int> value;
    std::unique_ptr<HashNode> next;

    HashNode(const std::string& k) : key(k), next(nullptr) {}
};

class SimpleHashMap {
private:
    static const size_t BUCKET_COUNT = 10000;
    std::vector<std::unique_ptr<HashNode>> buckets;

    size_t getHash(const std::string& key) const { return std::hash<std::string>{}(key) % BUCKET_COUNT; }

public:
    SimpleHashMap() { buckets.resize(BUCKET_COUNT); }

    std::vector<int>& get(const std::string& key) {
        size_t h = getHash(key);

        HashNode* curr = buckets[h].get();
        while (curr) {
            if (curr->key == key) {
                return curr->value;
            }
            curr = curr->next.get();
        }

        auto newNode = std::make_unique<HashNode>(key);
        newNode->next = std::move(buckets[h]);
        buckets[h] = std::move(newNode);

        return buckets[h]->value;
    }

    std::vector<int>* find(const std::string& key) {
        size_t h = getHash(key);
        HashNode* curr = buckets[h].get();
        while (curr) {
            if (curr->key == key) {
                return &curr->value;
            }
            curr = curr->next.get();
        }
        return nullptr;
    }
};

class Indexator {
private:
    std::shared_ptr<Tokenizer> tokenizer;
    SimpleHashMap& index;
    std::vector<std::string>& urls;

public:
    Indexator(SimpleHashMap& index, std::vector<std::string>& urls, std::shared_ptr<Tokenizer> tokenizer);
    void addDocument(const std::string_view& url_view, const std::string_view& doc_view);
};

std::vector<int> intersect_lists(const std::vector<int>& l1, const std::vector<int>& l2);
std::vector<int> union_lists(const std::vector<int>& l1, const std::vector<int>& l2);
std::vector<int> not_list(const std::vector<int>& l, int total_docs);

class Searcher {
private:
    std::shared_ptr<Tokenizer> tokenizer;
    SimpleHashMap& index;
    std::vector<std::string>& urls;

public:
    Searcher(SimpleHashMap& index, std::vector<std::string>& urls, std::shared_ptr<Tokenizer> tokenizer);
    std::vector<std::string> findDocument(const std::string& query);

private:
    int getPriority(const std::string& op);
    bool isOperator(const std::string& token);

    std::vector<std::string> sortingStation(const std::vector<std::string>& tokens);
    std::vector<int> evaluate(const std::vector<std::string>& tokens, int total_docs);
};