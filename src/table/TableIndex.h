#pragma once
#include "Table.h"

namespace caxios {
  class TableTrieIndex {
  public:
    TableTrieIndex(CDatabase* pDB);
    ~TableTrieIndex();

    bool Add(const std::string& word);
    std::vector<std::string> Get(const std::string& word);
  };
}