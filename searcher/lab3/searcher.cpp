#include "searcher.h"

#include <cxxopts.hpp>
#include <iostream>
#include <memory>
#include <mongocxx/instance.hpp>
#include <string>

#include "db_downloader.h"
#include "indexator.h"
#include "tokenizer.h"

int main(int argc, char* argv[]) {
    cxxopts::Options options("searcher", "Searcher");

    options.add_options()("z,zip", "Compress index")("i,index", "Build index")("limit", "Download limit",
                                                                               cxxopts::value<int>()->default_value("1000000"))(
        "dump", "Dump path", cxxopts::value<std::string>()->default_value("../dump.idx"))("h,help", "Print help");

    auto r = options.parse(argc, argv);

    if (r.count("help")) {
        std::cout << options.help() << "\n";
        return 0;
    }

    bool build_index = r.count("index") > 0;
    bool zip = r.count("zip") > 0;
    int limit = r["limit"].as<int>();
    std::string dump_path = r["dump"].as<std::string>();

    std::vector<std::string> urls;
    auto stemmer = std::make_unique<PorterStemmer>();
    auto tokenizer = std::make_shared<Tokenizer>(std::move(stemmer));
    auto source = std::make_shared<RamIndexSource>();
    auto indexator = std::make_shared<TFIDFIndexator>(source, tokenizer);
    if (build_index) {
        mongocxx::instance inst{};
        DocumentDownloader downloader("mongodb://localhost:27017", indexator);

        std::cout << "Started downloading documents\n";
        auto start_time = std::chrono::high_resolution_clock::now();
        downloader.downloadDocuments(limit);
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end_time - start_time;
        std::cout << "Total time: " << duration.count() << " sec\n";

        start_time = std::chrono::high_resolution_clock::now();
        source->dump(dump_path, zip);
        end_time = std::chrono::high_resolution_clock::now();
        duration = end_time - start_time;
        std::cout << "Index dumped in " << duration.count() << " sec!\n";
    }
    auto mapped_source = std::make_shared<MappedIndexSource>(dump_path);
    TFIDFSearcher searcher(mapped_source, tokenizer);
    while (true) {
        std::cout << "Enter query: ";
        std::string request;
        std::getline(std::cin, request);

        auto start_time = std::chrono::high_resolution_clock::now();
        auto result = searcher.findDocument(request);
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end_time - start_time;

        std::cout << "Top 10 results" << std::endl;
        for (int i = 0; i < 10 && i < result.size(); ++i) {
            std::cout << "[" << i + 1 << "] " << result[i].first << " TF-IDF: " << result[i].second << '\n';
        }
        std::cout << "Query time: " << duration.count() << " sec\n";
        std::cout << "Number of results: " << result.size() << " items\n";
    }
}