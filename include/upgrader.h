#pragma once
#include "json.hpp"
#include "log.h"
#include "db_manager.h"

namespace caxios{

  template<int, int> struct Upgrader;

  template<> struct Upgrader<1, 2> {
    static void upgrade(CivetStorage* pUpgrade) {
      //pUpgrade->Filter(TABLE_FILESNAP, [](uint32_t k, void* pData, uint32_t len, void*& newVal, uint32_t& newLen) -> bool {
      //  if (k == 0) return false;
      //  using namespace nlohmann;
      //  std::string str((char*)pData, (char*)pData + len);
      //  json js=json::parse(str);
      //  static std::vector<uint8_t> val;
      //  val = json::to_cbor(js);
      //  newVal = val.data();
      //  newLen = val.size();
      //  T_LOG("upgrade", "key: %d, snap: %s, size: %d", k, str.c_str(), newLen);
      //  return false; // continue upgrade rest value
      //});
    }
  };

  bool upgrade(CivetStorage* pUpgrade, int from, int to);
}