#include "searcher.h"

#include <string_view>

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

int Searcher::getPriority(const std::string& op) {
    if (op == "!") return 3;
    if (op == "&") return 2;
    if (op == "|") return 1;
    return 0;
}

bool Searcher::isOperator(const std::string& token) {
    return token == "!" || token == "&" || token == "|" || token == "(" || token == ")";
}

Searcher::Searcher(SimpleHashMap& index, std::vector<std::string>& urls, std::shared_ptr<Tokenizer> tokenizer)
    : index(index), urls(urls), tokenizer(std::move(tokenizer)) {}

std::vector<std::string> Searcher::sortingStation(const std::vector<std::string>& tokens) {
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

std::vector<std::string> Searcher::findDocument(const std::string& query) {
    std::vector<std::string> rawTokens;
    std::string currentToken;
    for (size_t i = 0; i < query.length(); ++i) {
        char c = query[i];
        bool is_op_char = (c == '(' || c == ')' || c == '!' || c == '&' || c == '|');
        if (c == ' ' || is_op_char) {
            if (!currentToken.empty()) {
                rawTokens.push_back(currentToken);
                currentToken.clear();
            }
            if (is_op_char) {
                rawTokens.push_back(std::string(1, c));
            }
        } else {
            currentToken += c;
        }
    }
    if (!currentToken.empty()) rawTokens.push_back(currentToken);

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

    auto polish_notation = sortingStation(processedTokens);
    auto result_ids = evaluate(polish_notation, (int)urls.size());

    std::vector<std::string> result_urls;
    result_urls.reserve(result_ids.size());
    for (int doc_id : result_ids) {
        if (doc_id >= 0 && (size_t)doc_id < urls.size()) {
            result_urls.push_back(urls[doc_id]);
        }
    }

    return result_urls;
}

std::vector<int> Searcher::evaluate(const std::vector<std::string>& tokens, int total_docs) {
    std::vector<std::vector<int>> evaluationStack;

    for (const auto& token : tokens) {
        if (!isOperator(token)) {
            auto* postings = index.find(token);
            if (postings) {
                evaluationStack.push_back(*postings);
            } else {
                evaluationStack.push_back({});
            }
        } else {
            if (token == "!") {
                if (evaluationStack.empty()) continue;
                auto op1 = evaluationStack.back();
                evaluationStack.pop_back();
                evaluationStack.push_back(not_list(op1, total_docs));
            } else {
                if (evaluationStack.size() < 2) continue;
                auto right = evaluationStack.back();
                evaluationStack.pop_back();
                auto left = evaluationStack.back();
                evaluationStack.pop_back();

                if (token == "&") {
                    evaluationStack.push_back(intersect_lists(left, right));
                } else if (token == "|") {
                    evaluationStack.push_back(union_lists(left, right));
                }
            }
        }
    }

    return evaluationStack.empty() ? std::vector<int>{} : evaluationStack.back();
}

Indexator::Indexator(SimpleHashMap& index, std::vector<std::string>& urls, std::shared_ptr<Tokenizer> tokenizer)
    : index(index), urls(urls), tokenizer(std::move(tokenizer)) {}

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
