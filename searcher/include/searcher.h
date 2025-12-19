#pragma once
#include <cstdlib>
#include <cstring>
#include <vector>

#include "index.h"
#include "tokenizer.h"

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
    virtual std::vector<std::pair<std::string, double>> findDocument(const std::string& query);

protected:
    int getPriority(const std::string& op);
    bool isOperator(const std::string& token);
    std::vector<int> evaluate(const std::vector<std::string>& tokens, int total_docs);

    std::vector<int> fetchDocIds(const std::string& term);
    virtual std::vector<std::pair<std::string, double>> processResults(const std::vector<int>& docIds,
                                                                       const std::vector<std::string>& terms) = 0;

    std::vector<std::string> parseQuery(const std::string& query);
    std::vector<std::string> sortingStation(const std::vector<std::string>& tokens);
};

class BinarySearcher : public ISearcher {
public:
    BinarySearcher(std::shared_ptr<IIndexSource> src, std::shared_ptr<Tokenizer> tok);

private:
    std::vector<std::pair<std::string, double>> processResults(const std::vector<int>& docIds,
                                                               const std::vector<std::string>& terms) override;
};

class TFIDFSearcher : public ISearcher {
public:
    TFIDFSearcher(std::shared_ptr<IIndexSource> src, std::shared_ptr<Tokenizer> tok);

private:
    std::vector<std::pair<int, double>> rankResults(const std::vector<int>& doc_ids, const std::vector<std::string>& terms);
    std::vector<std::pair<std::string, double>> processResults(const std::vector<int>& docIds,
                                                               const std::vector<std::string>& terms) override;
};