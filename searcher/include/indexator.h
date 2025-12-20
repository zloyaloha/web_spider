#pragma once

#include "index.h"
#include "tokenizer.h"

class IIndexator {
protected:
    std::shared_ptr<Tokenizer> tokenizer;
    std::shared_ptr<RamIndexSource> source;

public:
    IIndexator(std::shared_ptr<RamIndexSource> src, std::shared_ptr<Tokenizer> tok);
    virtual void addDocument(const std::string_view& url_view, const std::string_view& doc_view) = 0;
};

class BinaryIndexator : public IIndexator {
public:
    BinaryIndexator(std::shared_ptr<RamIndexSource> src, std::shared_ptr<Tokenizer> tok);
    void addDocument(const std::string_view& url_view, const std::string_view& doc_view) override;
};

class TFIDFIndexator : public IIndexator {
public:
    TFIDFIndexator(std::shared_ptr<RamIndexSource> src, std::shared_ptr<Tokenizer> tok);
    void addDocument(const std::string_view& url_view, const std::string_view& doc_view) override;
};
