#include "db_downloader.h"

#include <iostream>
#include <regex>

DocumentDownloader::DocumentDownloader(const std::string& uri) : client(mongocxx::uri{uri}) {}

using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;

std::vector<Document> DocumentDownloader::downloadDocuments(int max_documents) {
    std::vector<Document> documents;

    auto db = client["sports_corpus"];
    auto coll = db["documents"];

    auto projection = document{} << "normalized_url" << 1 << "html_content" << 1 << "_id" << 0 << finalize;

    int64_t count = coll.count_documents({});
    if (count > 0) {
        documents.reserve(static_cast<std::size_t>(count));
    }

    mongocxx::options::find opts;
    opts.projection(projection.view());

    auto cursor = coll.find({}, opts);
    int counter = 0;
    size_t total_bytes = 0;

    for (auto&& doc : cursor) {
        auto url_ele = doc["normalized_url"];
        auto content_ele = doc["html_content"];

        if (url_ele && content_ele && url_ele.type() == bsoncxx::type::k_string &&
            content_ele.type() == bsoncxx::type::k_string) {
            std::string url = std::string(url_ele.get_string().value);
            std::string html = std::string(content_ele.get_string().value);
            GumboOutput* output = gumbo_parse(html.c_str());

            std::string content;
            content.reserve(html.length() / 5);
            extractText(output->root, content);
            cleanText(content);

            documents.emplace_back(Document{std::move(url), std::move(content)});

            total_bytes += (html.length());
            std::clog << "\rDownloaded: " << counter++ << " docs (" << total_bytes / 1024 / 1024 << " MB)" << std::flush;
            gumbo_destroy_output(&kGumboDefaultOptions, output);
            if (counter == max_documents) {
                break;
            }
        }
    }
    std::clog << "\nFinished. Total docs: " << counter << std::endl;

    return documents;
}

void DocumentDownloader::extractText(GumboNode* node, std::string& buffer) {
    if (node->type == GUMBO_NODE_TEXT) {
        buffer.append(node->v.text.text);
        return;
    }

    if (node->type == GUMBO_NODE_ELEMENT) {
        GumboTag tag = node->v.element.tag;
        if (tag == GUMBO_TAG_SCRIPT || tag == GUMBO_TAG_STYLE || tag == GUMBO_TAG_NOSCRIPT || tag == GUMBO_TAG_IFRAME) {
            return;
        }

        GumboVector* children = &node->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i) {
            extractText(static_cast<GumboNode*>(children->data[i]), buffer);
        }

        if (tag == GUMBO_TAG_P || tag == GUMBO_TAG_DIV || tag == GUMBO_TAG_H1 || tag == GUMBO_TAG_H2 || tag == GUMBO_TAG_BR ||
            tag == GUMBO_TAG_LI) {
            buffer += ' ';
        }
    }
}

void DocumentDownloader::cleanText(std::string& text) {
    if (text.empty()) return;

    for (char& ch : text) {
        if (std::isspace(static_cast<unsigned char>(ch))) {
            ch = ' ';
        }
    }

    auto new_end = std::unique(text.begin(), text.end(), [](char lhs, char rhs) { return lhs == ' ' && rhs == ' '; });
    text.erase(new_end, text.end());

    if (!text.empty() && text.front() == ' ') text.erase(0, 1);
    if (!text.empty() && text.back() == ' ') text.pop_back();
}