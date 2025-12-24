#pragma once

#include "index.h"
#include "tokenizer.h"

class IIndexator {
protected:
    std::shared_ptr<Tokenizer> tokenizer;
    std::shared_ptr<RamIndexSource> source;

public:
    IIndexator(std::shared_ptr<RamIndexSource> src, std::shared_ptr<Tokenizer> tok);
    virtual void addDocument(const std::string_view& url_view, const std::string_view& doc_view);
    virtual void processTokens(const std::vector<std ::string>& tokens, int doc_id) = 0;
};

class BooleanIndexator : public IIndexator {
public:
    BooleanIndexator(std::shared_ptr<RamIndexSource> src, std::shared_ptr<Tokenizer> tok);
    void processTokens(const std::vector<std ::string>& tokens, int doc_id) override;
};

class TFIDFIndexator : public IIndexator {
public:
    TFIDFIndexator(std::shared_ptr<RamIndexSource> src, std::shared_ptr<Tokenizer> tok);
    void processTokens(const std::vector<std ::string>& tokens, int doc_id) override;
};
