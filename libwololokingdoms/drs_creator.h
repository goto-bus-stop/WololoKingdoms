#pragma once
#include <fs.h>
#include <iostream>
#include <map>
#include <utility>
#include <vector>
#include <drs_base.h>

class DRSCreatorTableEntry;
class DRSCreatorTable;
class DRSCreator;

class DRSCreatorTableEntry {
  uint32_t id_;
  uint32_t offset_ = 0;
  uint32_t size_ = 0;
  // Owned by this object
  std::istream* stream_ = nullptr;
  fs::path filename_;
  std::optional<std::vector<char>> bytes_;

public:
  inline DRSCreatorTableEntry(uint32_t id, std::istream* data)
      : id_(id), stream_(data) {}
  inline DRSCreatorTableEntry(uint32_t id, fs::path filename)
      : id_(id), filename_(std::move(filename)) {}
  inline DRSCreatorTableEntry(uint32_t id, std::vector<char> data)
      : id_(id), bytes_(std::move(data)) {}
  inline DRSCreatorTableEntry(DRSCreatorTableEntry&& entry)
      : id_(entry.id_), stream_(entry.stream_),
        filename_(std::move(entry.filename_)) {
    entry.stream_ = nullptr;
  }
  DRSCreatorTableEntry(const DRSCreatorTableEntry&) = delete;
  DRSCreatorTableEntry& operator=(DRSCreatorTableEntry const&);
  inline ~DRSCreatorTableEntry() {
    // may be moved out.
    if (stream_ != nullptr)
      delete stream_;
  }

private:
  inline void writeMeta(std::ostream& target);
  inline void writeContent(std::ostream& target);
  friend class DRSCreatorTable;
};

class DRSCreatorTable {
  std::vector<DRSCreatorTableEntry> files_;
  uint32_t offset_ = 0;
  bool wrote_contents_ = false;

public:
  inline void addFile(uint32_t id, std::istream* data);
  inline void addFile(uint32_t id, fs::path filename);
  inline void addFile(uint32_t id, std::vector<char> data);

private:
  inline void setOffset(uint32_t offset);
  inline size_t getMetaSize();
  inline void writeMeta(DRSTableType type, std::ostream& target);
  inline void writeFileMeta(std::ostream& target);
  inline void writeFileContents(std::ostream& target);
  friend class DRSCreator;
};

class DRSCreator {
  std::ostream& output_;
  std::map<DRSTableType, DRSCreatorTable> tables_;

public:
  inline DRSCreator(std::ostream& output) : output_(output) {}

  inline void addFile(DRSTableType table, uint32_t id, std::istream* data);
  inline void addFile(DRSTableType table, uint32_t id, fs::path filename);
  inline void addFile(DRSTableType table, uint32_t id, std::vector<char> data);
  void commit();
};

inline void DRSCreatorTable::addFile(uint32_t id, std::istream* data) {
  files_.emplace_back(id, data);
}

inline void DRSCreatorTable::addFile(uint32_t id, fs::path filename) {
  files_.emplace_back(id, std::move(filename));
}

inline void DRSCreatorTable::addFile(uint32_t id, std::vector<char> data) {
  files_.emplace_back(id, std::move(data));
}

inline void DRSCreator::addFile(DRSTableType table, uint32_t id,
                                std::istream* data) {
  tables_[table].addFile(id, data);
}

inline void DRSCreator::addFile(DRSTableType table, uint32_t id, fs::path filename) {
  tables_[table].addFile(id, std::move(filename));
}

inline void DRSCreator::addFile(DRSTableType table, uint32_t id, std::vector<char> data) {
  tables_[table].addFile(id, std::move(data));
}
