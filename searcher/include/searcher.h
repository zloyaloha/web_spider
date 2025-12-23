#pragma once
#include <cstdlib>
#include <cstring>
#include <span>
#include <vector>

#include "index.h"
#include "tokenizer.h"

std::vector<TermInfo> intersect_lists(std::span<const TermInfo> l1, std::span<const TermInfo> l2);
std::vector<TermInfo> union_lists(std::span<const TermInfo> l1, std::span<const TermInfo> l2);
std::vector<TermInfo> not_list(std::span<const TermInfo> l, int total_docs);

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
    std::vector<TermInfo> evaluate(const std::vector<std::string>& tokens, int total_docs);

    virtual std::vector<std::pair<std::string, double>> processResults(const std::vector<TermInfo>& docIds,
                                                                       const std::vector<std::string>& terms) = 0;

    std::vector<std::string> parseQuery(const std::string& query);
    std::vector<std::string> sortingStation(const std::vector<std::string>& tokens);
};

class BinarySearcher : public ISearcher {
public:
    BinarySearcher(std::shared_ptr<IIndexSource> src, std::shared_ptr<Tokenizer> tok);

private:
    std::vector<std::pair<std::string, double>> processResults(const std::vector<TermInfo>& docIds,
                                                               const std::vector<std::string>& terms) override;
};

class TFIDFSearcher : public ISearcher {
public:
    TFIDFSearcher(std::shared_ptr<IIndexSource> src, std::shared_ptr<Tokenizer> tok);

private:
    std::vector<std::pair<int, double>> rankResults(const std::vector<TermInfo>& doc_ids, const std::vector<std::string>& terms);
    std::vector<std::pair<std::string, double>> processResults(const std::vector<TermInfo>& docIds,
                                                               const std::vector<std::string>& terms) override;
};