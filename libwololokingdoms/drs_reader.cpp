#include "drs_reader.h"
#include <vector>
#include <cstring>

DRSReaderHeader DRSReader::header() const {
  DRSReaderHeader header = {};
  std::memcpy(&header, mmap_.data(), sizeof(DRSReaderHeader));
  return header;
}

std::vector<DRSReaderTable> DRSReader::read_tables() const {
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

std::vector<DRSReaderFile>
DRSReader::read_files(const DRSReaderTable& table) const {
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

std::vector<char> DRSReader::read_file(const DRSReaderFile& file) const {
  auto raw_bytes = mmap_.data() + file.offset();
  return std::vector(raw_bytes, raw_bytes + file.size());
}
