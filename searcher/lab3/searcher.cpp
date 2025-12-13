#include "searcher.h"

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <iostream>
#include <memory>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/options/find.hpp>  // Не забудьте подключить этот заголовок
#include <mongocxx/uri.hpp>
#include <string>
#include <string_view>

#include "tokenizer.h"

using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;

int main() {
    std::unordered_map<std::string_view, std::vector<std::string>> link2tokens;

    mongocxx::instance inst{};

    mongocxx::client client{mongocxx::uri{"mongodb://localhost:27017"}};

    auto db = client["sports_corpus"];
    auto coll = db["documents"];
    size_t total_bytes = 0;
    auto start_time = std::chrono::high_resolution_clock::now();

    auto projection = document{} << "normalized_url" << 1 << "content" << 1 << "_id" << 0 << finalize;

    int64_t count = coll.count_documents({});

    mongocxx::options::find opts;
    opts.projection(projection.view());

    auto cursor = coll.find({}, opts);

    auto stemmer = std::make_unique<PorterStemmer>();
    auto tokenizer = std::make_shared<Tokenizer>(std::move(stemmer));

    SimpleHashMap index;
    std::vector<std::string> urls;

    Indexator indexator(index, urls, tokenizer);
    int counter = 0;
    for (auto&& doc : cursor) {
        bsoncxx::document::element url_ele = doc["normalized_url"];
        bsoncxx::document::element content = doc["content"];
        std::string_view url_view(url_ele.get_string().value.data(), url_ele.get_string().value.size());
        std::string_view content_view(content.get_string().value.data(), content.get_string().value.size());

        total_bytes += content_view.size();

        indexator.addDocument(url_view.data(), content_view.data());
        std::cout << counter++ << std::endl;
        std::cout << url_view << std::endl;
        break;
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;
    double speed_kb_s = (total_bytes / 1024.0) / duration.count();

    std::cout << "Время: " << duration.count() << " сек\n";
    std::cout << "Объем данных: " << total_bytes / 1024.0 / 1024.0 << " MB\n";
    std::cout << "Скорость обработки: " << speed_kb_s << " KB/s\n";

    Searcher searcher(index, urls, tokenizer);
    while (true) {
        std::string request;
        std::cin >> request;

        start_time = std::chrono::high_resolution_clock::now();
        auto result = searcher.findDocument(request);
        end_time = std::chrono::high_resolution_clock::now();
        duration = end_time - start_time;

        for (int i = 0; i < result.size(); ++i) {
            std::cout << result[i] << '\n';
        }
        std::cout << "Время запроса: " << duration.count() << " сек\n";
        std::cout << "Количество результатов: " << result.size() << " штук\n";
    }
}