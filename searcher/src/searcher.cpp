#include "searcher.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include "tokenizer.h"

std::vector<int> intersect_lists(const std::vector<int>& l1, const std::vector<int>& l2) {
    std::vector<int> res;
    auto i1 = l1.begin(), i2 = l2.begin();
    while (i1 != l1.end() && i2 != l2.end()) {
        if (*i1 < *i2)
            i1++;
        else if (*i2 < *i1)
            i2++;
        else {
            res.push_back(*i1);
            i1++;
            i2++;
        }
    }
    return res;
}

std::vector<int> union_lists(const std::vector<int>& l1, const std::vector<int>& l2) {
    std::vector<int> res;
    auto i1 = l1.begin(), i2 = l2.begin();
    while (i1 != l1.end() || i2 != l2.end()) {
        if (i1 == l1.end()) {
            res.push_back(*i2++);
            continue;
        }
        if (i2 == l2.end()) {
            res.push_back(*i1++);
            continue;
        }

        if (*i1 < *i2)
            res.push_back(*i1++);
        else if (*i2 < *i1)
            res.push_back(*i2++);
        else {
            res.push_back(*i1);
            i1++;
            i2++;
        }
    }
    return res;
}

std::vector<int> not_list(const std::vector<int>& l, int total_docs) {
    std::vector<int> res;
    int current_doc = 0;
    auto it = l.begin();

    while (current_doc < total_docs) {
        if (it != l.end() && *it == current_doc) {
            it++;
        } else {
            res.push_back(current_doc);
        }
        current_doc++;
    }
    return res;
}

int ISearcher::getPriority(const std::string& op) {
    if (op == "!") return 3;
    if (op == "&") return 2;
    if (op == "|") return 1;
    return 0;
}

bool ISearcher::isOperator(const std::string& token) {
    return token == "!" || token == "&" || token == "|" || token == "(" || token == ")";
}

std::vector<std::string> ISearcher::findDocument(const std::string& query) {
    auto tokens = parseQuery(query);

    std::vector<std::string> queryTerms;
    for (const auto& token : tokens) {
        if (!isOperator(token)) {
            queryTerms.push_back(token);
        }
    }

    auto rpn = sortingStation(tokens);

    auto docIds = evaluate(rpn, source->getTotalDocs());

    if (docIds.empty()) return {};

    return processResults(docIds, queryTerms);
}

std::vector<int> ISearcher::evaluate(const std::vector<std::string>& rpn, int total_docs) {
    std::vector<std::vector<int>> stack;
    for (const auto& token : rpn) {
        if (!isOperator(token)) {
            stack.push_back(fetchDocIds(token));
        } else {
            if (token == "!") {
                if (stack.empty()) continue;
                auto op1 = stack.back();
                stack.pop_back();
                stack.push_back(not_list(op1, total_docs));
            } else {
                if (stack.size() < 2) continue;
                auto right = stack.back();
                stack.pop_back();
                auto left = stack.back();
                stack.pop_back();
                if (token == "&")
                    stack.push_back(intersect_lists(left, right));
                else if (token == "|")
                    stack.push_back(union_lists(left, right));
            }
        }
    }
    return stack.empty() ? std::vector<int>{} : stack.back();
}

std::vector<std::string> ISearcher::parseQuery(const std::string& query) {
    std::vector<std::string> rawTokens = tokenizer->getRawTokens(query);

    std::vector<std::string> processedTokens;

    auto addTokenWithImplicitAnd = [&](const std::string& newToken) {
        if (!processedTokens.empty()) {
            const std::string& last = processedTokens.back();
            if ((!isOperator(last) || last == ")") && (!isOperator(newToken) || newToken == "(" || newToken == "!")) {
                processedTokens.push_back("&");
            }
        }
        processedTokens.push_back(newToken);
    };

    for (const auto& t : rawTokens) {
        if (isOperator(t)) {
            addTokenWithImplicitAnd(t);
        } else {
            tokenizer->tokenize(t);
            auto subtokens = tokenizer->getTokens();
            for (const std::string& sub : subtokens) {
                addTokenWithImplicitAnd(sub);
            }
        }
    }
    return processedTokens;
}

ISearcher::ISearcher(std::shared_ptr<IIndexSource> src, std::shared_ptr<Tokenizer> tok)
    : source(std::move(src)), tokenizer(std::move(tok)) {}

std::vector<std::string> ISearcher::sortingStation(const std::vector<std::string>& tokens) {
    std::vector<std::string> outputQueue;
    std::vector<std::string> operatorStack;

    for (const auto& token : tokens) {
        if (!isOperator(token)) {
            outputQueue.push_back(token);
        } else if (token == "(") {
            operatorStack.push_back("(");
        } else if (token == ")") {
            while (!operatorStack.empty() && operatorStack.back() != "(") {
                outputQueue.push_back(operatorStack.back());
                operatorStack.pop_back();
            }
            if (!operatorStack.empty()) operatorStack.pop_back();
        } else {
            while (!operatorStack.empty() && operatorStack.back() != "(" &&
                   getPriority(operatorStack.back()) >= getPriority(token)) {
                outputQueue.push_back(operatorStack.back());
                operatorStack.pop_back();
            }
            operatorStack.push_back(token);
        }
    }

    while (!operatorStack.empty()) {
        outputQueue.push_back(operatorStack.back());
        operatorStack.pop_back();
    }
    return outputQueue;
}

std::vector<int> ISearcher::fetchDocIds(const std::string& term) {
    std::vector<TermInfo> postings = source->getPostings(term);

    std::vector<int> ids;
    ids.reserve(postings.size());

    for (const auto& entry : postings) {
        ids.push_back(entry.doc_id);
    }
    return ids;
}

BinarySearcher::BinarySearcher(std::shared_ptr<IIndexSource> src, std::shared_ptr<Tokenizer> tok) : ISearcher(src, tok) {}

std::vector<std::string> BinarySearcher::processResults(const std::vector<int>& docIds, const std::vector<std::string>& terms) {
    std::vector<std::string> result_urls;
    result_urls.reserve(docIds.size());

    for (int id : docIds) {
        std::string url = source->getUrl(id);
        if (!url.empty()) {
            result_urls.push_back(url);
        }
    }
    return result_urls;
}

TFIDFSearcher::TFIDFSearcher(std::shared_ptr<IIndexSource> src, std::shared_ptr<Tokenizer> tok) : ISearcher(src, tok) {}

std::vector<std::pair<int, double>> TFIDFSearcher::rankResults(const std::vector<int>& doc_ids,
                                                               const std::vector<std::string>& terms) {
    int N = source->getTotalDocs();
    std::vector<double> scores(N, 0.0);

    std::vector<bool> isRelevant(N, false);
    for (int id : doc_ids) isRelevant[id] = true;

    for (const auto& term : terms) {
        auto postings = source->getPostings(term);
        if (postings.empty()) continue;

        double idf = std::log((double)N / (1 + postings.size()));

        for (const auto& entry : postings) {
            if (entry.doc_id < N && isRelevant[entry.doc_id]) {
                scores[entry.doc_id] += (double)entry.tf * idf;
            }
        }
    }

    std::vector<std::pair<int, double>> ranked;
    for (int id : doc_ids) {
        ranked.push_back({id, scores[id]});
    }

    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    return ranked;
}

IIndexator::IIndexator(std::shared_ptr<RamIndexSource> src, std::shared_ptr<Tokenizer> tok)
    : source(src), tokenizer(std::move(tok)) {}

BinaryIndexator::BinaryIndexator(std::shared_ptr<RamIndexSource> src, std::shared_ptr<Tokenizer> tok)
    : IIndexator(src, std::move(tok)) {}

void BinaryIndexator::addDocument(const std::string_view& url_view, const std::string_view& doc_view) {
    auto ramSource = std::static_pointer_cast<RamIndexSource>(source);

    uint32_t doc_id = ramSource->getTotalDocs();

    ramSource->urls.emplace_back(url_view);

    tokenizer->tokenize(doc_view);
    std::vector<std::string> tokens = tokenizer->getTokens();

    for (const std::string& token : tokens) {
        std::vector<TermInfo>& postings = ramSource->index.get(token);

        if (postings.empty() || postings.back().doc_id != doc_id) {
            postings.push_back({doc_id, 1});
        }
    }
}

TFIDFIndexator::TFIDFIndexator(std::shared_ptr<RamIndexSource> src, std::shared_ptr<Tokenizer> tok)
    : IIndexator(src, std::move(tok)) {}

void TFIDFIndexator::addDocument(const std::string_view& url_view, const std::string_view& doc_view) {
    int doc_id = source->getTotalDocs();
    source->urls.emplace_back(url_view);

    tokenizer->tokenize(doc_view);
    std::vector<std::string> tokens = tokenizer->getTokens();

    std::unordered_map<std::string, int> local_tf;
    for (const auto& token : tokens) {
        local_tf[token]++;
    }

    for (const auto& [term, tf] : local_tf) {
        std::vector<TermInfo>& postings = source->index.get(term);

        TermInfo entry;
        entry.doc_id = doc_id;
        entry.tf = tf;

        postings.push_back(entry);
    }
}

std::vector<std::string> TFIDFSearcher::processResults(const std::vector<int>& docIds, const std::vector<std::string>& terms) {
    auto ranked = rankResults(docIds, terms);

    std::vector<std::string> result_urls;
    for (const auto& pair : ranked) {
        result_urls.push_back(source->getUrl(pair.first));
    }
    return result_urls;
}

void RamIndexSource::dump(const std::string& filename) {
    std::ofstream ofs(filename, std::ios::binary);
    if (!ofs) throw std::runtime_error("Cannot open file for writing");

    std::vector<std::pair<std::string, std::vector<TermInfo>>> sorted_index;
    index.traverse([&](const std::string& term, const std::vector<TermInfo>& docs) { sorted_index.push_back({term, docs}); });

    std::sort(sorted_index.begin(), sorted_index.end(),
              [](const auto& a, const auto& b) { return std::hash<std::string>{}(a.first) < std::hash<std::string>{}(b.first); });

    BinaryFormat::Header header;
    header.magic = BinaryFormat::MAGIC;
    header.version = 2;
    header.num_docs = (uint32_t)urls.size();
    header.num_terms = (uint32_t)sorted_index.size();

    // храним все всё в следующем формате
    // [header] [[card_1][card_2]..[card_n]] [[token_1][token_2]..[token_n]] [[p_list_1][p_list_2]..[p_list_3]]
    // p_list_n == [[doc_id, token_freq]...]
    // card_n хранит в себе hash токена, offset на токен, offset на posting_list и размер
    // posting_list сможем делать бинарный поиск

    ofs.write(reinterpret_cast<char*>(&header), sizeof(header));

    for (const auto& url : urls) {
        uint32_t len = (uint32_t)url.size();
        ofs.write(reinterpret_cast<char*>(&len), sizeof(len));
        ofs.write(url.data(), len);
    }

    uint64_t dir_start = ofs.tellp();
    uint64_t dir_size = sorted_index.size() * sizeof(BinaryFormat::TermEntry);
    uint64_t term_pool_offset = dir_start + dir_size;
    uint64_t current_term_offset = term_pool_offset;

    uint64_t term_pool_size = 0;
    for (const auto& p : sorted_index) {
        term_pool_size += p.first.size() + 1;
    }

    uint64_t data_pool_offset = term_pool_offset + term_pool_size;
    uint64_t current_data_offset = data_pool_offset;

    for (const auto& [term, docs] : sorted_index) {
        BinaryFormat::TermEntry entry;
        entry.term_hash = std::hash<std::string>{}(term);
        entry.term_offset = current_term_offset;
        entry.data_offset = current_data_offset;
        entry.doc_count = (uint32_t)docs.size();

        ofs.write(reinterpret_cast<char*>(&entry), sizeof(entry));

        current_term_offset += term.size() + 1;
        current_data_offset += docs.size() * sizeof(int) * 2;
    }

    for (const auto& [term, docs] : sorted_index) {
        ofs.write(term.c_str(), term.size() + 1);
    }

    for (const auto& [term, docs] : sorted_index) {
        for (const auto posting_entry : docs) {
            ofs.write(reinterpret_cast<const char*>(&posting_entry.doc_id), sizeof(uint32_t));
            ofs.write(reinterpret_cast<const char*>(&posting_entry.tf), sizeof(uint32_t));
        }
    }

    ofs.close();
}

// MappedIndexSource implementation
MappedIndexSource::~MappedIndexSource() {
    if (map_addr && map_addr != MAP_FAILED) {
        munmap((void*)map_addr, file_size);
        map_addr = nullptr;
    }
    if (fd != -1) {
        close(fd);
        fd = -1;
    }
    term_directory = nullptr;
    num_terms = 0;
}

void MappedIndexSource::load(const std::string& filename, int expected_version) {
    fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1) throw std::runtime_error("Cannot open index file");

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        close(fd);
        fd = -1;
        throw std::runtime_error("Cannot stat file");
    }
    file_size = sb.st_size;

    map_addr = (const char*)mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map_addr == MAP_FAILED) {
        close(fd);
        fd = -1;
        map_addr = nullptr;
        throw std::runtime_error("mmap failed");
    }

    auto* header = reinterpret_cast<const BinaryFormat::Header*>(map_addr);
    if (header->magic != BinaryFormat::MAGIC) throw std::runtime_error("Invalid magic");
    if (header->version != expected_version) throw std::runtime_error("Invalid version");

    file_version = header->version;
    num_terms = header->num_terms;

    const char* ptr = map_addr + sizeof(BinaryFormat::Header);

    urls.reserve(header->num_docs);
    for (uint32_t i = 0; i < header->num_docs; ++i) {
        uint32_t len = *reinterpret_cast<const uint32_t*>(ptr);
        ptr += sizeof(uint32_t);
        urls.emplace_back(ptr, len);
        ptr += len;
    }

    term_directory = reinterpret_cast<const BinaryFormat::TermEntry*>(ptr);
}

const BinaryFormat::TermEntry* MappedIndexSource::findTermEntry(const std::string& term) const {
    if (!term_directory || num_terms == 0 || !map_addr) return nullptr;

    size_t h = std::hash<std::string>{}(term);

    auto it = std::lower_bound(term_directory, term_directory + num_terms, h,
                               [](const BinaryFormat::TermEntry& entry, size_t val) { return entry.term_hash < val; });

    while (it != term_directory + num_terms && it->term_hash == h) {
        const char* stored_term = map_addr + it->term_offset;
        if (std::string(stored_term) == term) {
            return it;
        }
        it++;
    }
    return nullptr;
}

std::vector<TermInfo> MappedIndexSource::getPostings(const std::string& term) {
    const auto* entry = findTermEntry(term);
    if (!entry) return {};

    std::vector<TermInfo> results;
    results.reserve(entry->doc_count);

    size_t data_block_size = 0;
    ptrdiff_t idx = entry - term_directory;
    if ((size_t)(idx + 1) < num_terms) {
        data_block_size = (size_t)((entry + 1)->data_offset - entry->data_offset);
    } else {
        data_block_size = file_size - entry->data_offset;
    }

    if (file_version == 1) {
        const int* raw_data = reinterpret_cast<const int*>(map_addr + entry->data_offset);
        size_t expected_pairs_size = entry->doc_count * sizeof(int) * 2;
        if (data_block_size >= expected_pairs_size) {
            for (uint32_t i = 0; i < entry->doc_count; ++i) {
                int doc_id = raw_data[i * 2];
                results.push_back(TermInfo{static_cast<uint32_t>(doc_id), 1});
            }
        } else {
            for (uint32_t i = 0; i < entry->doc_count; ++i) {
                TermInfo t{static_cast<uint32_t>(raw_data[i]), 1};
                results.push_back(t);
            }
        }
    } else {
        const char* data_ptr = map_addr + entry->data_offset;
        for (uint32_t i = 0; i < entry->doc_count; ++i) {
            int doc_id = *reinterpret_cast<const int*>(data_ptr);
            int tf = *reinterpret_cast<const int*>(data_ptr + sizeof(int));
            results.push_back(TermInfo{static_cast<uint32_t>(doc_id), static_cast<uint32_t>(tf)});
            data_ptr += sizeof(int) * 2;
        }
    }
    return results;
}

std::string MappedIndexSource::getUrl(int doc_id) const {
    if (doc_id >= 0 && doc_id < (int)urls.size()) return urls[doc_id];
    return "";
}