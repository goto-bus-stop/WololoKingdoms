#pragma once
#include <mio/mmap.hpp>
#include <fs.h>
#include <vector>
#include <string_view>
#include <cstdio>
#include <cstring>
#include <drs_base.h>

struct DRSReaderHeader {
private:
  char copyright_[40];
  char version_[4];
  char password_[12];
  int32_t num_tables_;
  int32_t files_offset_;

public:
  DRSReaderHeader() = default;
  DRSReaderHeader(const DRSReaderHeader&) = default;

  std::string_view copyright() const { return copyright_; }
  std::string_view version() const { return version_; }
  std::string_view password() const { return password_; }
  int32_t num_tables() const { return num_tables_; }
  int32_t files_offset() const { return files_offset_; }
};

struct DRSReaderTable {
private:
  DRSTableType type_;
  int32_t offset_;
  int32_t num_files_;

public:
  DRSReaderTable() = default;
  DRSReaderTable(const DRSReaderTable&) = default;

  DRSTableType file_type() const { return type_; }
  int32_t offset() const { return offset_; }
  int32_t num_files() const { return num_files_; }
};

struct DRSReaderFile {
private:
  int32_t id_;
  int32_t offset_;
  int32_t size_;

public:
  DRSReaderFile() = default;
  DRSReaderFile(const DRSReaderFile&) = default;

  int32_t id() const { return id_; }
  int32_t offset() const { return offset_; }
  int32_t size() const { return size_; }
};

class DRSReader {
private:
  mio::mmap_source mmap_;

public:
  DRSReader(fs::path filename) : mmap_(filename.string()) {}

  DRSReaderHeader header() const {
    DRSReaderHeader header = {};
    std::memcpy(&header, mmap_.data(), sizeof(DRSReaderHeader));
    return header;
  }

  std::vector<DRSReaderTable> read_tables() const {
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

  std::vector<DRSReaderFile> read_files(const DRSReaderTable& table) const {
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

  std::vector<char> read_file(const DRSReaderFile& file) const {
    auto raw_bytes = mmap_.data() + file.offset();
    return std::vector(raw_bytes, raw_bytes + file.size());
  }
};
