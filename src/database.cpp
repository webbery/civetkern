#include "database.h"
#include <iostream>
#if defined(__APPLE__) || defined(__gnu_linux__) || defined(__linux__) 
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#elif defined(WIN32)
#include <direct.h>
#include <io.h>
#endif
#include "util/util.h"
#include "log.h"
#include "json.hpp"

#define MAX_NUM_DATABASE  64
#define DBTHUMBMAIL "t_"    //thumbnail

namespace caxios {
  enum MetaType {
    BT_BINARY,
    BT_STRING,
    BT_VALUE
  };

  CDatabase::CDatabase(const std::string& dbpath, const std::string& name, DBFlag flag, size_t size) {
#if defined(__APPLE__) || defined(UNIX) || defined(__linux__)
    createDirectories(dbpath);
#elif defined(WIN32)
    auto wpath = string2wstring(dbpath);
    createDirectories(wpath);
#endif
    T_LOG("init", "createDirectories: %s", dbpath.c_str());
    if (flag == DBFlag::ReadOnly) m_flag = MDB_RDONLY;
    else m_flag = MDB_WRITEMAP;

    mdb_env_create(&m_pDBEnv);
    mdb_env_set_maxreaders(m_pDBEnv, 4);

    if (const int rc = mdb_env_set_mapsize(m_pDBEnv, size)) {
      T_LOG("init", "mdb_env_set_mapsize fail: %s", err2str(rc).c_str());
      mdb_env_close(m_pDBEnv);
      return;
    }
    if (const int rc = mdb_env_set_maxdbs(m_pDBEnv, MAX_NUM_DATABASE)) {
      T_LOG("init", "mdb_env_set_maxdbs fail: %s", err2str(rc).c_str());
      mdb_env_close(m_pDBEnv);
      return;
    }
    //open_flag |= MDB_NOTLS;
    const std::string schemaDB = dbpath + OS_DELIMITER + name;
    T_LOG("database", "Open DB %s, flag: %d, Mode: %s", schemaDB.c_str(), m_flag, m_flag == MDB_RDONLY? "ReadOnly": "ReadWrite");
    if (const int rc = mdb_env_open(m_pDBEnv, schemaDB.c_str(), m_flag | MDB_NOTLS | MDB_NORDAHEAD | MDB_NOSUBDIR | MDB_NOLOCK, 0664)) {
      T_LOG("database", "mdb_env_open fail: %s", err2str(rc).c_str());
    }
    if (const int rc = mdb_txn_begin(m_pDBEnv, 0, m_flag, &m_pTransaction)) {
      T_LOG("database", "mdb_txn_begin fail: %s", err2str(rc).c_str());
    }
  }
  CDatabase::~CDatabase() {
    if (m_pDBEnv) {
      mdb_env_close(m_pDBEnv);
      m_pDBEnv = nullptr;
    }
    T_LOG("database", "~CDatabase()");
  }

  MDB_dbi CDatabase::OpenDatabase(const std::string& dbname)
  {
    MDB_dbi dbi = -1;
    unsigned int flag = MDB_CREATE;
    if (m_flag & MDB_RDONLY) flag = 0;
    if (const int rc = mdb_dbi_open(m_pTransaction, dbname.c_str(), flag, &dbi)) {
      T_LOG("database", "mdb_dbi_open %s fail %s", dbname.c_str(), err2str(rc).c_str());
      return dbi;
    }
    T_LOG("database", "open database %s: %d", dbname.c_str(), dbi);
    return dbi;
  }

  void CDatabase::CloseDatabase(MDB_dbi dbi)
  {
    if (m_pDBEnv) {
      this->Commit();
      //mdb_close(m_pDBEnv, dbi);
      mdb_env_sync(m_pDBEnv, 1);
    }
  }

  bool CDatabase::Copy2(const std::string& path)
  {
    int rc = 0;
    if ((rc = mdb_env_copy2(m_pDBEnv, path.c_str(), MDB_CP_COMPACT)) != 0) {
      T_LOG("database", "mdb_env_copy2 %s, fail reason: %s", path.c_str(), err2str(rc).c_str());
      return false;
    }
    return true;
  }

  bool CDatabase::Put(MDB_dbi dbi, uint32_t k, void* pData, uint32_t len, int flag) {
    MDB_val key, datum;
    key.mv_data = reinterpret_cast<void*>(&k);
    key.mv_size = sizeof(uint32_t);
    datum.mv_data = pData;
    datum.mv_size = len;
    if (int rc = mdb_put(m_pTransaction, dbi, &key, &datum, 0)) {
      T_LOG("database", "mdb_put fail: %s, key: %d, data: %p, len: %d", err2str(rc).c_str(), k, pData, len);
      return false;
    }
    if (m_dOperator == NORMAL) {
      m_dOperator = TRANSACTION;
    }
    // T_LOG("database", "mdb_put %d, key: %d", m_mDBs[dbname], k);
    return true;
  }

  bool CDatabase::Put(MDB_dbi dbi, const std::string& k, void* pData, uint32_t len, int flag /*= MDB_CURRENT*/)
  {
    MDB_val key, datum;
    key.mv_data = (void*)(k.data());
    key.mv_size = k.size();
    datum.mv_data = pData;
    datum.mv_size = len;
    if (int rc = mdb_put(m_pTransaction, dbi, &key, &datum, 0)) {
      T_LOG("database", "mdb_put fail: %s, key: %s", err2str(rc).c_str(), k.c_str());
      return false;
    }
    if (m_dOperator == NORMAL) {
      m_dOperator = TRANSACTION;
    }
    // T_LOG("database", "mdb_put %d, key: %s", m_mDBs[tbName], k.c_str());
    return true;
  }

  bool CDatabase::Get(MDB_dbi dbi, uint32_t k, void*& pData, uint32_t& len)
  {
    MDB_val key, datum = { 0 };
    key.mv_data = reinterpret_cast<void*>(&k);
    key.mv_size = sizeof(uint32_t);
    this->Begin();
    if (int rc = mdb_get(m_pTransaction, dbi, &key, &datum)) {
      T_LOG("database", "mdb_get %d fail: %s, key: %d", dbi, err2str(rc).c_str(), k);
      //pData = nullptr;
      len = 0;
      return false;
    }
    len = datum.mv_size;
    pData = datum.mv_data;
    // T_LOG("database", "mdb_get %d, key: %u, len: %d", m_mDBs[tbName], k, len);
    return true;
  }

  bool CDatabase::Get(MDB_dbi dbi, const std::string& k, void*& pData, uint32_t& len)
  {
    MDB_val key, datum = { 0 };
    key.mv_data = (void*)(k.data());
    key.mv_size = k.size();
    this->Begin();
    if (int rc = mdb_get(m_pTransaction, dbi, &key, &datum)) {
      T_LOG("database", "mdb_get fail: %s", err2str(rc).c_str());
      len = 0;
      return false;
    }
    len = datum.mv_size;
    pData = datum.mv_data;
    // T_LOG("database", "mdb_get %d, key: %s, len: %d", m_mDBs[tbName], k.c_str(), len);
    return true;
  }

  bool CDatabase::Filter(MDB_dbi dbi, std::function<bool(uint32_t key, void* pData, uint32_t len, void*& newVal, uint32_t& newLen)> cb)
  {
    MDB_cursor* cursor = nullptr;
    int rc = 0;
    this->Begin();
    if (rc = mdb_cursor_open(m_pTransaction, dbi, &cursor)) {
      T_LOG("database", "mdb_cursor_open fail: %s, transaction: %d", err2str(rc).c_str(), m_pTransaction);
      return false;
    }

    MDB_val key, datum = { 0 };
    while ((rc = mdb_cursor_get(cursor, &key, &datum, MDB_NEXT)) == 0) {
      //printf("key: %u, data: %d %s\n",
      //  *(uint32_t*)(key.mv_data),
      //  (int)datum.mv_size, (char*)datum.mv_data);
      void* pNewData = nullptr;
      uint32_t newLen = 0;
      uint32_t uik = *(uint32_t*)(key.mv_data);
      if (cb(uik, datum.mv_data, datum.mv_size, pNewData, newLen)) {
        break;
      }
      if (newLen) {
        MDB_val dupKey, newVal = { 0 };
        newVal.mv_data = pNewData;
        newVal.mv_size = newLen;
        dupKey.mv_data = &uik;
        dupKey.mv_size = sizeof(uint32_t);
        if ((rc = mdb_cursor_put(cursor, &dupKey, &newVal, MDB_CURRENT)) != 0) {
          T_LOG("database", "cursor put error: %s", err2str(rc).c_str());
        }
      }
    }
    mdb_cursor_close(cursor);
    return true;
  }

  bool CDatabase::Filter(MDB_dbi dbi, std::function<bool(const std::string& key, void* pData, uint32_t len, void*& newVal, uint32_t& newLen)> cb)
  {
    MDB_cursor* cursor = nullptr;
    int rc = 0;
    this->Begin();
    if (rc = mdb_cursor_open(m_pTransaction, dbi, &cursor)) {
      T_LOG("database", "mdb_cursor_open fail: %s, transaction: %d", err2str(rc).c_str(), m_pTransaction);
      return false;
    }

    MDB_val key, datum = { 0 };
    while ((rc = mdb_cursor_get(cursor, &key, &datum, MDB_NEXT)) == 0) {
      //printf("key: %u, data: %d %s\n",
      //  *(uint32_t*)(key.mv_data),
      //  (int)datum.mv_size, (char*)datum.mv_data);
      void* pNewData = nullptr;
      uint32_t newLen = 0;
      std::string sKey((char*)key.mv_data, key.mv_size);
      if (cb(sKey, datum.mv_data, datum.mv_size, pNewData, newLen)) {
        break;
      }
      if (newLen) {
        MDB_val newVal = { 0 };
        newVal.mv_data = pNewData;
        newVal.mv_size = newLen;
        if ((rc = mdb_cursor_put(cursor, &key, &newVal, MDB_CURRENT)) != 0) {
          T_LOG("database", "cursor put error: %s", err2str(rc).c_str());
        }
      }
    }
    mdb_cursor_close(cursor);
    return true;
  }

  bool CDatabase::Del(MDB_dbi dbi, uint32_t k)
  {
    MDB_val key, datum = { 0 };
    key.mv_data = reinterpret_cast<void*>(&k);
    key.mv_size = sizeof(uint32_t);

    if (int rc = mdb_del(m_pTransaction, dbi, &key, NULL)) {
      T_LOG("database", "mdb_del fail: %s", err2str(rc).c_str());
      return false;
    }
    if (m_dOperator == NORMAL) {
      m_dOperator = TRANSACTION;
    }
    return true;
  }

  bool CDatabase::Del(MDB_dbi dbi, const std::string& k)
  {
    MDB_val key, datum = { 0 };
    key.mv_data = (void*)(k.data());
    key.mv_size = k.size();

    if (int rc = mdb_del(m_pTransaction, dbi, &key, NULL)) {
      T_LOG("database", "mdb_del fail: %s", err2str(rc).c_str());
      return false;
    }
    if (m_dOperator == NORMAL) {
      m_dOperator = TRANSACTION;
    }
    return true;
  }

  MDB_cursor* CDatabase::OpenCursor(MDB_dbi dbi)
  {
    MDB_cursor* cursor = nullptr;
    this->Begin();
    if (int rc = mdb_cursor_open(m_pTransaction, dbi, &cursor)) {
      T_LOG("database", "mdb_cursor_open fail: %s, transaction: %d", err2str(rc).c_str(), m_pTransaction);
      return nullptr;
    }
    return cursor;
  }

  int CDatabase::MoveNext(MDB_cursor* cursor, MDB_val& key, MDB_val& datum)
  {
    return mdb_cursor_get(cursor, &key, &datum, MDB_NEXT);
  }

  void CDatabase::CloseCursor(MDB_cursor* cursor)
  {
    mdb_cursor_close(cursor);
  }

  MDB_txn* CDatabase::Begin()
  {
    if (m_pTransaction == nullptr) {
      if (int rc = mdb_txn_begin(m_pDBEnv, 0, m_flag, &m_pTransaction)) {
        T_LOG("database", "mdb_txn_begin: %s", err2str(rc).c_str());
        return nullptr;
      }
      return m_pTransaction;
    }
    return nullptr;
  }

  bool CDatabase::Commit()
  {
    if (m_dOperator == NORMAL) return true;
    if (int rc = mdb_txn_commit(m_pTransaction)) {
      T_LOG("database", "mdb_txn_commit: %s", err2str(rc).c_str());
      return false;
    }
    m_pTransaction = nullptr;
    T_LOG("database", "mdb_txn_commit");
    m_dOperator = NORMAL;
    return true;
  }

}