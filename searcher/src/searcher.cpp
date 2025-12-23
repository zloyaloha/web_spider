#include "searcher.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <span>
#include <string>

#include "index.h"
#include "tokenizer.h"

std::vector<TermInfo> intersect_lists(std::span<const TermInfo> l1, std::span<const TermInfo> l2) {
    std::vector<TermInfo> res;
    res.reserve(std::min(l1.size(), l2.size()));
    auto i1 = l1.begin(), i2 = l2.begin();

    while (i1 != l1.end() && i2 != l2.end()) {
        if (i1->doc_id < i2->doc_id)
            i1++;
        else if (i2->doc_id < i1->doc_id)
            i2++;
        else {
            res.push_back(*i1);
            i1++;
            i2++;
        }
    }
    return res;
}

std::vector<TermInfo> union_lists(std::span<const TermInfo> l1, std::span<const TermInfo> l2) {
    std::vector<TermInfo> res;
    res.reserve(l1.size() + l2.size());
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

        if (i1->doc_id < i2->doc_id)
            res.push_back(*i1++);
        else if (i2->doc_id < i1->doc_id)
            res.push_back(*i2++);
        else {
            res.push_back(*i1);
            i1++;
            i2++;
        }
    }
    return res;
}

std::vector<TermInfo> not_list(std::span<const TermInfo> l, int total_docs) {
    std::vector<TermInfo> res;
    res.reserve(std::max(0, total_docs - (int)l.size()));
    int current_doc = 0;
    auto it = l.begin();

    while (current_doc < total_docs) {
        if (it != l.end() && it->doc_id == (uint32_t)current_doc) {
            it++;
        } else {
            res.push_back({(uint32_t)current_doc, 0});
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

std::vector<std::pair<std::string, double>> ISearcher::findDocument(const std::string& query) {
    auto tokens = parseQuery(query);

    std::vector<std::string> queryTerms;
    for (const auto& token : tokens) {
        if (!isOperator(token)) {
            queryTerms.push_back(token);
        }
    }

    std::cout << "Tokens searching: [";
    for (const auto& token : queryTerms) {
        std::cout << token << ", ";
    }
    std::cout << "]\n";

    auto rpn = sortingStation(tokens);

    auto terms_info = evaluate(rpn, source->getTotalDocs());

    if (terms_info.empty()) return {};

    return processResults(terms_info, queryTerms);
}

std::vector<TermInfo> ISearcher::evaluate(const std::vector<std::string>& rpn, int total_docs) {
    std::vector<std::vector<TermInfo>> stack;
    for (const auto& token : rpn) {
        if (!isOperator(token)) {
            stack.push_back(source->getPostings(token));
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
    return stack.empty() ? std::vector<TermInfo>{} : stack.back();
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

BinarySearcher::BinarySearcher(std::shared_ptr<IIndexSource> src, std::shared_ptr<Tokenizer> tok) : ISearcher(src, tok) {}

std::vector<std::pair<std::string, double>> BinarySearcher::processResults(const std::vector<TermInfo>& terms_info,
                                                                           const std::vector<std::string>& terms) {
    std::vector<std::pair<std::string, double>> result_urls;
    result_urls.reserve(terms_info.size());

    for (const auto& term_info : terms_info) {
        std::string url = source->getUrl(term_info.doc_id);
        if (!url.empty()) {
            result_urls.push_back({url, 0.});
        }
    }
    return result_urls;
}

TFIDFSearcher::TFIDFSearcher(std::shared_ptr<IIndexSource> src, std::shared_ptr<Tokenizer> tok) : ISearcher(src, tok) {}

std::vector<std::pair<int, double>> TFIDFSearcher::rankResults(const std::vector<TermInfo>& terms_info,
                                                               const std::vector<std::string>& terms) {
    int N = source->getTotalDocs();
    HashMap<uint32_t, double> scores;

    std::vector<bool> isRelevant(N, false);
    for (const auto& term_info : terms_info) isRelevant[term_info.doc_id] = true;

    for (const auto& term : terms) {
        auto postings = source->getPostings(term);

        double idf = std::log((double)N / (1 + postings.size()));

        for (const auto& entry : postings) {
            if (isRelevant[entry.doc_id]) {
                scores.get(entry.doc_id) += (1.0 + std::log((double)entry.tf)) * idf;
            }
        }
    }

    std::vector<std::pair<int, double>> ranked;
    for (const auto& term_info : terms_info) {
        ranked.push_back({term_info.doc_id, scores.get(term_info.doc_id)});
    }

    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    return ranked;
}

std::vector<std::pair<std::string, double>> TFIDFSearcher::processResults(const std::vector<TermInfo>& terms_info,
                                                                          const std::vector<std::string>& terms) {
    auto ranked = rankResults(terms_info, terms);

    std::vector<std::pair<std::string, double>> result_urls;
    for (const auto& pair : ranked) {
        result_urls.push_back({source->getUrl(pair.first), pair.second});
    }
    return result_urls;
}