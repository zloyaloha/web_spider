#pragma once
#include <gumbo.h>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <memory>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/options/find.hpp>
#include <mongocxx/uri.hpp>

#include "indexator.h"

struct Document {
    std::string url;
    std::string content;
};

class DocumentDownloader {
private:
    mongocxx::client client;
    uint64_t total_bytes{0};
    std::shared_ptr<IIndexator> indexator;

public:
    DocumentDownloader(const std::string& uri, std::shared_ptr<IIndexator> indexator);
    void downloadDocuments(int max_documents = 1000000000);
    void downloadDocumentsWithonIndexation();
    void extractText(GumboNode* node, std::string& buffer);
    void cleanText(std::string& text);
};