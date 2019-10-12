#pragma once
#include "drs_base.h"
#include <array>
#include <cstdint>
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

  [[nodiscard]] DRSReaderHeader header() const;

  [[nodiscard]] std::vector<DRSReaderTable> read_tables() const;

  [[nodiscard]] std::vector<DRSReaderFile>
  read_files(const DRSReaderTable& table) const;

  [[nodiscard]] std::vector<char> read_file(const DRSReaderFile& file) const;
};
