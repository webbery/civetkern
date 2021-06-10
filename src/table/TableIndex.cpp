#include "TableIndex.h"

namespace caxios {

  TableTrieIndex::TableTrieIndex(CDatabase* pDB)
  {

  }

  TableTrieIndex::~TableTrieIndex()
  {

  }

  bool TableTrieIndex::Add(const std::string& word)
  {
    return true;
  }

  std::vector<std::string> TableTrieIndex::Get(const std::string& word)
  {
    std::vector<std::string> results;
    return std::move(results);
  }

}