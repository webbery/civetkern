#pragma once
#include "Table.h"

namespace caxios {
    // this table is used to store binary data of file
    class TableBinaryMeta: public ITable {
    public:
        TableBinaryMeta(CDatabase* pDatabase, const std::string& name/*, const std::string& stp*/);
        virtual ~TableBinaryMeta();

        virtual bool Add(const std::string& value, const std::vector<FileID>& fileid);
        virtual bool Update(const std::string& current, const UpdateValue& value){}
        virtual bool Delete(const std::string& k, FileID fileID);
        virtual Iterator begin();
        virtual Iterator end();

    private:
        MDB_dbi _dbi = 0;
        std::string _table;
    };
}
