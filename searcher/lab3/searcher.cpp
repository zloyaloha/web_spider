#include "searcher.h"

#include <iostream>
#include <memory>
#include <mongocxx/instance.hpp>
#include <string>

#include "db_downloader.h"
#include "tokenizer.h"

int main(int argc, char* argv[]) {
    SimpleHashMap<int> index;
    std::vector<std::string> urls;
    auto stemmer = std::make_unique<PorterStemmer>();
    auto tokenizer = std::make_shared<Tokenizer>(std::move(stemmer));
    auto source = std::make_shared<RamIndexSource>();
    TFIDFIndexator indexator(source, tokenizer);
    if (argc == 3 && std::string(argv[1]) == "-i") {
        mongocxx::instance inst{};
        DocumentDownloader downloader("mongodb://localhost:27017");

        std::cout << "Начал загрузку документов\n";
        auto start_time = std::chrono::high_resolution_clock::now();
        auto documents = downloader.downloadDocuments(std::stoi(argv[2]));
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end_time - start_time;
        std::cout << "Время: " << duration.count() << " сек\n";

        uint64_t total_bytes = 0;
        start_time = std::chrono::high_resolution_clock::now();

        std::cout << "Начал индексацию документов\n";
        int counter = 0;
        for (const auto& document : documents) {
            total_bytes += document.content.size();
            indexator.addDocument(document.url, document.content);
            std::clog << "\rIndexated " << counter++;
        }
        end_time = std::chrono::high_resolution_clock::now();
        duration = end_time - start_time;

        double speed_kb_s = (total_bytes / 1024.0) / duration.count();

        std::cout << "\nВремя: " << duration.count() << " сек\n";
        std::cout << "Объем данных: " << total_bytes / 1024.0 / 1024.0 << " MB\n";
        std::cout << "Скорость обработки: " << speed_kb_s << " KB/s\n";
        source->dump("../dump.idx");
        std::cout << "Индекс сдамплен!\n";
    }
    auto mapped_source = std::make_shared<MappedIndexSource>("../dump.idx", 2);
    BinarySearcher searcher(mapped_source, tokenizer);
    while (true) {
        std::cout << "Введите запрос: ";
        std::string request;
        std::getline(std::cin, request);

        auto start_time = std::chrono::high_resolution_clock::now();
        auto result = searcher.findDocument(request);
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end_time - start_time;

        std::cout << "Топ 5 результатов" << std::endl;
        for (int i = 0; i < 5 && i < result.size(); ++i) {
            std::cout << result[i] << '\n';
        }
        std::cout << "Время запроса: " << duration.count() << " сек\n";
        std::cout << "Количество результатов: " << result.size() << " штук\n";
    }
}