// #include <bsoncxx/builder/stream/document.hpp>
// #include <bsoncxx/json.hpp>
// #include <fstream>
// #include <iostream>
// #include <mongocxx/client.hpp>
// #include <mongocxx/instance.hpp>
// #include <mongocxx/options/find.hpp>  // Не забудьте подключить этот заголовок
// #include <mongocxx/uri.hpp>
// #include <string_view>

// using bsoncxx::builder::stream::document;
// using bsoncxx::builder::stream::finalize;

// #include "tokenizer.h"

#include "db_downloader.h"
int main() {
    mongocxx::instance inst{};
    DocumentDownloader downloader("mongodb://localhost:27017", nullptr);
    downloader.downloadDocumentsWithonIndexation();
}