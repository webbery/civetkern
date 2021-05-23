#include "TableBinaryMeta.h"
#include "log.h"

namespace caxios {
    TableBinaryMeta::TableBinaryMeta(CDatabase* pDatabase, const std::string& name)
        :ITable(pDatabase)
    {
        _dbi = _pDatabase->OpenDatabase(name);
    }

    TableBinaryMeta::~TableBinaryMeta() {
        if (_dbi) {
            _pDatabase->CloseDatabase(_dbi);
            _dbi = 0;
        }
    }
    bool TableBinaryMeta::Add(const std::string& value, const std::vector<FileID>& fileid) {
        for (FileID fid : fileid) {
          T_LOG("bindb", "add bin data: %d(len:%d)", fid, value.size());
          _pDatabase->Put(_dbi, fid, (void*)value.data(), value.size() * sizeof(char));
        }
        return true;
    }

    bool TableBinaryMeta::Delete(const std::string& k, FileID fileID){
      return _pDatabase->Del(_dbi, fileID);
    }

    Iterator TableBinaryMeta::begin()
    {
      Iterator itr(_pDatabase, _dbi);
      return std::move(itr);
    }

    Iterator TableBinaryMeta::end()
    {
      return Iterator();
    }

    bool TableBinaryMeta::Get(uint32_t key, std::string& val)
    {
      void* pData = nullptr;
      uint32_t len = 0;
      _pDatabase->Get(_dbi, key, pData, len);
      T_LOG("bindb", "get %d: %d", key, len);
      if (len == 0) return false;
      val.assign((char*)pData, len);
      return true;
    }

}