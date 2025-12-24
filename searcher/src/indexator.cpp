#include "indexator.h"

TFIDFIndexator::TFIDFIndexator(std::shared_ptr<RamIndexSource> src, std::shared_ptr<Tokenizer> tok)
    : IIndexator(src, std::move(tok)) {}

void IIndexator::addDocument(const std::string_view& url_view, const std::string_view& doc_view) {
    uint32_t doc_id = source->getTotalDocs();

    source->addUrl(url_view);

    tokenizer->tokenize(doc_view);
    const std::vector<std::string>& tokens = tokenizer->getTokens();

    processTokens(tokens, doc_id);
}

void TFIDFIndexator::processTokens(const std::vector<std ::string>& tokens, int doc_id) {
    HashMap<std::string, uint32_t> local_counts;

    for (const std::string& token : tokens) {
        local_counts.get(token)++;
    }

    local_counts.traverse([&](const std::string& term, const uint32_t& tf_val) {
        if (tf_val > 0) {
            source->addDocument(term, doc_id, tf_val);
        }
    });
}

void BooleanIndexator::processTokens(const std::vector<std ::string>& tokens, int doc_id) {
    for (const std::string& token : tokens) {
        source->addDocument(token, doc_id);
    }
}

IIndexator::IIndexator(std::shared_ptr<RamIndexSource> src, std::shared_ptr<Tokenizer> tok)
    : source(src), tokenizer(std::move(tok)) {}

BooleanIndexator::BooleanIndexator(std::shared_ptr<RamIndexSource> src, std::shared_ptr<Tokenizer> tok)
    : IIndexator(src, std::move(tok)) {}
