#include "searcher.h"

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

    auto docIds = evaluate(rpn, (int)urls.size());

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

ISearcher::ISearcher(std::vector<std::string>& urls, std::shared_ptr<Tokenizer> tokenizer) : urls(urls), tokenizer(tokenizer) {}

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

Searcher::Searcher(SimpleHashMap<int>& index, std::vector<std::string>& urls, std::shared_ptr<Tokenizer> tokenizer)
    : ISearcher(urls, tokenizer), index(index) {}

std::vector<int> Searcher::fetchDocIds(const std::string& term) {
    auto* vec = index.find(term);
    if (vec) return *vec;
    return {};
}

std::vector<std::string> Searcher::processResults(const std::vector<int>& docIds, const std::vector<std::string>& terms) {
    std::vector<std::string> result_urls;
    for (int id : docIds) {
        if (id >= 0 && id < urls.size()) {
            result_urls.push_back(urls[id]);
        }
    }
    return result_urls;
}

TFIDFSearcher::TFIDFSearcher(SimpleHashMap<PostingEntry>& index, std::vector<std::string>& urls,
                             std::shared_ptr<Tokenizer> tokenizer)
    : index(index), ISearcher(urls, tokenizer) {}

std::vector<int> TFIDFSearcher::fetchDocIds(const std::string& term) {
    auto* node = index.getNode(term);
    if (!node) return {};

    std::vector<int> ids;
    for (const auto& entry : node->postings) {
        ids.push_back(entry.doc_id);
    }
    return ids;
}

std::vector<std::pair<int, double>> TFIDFSearcher::rankResults(const std::vector<int>& doc_ids,
                                                               const std::vector<std::string>& terms) {
    int total_docs = (int)urls.size();

    std::vector<double> scores(total_docs, 0.0);

    std::vector<bool> is_relevant(total_docs, false);
    for (int id : doc_ids) {
        if (id >= 0 && id < total_docs) {
            is_relevant[id] = true;
        }
    }

    for (const std::string& term : terms) {
        HashNode<PostingEntry>* node = index.getNode(term);
        if (!node) continue;

        double idf = std::log((double)total_docs / (1 + node->document_frequency));

        for (const auto& entry : node->postings) {
            if (entry.doc_id >= 0 && entry.doc_id < total_docs && is_relevant[entry.doc_id]) {
                double tf = (double)entry.term_frequency;
                scores[entry.doc_id] += tf * idf;
            }
        }
    }

    std::vector<std::pair<int, double>> sorted_results;
    sorted_results.reserve(doc_ids.size());

    for (int id : doc_ids) {
        if (id >= 0 && id < total_docs) {
            sorted_results.push_back({id, scores[id]});
        }
    }

    std::sort(sorted_results.begin(), sorted_results.end(),
              [](const std::pair<int, double>& a, const std::pair<int, double>& b) { return a.second > b.second; });

    return sorted_results;
}

IIndexator::IIndexator(std::vector<std::string>& urls, std::shared_ptr<Tokenizer> tokenizer)
    : urls(urls), tokenizer(std::move(tokenizer)) {}

Indexator::Indexator(SimpleHashMap<int>& index, std::vector<std::string>& urls, std::shared_ptr<Tokenizer> tokenizer)
    : index(index), IIndexator(urls, std::move(tokenizer)) {}

void Indexator::addDocument(const std::string_view& url_view, const std::string_view& doc_view) {
    int doc_id = (int)urls.size();
    urls.emplace_back(url_view);

    tokenizer->tokenize(doc_view);
    std::vector<std::string> tokens = tokenizer->getTokens();

    for (const std::string& token : tokens) {
        std::vector<int>& postings = index.get(token);

        if (postings.empty() || postings.back() != doc_id) {
            postings.push_back(doc_id);
        }
    }
}

TFIDFIndexator::TFIDFIndexator(SimpleHashMap<PostingEntry>& index, std::vector<std::string>& urls,
                               std::shared_ptr<Tokenizer> tokenizer)
    : index(index), IIndexator(urls, std::move(tokenizer)) {}

void TFIDFIndexator::addDocument(const std::string_view& url_view, const std::string_view& doc_view) {
    int doc_id = (int)urls.size();
    urls.emplace_back(url_view);

    tokenizer->tokenize(doc_view);
    std::vector<std::string> tokens = tokenizer->getTokens();

    SimpleHashMap<int> term_positions_map;
    for (int i = 0; i < tokens.size(); ++i) {
        term_positions_map.get(tokens[i]).push_back(i);
    }

    term_positions_map.traverse([&](const std::string& term, const std::vector<int>& positions) {
        HashNode<PostingEntry>* node = index.getNode(term);

        PostingEntry entry;
        entry.doc_id = doc_id;
        entry.term_frequency = (int)positions.size();
        entry.positions = positions;

        node->postings.push_back(entry);

        node->document_frequency++;
    });
}

std::vector<std::string> TFIDFSearcher::processResults(const std::vector<int>& docIds, const std::vector<std::string>& terms) {
    auto ranked = rankResults(docIds, terms);

    std::vector<std::string> result_urls;
    for (const auto& pair : ranked) {
        result_urls.push_back(urls[pair.first]);
    }
    return result_urls;
}
