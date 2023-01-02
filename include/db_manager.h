#ifndef _CAXIOS_DB_MANAGER_H_
#define _CAXIOS_DB_MANAGER_H_
#include "json.hpp"
#include <map>
#include <list>
#include <functional>
#include "log.h"
#include "gqlite.h"
#include "datum_type.h"

#define TABLE_FILEID        32    // "file_cur_id"

namespace caxios {
  class CivetStorage;

  struct ResultSet {
    gqlite_result* _result;
  };

  //
  class IGQLExecutor {
  public:
    void operator()(gqlite_result* result);

    friend int gqlite_callback_func(gqlite_result* result, void* handle);
  };

  class CivetStorage {
  public:
    CivetStorage(const std::string& dbdir, int flag, const std::string& meta = "");
    ~CivetStorage();

    //bool AddFiles(const std::vector <std::tuple< FileID, MetaItems, Keywords >>&);
    bool AddFile(FileID, const MetaItems&, const Keywords&);
    bool AddClasses(const std::vector<std::string>& classes);
    bool AddClasses(const std::vector<std::string>& classes, const std::vector<FileID>& filesID);
    bool AddMeta(const std::vector<FileID>& files, const nlohmann::json& meta);
    bool RemoveFiles(const std::vector<FileID>& filesID);
    bool RemoveTags(const std::vector<FileID>& files, const Tags& tags);
    bool RemoveClasses(const std::vector<std::string>& classes);
    bool RemoveClasses(const std::vector<std::string>& classes, const std::vector<FileID>& filesID);
    bool SetTags(const std::vector<FileID>& filesID, const std::vector<std::string>& tags);
    bool GetFilesInfo(const std::vector<FileID>& filesID, std::vector< FileInfo>& filesInfo);
    bool GetFilesSnap(std::vector< Snap >& snaps);
    size_t GetFileCountOfClass(ClassID cid);
    size_t GetAllFileCountOfClass(ClassID cid);
    bool GetUntagFiles(std::vector<FileID>& filesID);
    bool GetUnClassifyFiles(std::vector<FileID>& filesID);
    bool GetTagsOfFiles(const std::vector<FileID>& filesID, std::vector<Tags>& tags);
    bool GetClasses(const std::string& parent, nlohmann::json& classes);
    bool getClassesInfo(const std::string& classPath, nlohmann::json& clsInfo);
    bool getChildrenClassInfo(ClassID clsID, nlohmann::json& clsInfo);
    bool GetAllTags(TagTable& tags);
    bool UpdateFilesClasses(const std::vector<FileID>& filesID, const std::vector<std::string>& classes);
    bool UpdateClassName(const std::string& oldName, const std::string& newName);
    bool UpdateFileMeta(const std::vector<FileID>& filesID, const nlohmann::json& mutation);
    bool Query(const std::string& query, std::vector< FileInfo>& filesInfo);
    bool Insert(const std::string& sql);
    bool Remove(const std::string& sql);

  private:
    //void InitDB(CStorageProxy*& pDB, const char* dir, size_t size);
    void InitTable(const std::string& meta);
    void InitMap();
    void TryUpdate(const std::string& meta);
    //bool AddBinMeta(FileID, )
    bool AddClass2FileID(uint32_t, const std::vector<FileID>& vFilesID);
    bool AddFileID2Class(const std::vector<FileID>&, uint32_t);
    uint64_t GetClassID(const std::string& name);
    std::vector<uint64_t> AddClassImpl(const std::vector<std::string>& classes);
    void InitClass(const std::string& key, uint32_t curClass, uint32_t parent);
    bool RemoveFile(FileID);
    void RemoveFile(FileID, const std::string& file2type, const std::string& type2file);
    bool RemoveTag(FileID, const Tags& tags);
    bool RemoveClassImpl(const std::string& classPath);
    bool RemoveKeywords(const std::vector<FileID>& filesID, const std::vector<std::string>& keyword);
    bool RemoveFileIDFromKeyword(FileID fileID);
    bool GetFileInfo(FileID fileID, MetaItems& meta, Keywords& keywords, Tags& tags, Annotations& anno);
    bool GetFileTags(FileID fileID, Tags& tags);
    std::vector<FileID> GetFilesByClass(const std::vector<std::string>& clazz);
    std::string GetClassInfo(ClassID clsID);
    uint32_t GetClassParent(const std::string& clazz);
    std::vector<ClassID> GetClassChildren(ClassID parentClsID);
    std::vector<FileID> GetFilesOfClass(uint32_t clsID);
    nlohmann::json ParseMeta(const std::string& meta);
    void SetSnapStep(FileID fileID, int bit, bool set=true);
    char GetSnapStep(FileID fileID, nlohmann::json&);
    Snap GetFileSnap(FileID);
    bool InitBinaryTables();
    bool CanBeQuery(const nlohmann::json& meta);

    std::vector<std::vector<FileID>> GetFilesIDByTag(const std::vector<std::string>& tags);

  private:
    void execGQL(const std::string& gql);
    void execGQL(const std::string& gql, std::function<void(gqlite_result*)> func);
  private:
    std::vector<std::string> _binTables;
    DBFlag _flag = ReadWrite;
    gqlite* _pHandle = nullptr;
  };
  
}

#endif
