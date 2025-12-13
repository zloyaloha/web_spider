#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <fstream>
#include <iostream>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/options/find.hpp>  // Не забудьте подключить этот заголовок
#include <mongocxx/uri.hpp>
#include <string_view>

using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;

#include "tokenizer.h"

int main() {
    std::unordered_map<std::string_view, std::vector<std::string>> link2tokens;
    Tokenizer tokenizer;

    mongocxx::instance inst{};

    mongocxx::client client{mongocxx::uri{"mongodb://localhost:27017"}};

    auto db = client["sports_corpus"];
    auto coll = db["documents"];
    size_t total_bytes = 0;
    auto start_time = std::chrono::high_resolution_clock::now();

    auto projection = document{} << "normalized_url" << 1 << "content" << 1 << "_id" << 0 << finalize;

    mongocxx::options::find opts;
    opts.projection(projection.view());

    auto cursor = coll.find({}, opts);

    for (auto&& doc : cursor) {
        bsoncxx::document::element url_ele = doc["normalized_url"];
        bsoncxx::document::element content = doc["content"];
        std::string_view url_view(url_ele.get_string().value.data(), url_ele.get_string().value.size());
        std::string_view content_view(content.get_string().value.data(), content.get_string().value.size());

        total_bytes += content_view.size();

        tokenizer.tokenize(content_view);
        link2tokens[url_view] = tokenizer.getTokens();
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;
    double speed_kb_s = (total_bytes / 1024.0) / duration.count();

    std::cout << "Время: " << duration.count() << " сек\n";
    std::cout << "Объем данных: " << total_bytes / 1024.0 / 1024.0 << " MB\n";
    std::cout << "Скорость обработки: " << speed_kb_s << " KB/s\n";

    std::map<std::string_view, uint64_t> token2amount;

    for (const auto& [link, tokens] : link2tokens) {
        for (const auto& token : tokens) {
            auto token_iter = token2amount.find(token);
            if (token_iter == token2amount.end()) {
                token2amount[token]++;
            } else {
                token_iter->second++;
            }
        }
    }

    std::ofstream csv("../freq.csv");

    csv << "token;frequency\n";

    for (const auto& [token, amount] : token2amount) {
        csv << token << ';' << amount << '\n';
    }
}