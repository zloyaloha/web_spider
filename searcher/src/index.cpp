#include "index.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string_view>

uint32_t stringHash(std::string_view str) {
    uint32_t hash = 2166136261u;
    for (char c : str) {
        hash ^= (uint8_t)c;
        hash *= 16777619u;
    }
    return hash;
}

void writeVarInt(std::ofstream& out, uint32_t value) {
    while (value >= 128) {
        out.put((char)((value & 127) | 128));
        value >>= 7;
    }
    out.put((char)value);
}

int getVarIntSize(uint32_t value) {
    int size = 1;
    while (value >= 128) {
        size++;
        value >>= 7;
    }
    return size;
}

void RamIndexSource::addUrl(std::string_view url) { urls.emplace_back(url); }

void RamIndexSource::addDocument(const std::string& token, uint32_t doc_id) {
    std::vector<TermInfo>& postings = index.get(token);

    if (postings.empty() || postings.back().doc_id != doc_id) {
        postings.push_back({doc_id, 1});
    }
}

void RamIndexSource::dump(const std::string& filename, bool zip) {
    std::ofstream ofs(filename, std::ios::binary);
    if (!ofs) throw std::runtime_error("Cannot open file for writing");

    std::vector<std::pair<std::string, std::vector<TermInfo>>> sorted_index;
    index.traverse([&](const std::string& term, const std::vector<TermInfo>& docs) { sorted_index.push_back({term, docs}); });

    std::sort(sorted_index.begin(), sorted_index.end(),
              [](const auto& a, const auto& b) { return stringHash(a.first) < stringHash(b.first); });

    BinaryFormat::Header header = {BinaryFormat::MAGIC, zip ? 2u : 1u, (uint32_t)urls.size(), (uint32_t)sorted_index.size()};
    ofs.write(reinterpret_cast<char*>(&header), sizeof(header));

    for (const auto& url : urls) {
        uint32_t len = (uint32_t)url.size();
        ofs.write(reinterpret_cast<char*>(&len), sizeof(len));
        ofs.write(url.data(), len);
    }

    uint64_t current_term_offset = (uint64_t)ofs.tellp() + (sorted_index.size() * sizeof(BinaryFormat::TermEntry));
    uint64_t current_data_offset = current_term_offset;
    for (const auto& [term, docs] : sorted_index) current_data_offset += term.size() + 1;

    for (const auto& [term, docs] : sorted_index) {
        BinaryFormat::TermEntry entry;
        entry.term_hash = stringHash(term);
        entry.term_offset = current_term_offset;
        entry.data_offset = current_data_offset;
        entry.doc_count = (uint32_t)docs.size();
        ofs.write(reinterpret_cast<char*>(&entry), sizeof(entry));

        current_term_offset += term.size() + 1;

        if (zip) {
            uint32_t prev_id = 0;
            for (const auto& p : docs) {
                current_data_offset += getVarIntSize(p.doc_id - prev_id) + getVarIntSize(p.tf);
                prev_id = p.doc_id;
            }
        } else {
            current_data_offset += docs.size() * sizeof(uint32_t) * 2;
        }
    }

    for (const auto& [term, docs] : sorted_index) {
        ofs.write(term.c_str(), term.size() + 1);
    }

    for (const auto& [term, docs] : sorted_index) {
        uint32_t prev_id = 0;
        for (const auto& p : docs) {
            if (zip) {
                writeVarInt(ofs, p.doc_id - prev_id);
                writeVarInt(ofs, p.tf);
                prev_id = p.doc_id;
            } else {
                ofs.write(reinterpret_cast<const char*>(&p.doc_id), sizeof(uint32_t));
                ofs.write(reinterpret_cast<const char*>(&p.tf), sizeof(uint32_t));
            }
        }
    }
}

MappedIndexSource::~MappedIndexSource() {
    if (map_addr && map_addr != MAP_FAILED) {
        munmap((void*)map_addr, file_size);
        map_addr = nullptr;
    }
    if (fd != -1) {
        close(fd);
        fd = -1;
    }
    term_directory = nullptr;
    num_terms = 0;
}

void MappedIndexSource::load(const std::string& filename, int expected_version) {
    fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1) throw std::runtime_error("Cannot open index file");

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        close(fd);
        fd = -1;
        throw std::runtime_error("Cannot stat file");
    }
    file_size = sb.st_size;

    map_addr = (const char*)mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map_addr == MAP_FAILED) {
        close(fd);
        fd = -1;
        map_addr = nullptr;
        throw std::runtime_error("mmap failed");
    }

    auto* header = reinterpret_cast<const BinaryFormat::Header*>(map_addr);
    if (header->magic != BinaryFormat::MAGIC) throw std::runtime_error("Invalid magic");
    if (header->version != expected_version) throw std::runtime_error("Invalid version" + std::to_string(header->version));

    file_version = header->version;
    num_terms = header->num_terms;

    const char* ptr = map_addr + sizeof(BinaryFormat::Header);

    urls.reserve(header->num_docs);
    for (uint32_t i = 0; i < header->num_docs; ++i) {
        uint32_t len = *reinterpret_cast<const uint32_t*>(ptr);
        ptr += sizeof(uint32_t);
        urls.emplace_back(ptr, len);
        ptr += len;
    }

    term_directory = reinterpret_cast<const BinaryFormat::TermEntry*>(ptr);
}

const BinaryFormat::TermEntry* MappedIndexSource::findTermEntry(const std::string& term) const {
    if (!term_directory || num_terms == 0 || !map_addr) return nullptr;

    size_t h = stringHash(term);

    auto it = std::lower_bound(term_directory, term_directory + num_terms, h,
                               [](const BinaryFormat::TermEntry& entry, size_t val) { return entry.term_hash < val; });

    while (it != term_directory + num_terms && it->term_hash == h) {
        const char* stored_term = map_addr + it->term_offset;
        std::string_view stored_view(stored_term);

        if (stored_view == term) {
            return it;
        }
        it++;
    }
    return nullptr;
}

uint32_t readVarInt(const char*& ptr) {
    uint32_t value = 0;
    uint32_t shift = 0;
    while (true) {
        uint8_t byte = *reinterpret_cast<const uint8_t*>(ptr++);
        value |= (static_cast<uint32_t>(byte & 127) << shift);
        if (!(byte & 128)) break;
        shift += 7;
    }
    return value;
}

std::vector<TermInfo> MappedIndexSource::getPostings(const std::string& term) {
    const auto* entry = findTermEntry(term);
    if (!entry) return {};

    std::vector<TermInfo> results;
    results.reserve(entry->doc_count);

    const char* data_ptr = map_addr + entry->data_offset;
    uint32_t last_doc_id = 0;
    for (uint32_t i = 0; i < entry->doc_count; ++i) {
        if (file_version == 1) {
            uint32_t doc_id = *reinterpret_cast<const uint32_t*>(data_ptr);
            uint32_t tf = *reinterpret_cast<const uint32_t*>(data_ptr + sizeof(int));
            results.push_back(TermInfo{static_cast<uint32_t>(doc_id), static_cast<uint32_t>(tf)});
            data_ptr += sizeof(int) * 2;
        } else {
            uint32_t delta = readVarInt(data_ptr);
            uint32_t current_doc_id = last_doc_id + delta;

            uint32_t tf = readVarInt(data_ptr);

            results.push_back({current_doc_id, tf});

            last_doc_id = current_doc_id;
        }
    }

    return results;
}

std::string MappedIndexSource::getUrl(int doc_id) const {
    if (doc_id >= 0 && doc_id < (int)urls.size()) return urls[doc_id];
    return "";
}