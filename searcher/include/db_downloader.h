#include <gumbo.h>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/options/find.hpp>
#include <mongocxx/uri.hpp>

struct Document {
    std::string url;
    std::string content;
};

class DocumentDownloader {
private:
    mongocxx::client client;
    uint64_t total_bytes{0};

public:
    DocumentDownloader(const std::string& uri);
    std::vector<Document> downloadDocuments(int max_documents = 1000000000);
    void extractText(GumboNode* node, std::string& buffer);
    void cleanText(std::string& text);
};