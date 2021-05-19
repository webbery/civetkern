#include "TableBinaryMeta.h"

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
        for (auto fid : fileid) {
            _pDatabase->Put(_dbi, fid, (void*)vFilesID.data(), vFilesID.size() * sizeof(FileID));
        }
        return true;
    }

    bool TableBinaryMeta::Delete(const std::string& k, FileID fileID){
        return true;
    }

}