#include "db_downloader.h"

#include <cstdint>
#include <fstream>
#include <iostream>

#include "indexator.h"
#include "tokenizer.h"

DocumentDownloader::DocumentDownloader(const std::string& uri, std::shared_ptr<IIndexator> idxator)
    : client(mongocxx::uri{uri}), indexator(std::move(idxator)) {}

using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;

void DocumentDownloader::downloadDocuments(int max_documents) {
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
    uint64_t total_downloaded_bytes = 0;
    uint64_t total_content_bytes = 0;
    double total_indexate_time = 0.;

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

            auto start_time = std::chrono::high_resolution_clock::now();
            indexator->addDocument(url, content);
            auto end_time = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> duration = end_time - start_time;
            total_indexate_time += duration.count();

            total_downloaded_bytes += html.size();
            total_content_bytes += content.size();
            std::clog << "\rDownloaded: " << counter++ << " docs";
            gumbo_destroy_output(&kGumboDefaultOptions, output);
            if (counter == max_documents) {
                break;
            }
        }
    }
    double speed_kb_s = (total_content_bytes / 1024.0) / total_indexate_time;

    std::clog << "\nFinished. Total docs: " << counter << std::endl;
    std::clog << "Total indexate time: " << total_indexate_time << std::endl;
    std::clog << "Speed indexate: " << speed_kb_s << std::endl;
    std::cout << "Total downloaded bytes: " << total_downloaded_bytes / 1024.0 / 1024.0 << " MB\n";
    std::cout << "Total indexated bytes: " << total_content_bytes / 1024.0 / 1024.0 << " MB\n";
}

void DocumentDownloader::downloadDocumentsWithonIndexation() {
    std::vector<Document> documents;
    std::unordered_map<std::string, std::vector<std::string>> link2tokens;
    auto stemmer = std::make_unique<PorterStemmer>();
    Tokenizer tokenizer(std::move(stemmer));

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
    uint64_t total_content_bytes = 0;
    double total_tokenize_time = 0.;

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

            auto start_time = std::chrono::high_resolution_clock::now();
            tokenizer.tokenize(content);
            auto end_time = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> duration = end_time - start_time;
            total_tokenize_time += duration.count();
            total_content_bytes += content.size();
            link2tokens[url] = tokenizer.getTokens();
            std::clog << "\rDownloaded: " << counter++ << " docs";
            gumbo_destroy_output(&kGumboDefaultOptions, output);
        }
    }
    double speed_kb_s = (total_content_bytes / 1024.0) / total_tokenize_time;

    std::clog << "\nTotal tokenizing time: " << total_tokenize_time << std::endl;
    std::clog << "Speed tokenizing: " << speed_kb_s << std::endl;
    std::cout << "Total tokenized bytes: " << total_content_bytes / 1024.0 / 1024.0 << " MB\n";

    std::map<std::string_view, uint64_t> token2amount;
    uint64_t total_len = 0;
    counter = 0;
    for (const auto& [link, tokens] : link2tokens) {
        std::clog << "\rAdded tokens from: " << counter++ << " documents";
        for (const auto& token : tokens) {
            auto token_iter = token2amount.find(token);
            if (token_iter == token2amount.end()) {
                total_len += token.size();
                token2amount[token]++;
            } else {
                token_iter->second++;
            }
        }
    }

    std::cout << "\nAverage len: " << (double)total_len / token2amount.size() << std::endl;
    std::cout << "Tokens amount: " << token2amount.size() << std::endl;

    std::ofstream csv("../freq.csv");

    csv << "token;frequency\n";

    for (const auto& [token, amount] : token2amount) {
        csv << token << ';' << amount << '\n';
    }
}

void DocumentDownloader::extractText(GumboNode* node, std::string& buffer) {
    if (node->type == GUMBO_NODE_TEXT) {
        buffer.append(node->v.text.text);
        return;
    }

    if (node->type == GUMBO_NODE_ELEMENT) {
        GumboTag tag = node->v.element.tag;
        if (tag == GUMBO_TAG_SCRIPT || tag == GUMBO_TAG_STYLE || tag == GUMBO_TAG_NOSCRIPT || tag == GUMBO_TAG_IFRAME ||
            tag == GUMBO_TAG_HEAD || tag == GUMBO_TAG_TITLE) {
            return;
        }

        GumboVector* children = &node->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i) {
            extractText(static_cast<GumboNode*>(children->data[i]), buffer);
        }

        switch (tag) {
            case GUMBO_TAG_P:
            case GUMBO_TAG_DIV:
            case GUMBO_TAG_H1:
            case GUMBO_TAG_H2:
            case GUMBO_TAG_H3:
            case GUMBO_TAG_H4:
            case GUMBO_TAG_H5:
            case GUMBO_TAG_H6:
            case GUMBO_TAG_BR:
            case GUMBO_TAG_LI:
            case GUMBO_TAG_TR:
            case GUMBO_TAG_TD:
            case GUMBO_TAG_TH:
            case GUMBO_TAG_ARTICLE:
            case GUMBO_TAG_SECTION:
            case GUMBO_TAG_HEADER:
            case GUMBO_TAG_FOOTER:
            case GUMBO_TAG_BLOCKQUOTE:
            case GUMBO_TAG_PRE:
                buffer += ' ';
                break;
            default:
                break;
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