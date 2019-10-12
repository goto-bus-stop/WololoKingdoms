#pragma once
#include "drs_base.h"
#include <cstdio>
#include <cstring>
#include <fs.h>
#include <mio/mmap.hpp>
#include <string_view>
#include <vector>

struct DRSReaderHeader {
private:
  std::array<char, 40> copyright_;
  std::array<char, 4> version_;
  std::array<char, 12> password_;
  int32_t num_tables_;
  int32_t files_offset_;

public:
  DRSReaderHeader() = default;
  DRSReaderHeader(const DRSReaderHeader&) = default;

  [[nodiscard]] std::string_view copyright() const { return {copyright_.data(), copyright_.size()}; }
  [[nodiscard]] std::string_view version() const { return {version_.data(), version_.size()}; }
  [[nodiscard]] std::string_view password() const { return {password_.data(), password_.size()}; }
  [[nodiscard]] int32_t num_tables() const { return num_tables_; }
  [[nodiscard]] int32_t files_offset() const { return files_offset_; }
};

struct DRSReaderTable {
private:
  DRSTableType type_;
  int32_t offset_;
  int32_t num_files_;

public:
  DRSReaderTable() = default;
  DRSReaderTable(const DRSReaderTable&) = default;

  [[nodiscard]] DRSTableType file_type() const { return type_; }
  [[nodiscard]] int32_t offset() const { return offset_; }
  [[nodiscard]] int32_t num_files() const { return num_files_; }
};

struct DRSReaderFile {
private:
  int32_t id_;
  int32_t offset_;
  int32_t size_;

public:
  DRSReaderFile() = default;
  DRSReaderFile(const DRSReaderFile&) = default;

  [[nodiscard]] int32_t id() const { return id_; }
  [[nodiscard]] int32_t offset() const { return offset_; }
  [[nodiscard]] int32_t size() const { return size_; }
};

class DRSReader {
private:
  mio::mmap_source mmap_;

public:
  DRSReader(fs::path filename) : mmap_(filename.string()) {}

  [[nodiscard]] DRSReaderHeader header() const {
    DRSReaderHeader header = {};
    std::memcpy(&header, mmap_.data(), sizeof(DRSReaderHeader));
    return header;
  }

  [[nodiscard]] std::vector<DRSReaderTable> read_tables() const {
    auto h = this->header();
    std::vector<DRSReaderTable> tables;
    tables.reserve(h.num_tables());
    auto raw_bytes = mmap_.data() + sizeof(DRSReaderHeader);
    for (auto i = 0; i < h.num_tables(); i += 1) {
      DRSReaderTable table;
      std::memcpy(&table, raw_bytes, sizeof(DRSReaderTable));
      raw_bytes += sizeof(DRSReaderTable);
      tables.push_back(table);
    }
    return tables;
  }

  [[nodiscard]] std::vector<DRSReaderFile>
  read_files(const DRSReaderTable& table) const {
    std::vector<DRSReaderFile> files;
    files.reserve(table.num_files());
    auto raw_bytes = mmap_.data() + table.offset();
    for (auto i = 0; i < table.num_files(); i += 1) {
      DRSReaderFile file;
      std::memcpy(&file, raw_bytes, sizeof(DRSReaderFile));
      raw_bytes += sizeof(DRSReaderFile);
      files.push_back(file);
    }
    return files;
  }

  [[nodiscard]] std::vector<char> read_file(const DRSReaderFile& file) const {
    auto raw_bytes = mmap_.data() + file.offset();
    return std::vector(raw_bytes, raw_bytes + file.size());
  }
};
