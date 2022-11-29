#ifndef _CAXIOS_DB_MANAGER_H_
#define _CAXIOS_DB_MANAGER_H_
#include "json.hpp"
#include <map>
#include "log.h"
#include "gqlite.h"
#include "datum_type.h"

#define TABLE_FILEID        32    // "file_cur_id"

namespace caxios {

  int gqlite_callback_func(gqlite_result* result, void* handle);

  struct ResultSet {
    gqlite_result* _result;
  };

  //
  class IGQLExecutor {
  public:
    virtual bool execute() = 0;
    virtual const ResultSet& get() const = 0;

    friend int gqlite_callback_func(gqlite_result* result, void* handle);
  protected:
    ResultSet _rs;
  };

  class CivetStorage {
  public:
    CivetStorage(const std::string& dbdir, int flag, const std::string& meta = "");
    ~CivetStorage();

    std::vector<FileID> GenerateNextFilesID(int cnt = 1);
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
    bool GetAllTags(TagTable& tags);
    bool UpdateFilesClasses(const std::vector<FileID>& filesID, const std::vector<std::string>& classes);
    bool UpdateClassName(const std::string& oldName, const std::string& newName);
    bool UpdateFileMeta(const std::vector<FileID>& filesID, const nlohmann::json& mutation);
    bool Query(const std::string& query, std::vector< FileInfo>& filesInfo);
    bool Insert(const std::string& sql);
    bool Remove(const std::string& sql);

    void CreateGQLTasks(const std::list<IGQLExecutor*>& task);

  private:
    //void InitDB(CStorageProxy*& pDB, const char* dir, size_t size);
    void InitTable(const std::string& meta);
    void InitMap();
    void TryUpdate(const std::string& meta);
    void AddMetaImpl(const std::vector<FileID>& files, const nlohmann::json& meta);
    //bool AddBinMeta(FileID, )
    bool AddFileID2Tag(const std::vector<FileID>&, WordIndex);
    bool AddFileID2Keyword(FileID, WordIndex);
    void UpdateChildrenClassName(
      const std::string& clssKey, const std::string& oldarentName, const std::string& newParentName,
      const std::vector<std::string>& oldStrings, const std::vector<WordIndex>& vWordsIndex);
    bool AddKeyword2File(WordIndex, FileID);
    void BindKeywordAndFile(WordIndex, FileID);
    void BindKeywordAndFile(const std::vector<WordIndex>&, const std::vector<FileID>&);
    bool AddTagPY(const std::string& tag, WordIndex indx);
    bool AddClass2FileID(uint32_t, const std::vector<FileID>& vFilesID);
    bool AddFileID2Class(const std::vector<FileID>&, uint32_t);
    void MapHash2Class(uint32_t clsID, const std::string& name);
    std::vector<uint32_t> AddClassImpl(const std::vector<std::string>& classes);
    void InitClass(const std::string& key, uint32_t curClass, uint32_t parent);
    bool RemoveFile(FileID);
    void RemoveFile(FileID, const std::string& file2type, const std::string& type2file);
    bool RemoveTag(FileID, const Tags& tags);
    bool RemoveClassImpl(const std::string& classPath);
    bool RemoveKeywords(const std::vector<FileID>& filesID, const std::vector<std::string>& keyword);
    bool RemoveFileIDFromKeyword(FileID fileID);
    bool GetFileInfo(FileID fileID, MetaItems& meta, Keywords& keywords, Tags& tags, Annotations& anno);
    bool GetFileTags(FileID fileID, Tags& tags);
    std::vector<FileID> GetFilesByClass(const std::vector<WordIndex>& clazz);
    bool IsFileExist(FileID fileID);
    bool IsClassExist(const std::string& clazz);
    uint32_t GenerateClassHash(const std::string& clazz);
    uint32_t GetClassHash(const std::string& clazz);
    uint32_t GetClassParent(const std::string& clazz);
    std::pair<uint32_t, std::string> EncodePath2Hash(const std::string& classPath);
    std::vector<ClassID> GetClassChildren(const std::string& clazz);
    std::string GetClassByHash(ClassID);
    std::string GetClassKey(ClassID);
    std::string GetClassKey(const std::string& clsPath);
    std::vector<FileID> GetFilesOfClass(uint32_t clsID);
    std::vector<FileID> mapExistFiles(const std::vector<FileID>&);
    nlohmann::json ParseMeta(const std::string& meta);
    void SetSnapStep(FileID fileID, int bit, bool set=true);
    char GetSnapStep(FileID fileID, nlohmann::json&);
    Snap GetFileSnap(FileID);
    WordIndex GetWordIndex(const std::string& word);
    bool InitBinaryTables();
    bool CanBeQuery(const nlohmann::json& meta);

    std::vector<std::vector<FileID>> GetFilesIDByTagIndex(const WordIndex* const wordsIndx, size_t cnt);

    void ClearTasks();
  private:
    void execGQL(const std::string& gql, IGQLExecutor* executor);
  private:
    std::vector<std::string> _binTables;
    DBFlag _flag = ReadWrite;
    gqlite* _pHandle = nullptr;
    std::list<IGQLExecutor*> _executors;
  };
  
}

#endif
