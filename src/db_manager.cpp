#include "db_manager.h"
#include <iostream>
#include "log.h"
#include "util/util.h"
#include <set>
#include <utility>
#include <deque>
#include "util/pinyin.h"
#include "StorageProxy.h"
#include <stack>
#include "upgrader.h"

#define TABLE_SCHEMA        "dbinfo"
#define TABLE_FILESNAP      "file_snap"
#define TABLE_FILE          "file_meta"
#define TABLE_KEYWORD       "keywords"
#define TABLE_TAG           "tags"
#define TABLE_CLASS         "classes"
#define TABLE_RELATION_KEYWORD "kw_rel"
#define TABLE_RELATION_CLASS "cls_rel"
#define TABLE_RELATION_TAG  "tag_rel"
#define TABLE_CLASS_TITLE   "title"
#define TABLE_COUNT         "count"         // about count of some statistic
#define TABLE_ANNOTATION    "annotation"
#define TABLE_MATCH_META    "match_meta"
#define TABLE_MATCH         "match_t"
#define TABLE_RECYCLE_ID    "recycle"
#define GRAPH_NAME          "civet"

#define SCHEMA_VERSION    2

#define CHILD_START_OFFSET  2

#define WRITE_BEGIN()  m_pDatabase->Begin()
#define WRITE_END()    m_pDatabase->Commit()

#define BIT_INIT_OFFSET  0
#define BIT_TAG_OFFSET   1
#define BIT_CLASS_OFFSET 2
#define BIT_TAG     (1<<BIT_TAG_OFFSET)
#define BIT_CLASS   (1<<BIT_CLASS_OFFSET)
//#define BIT_ANNO  (1<<3)

namespace caxios {
  const char* g_tables[] = {
    TABLE_SCHEMA,
    TABLE_FILESNAP,
    TABLE_FILE,
    TABLE_TAG,
    TABLE_CLASS,
    TABLE_COUNT,
    TABLE_ANNOTATION,
    TABLE_MATCH_META,
    TABLE_MATCH
  };

    int gqlite_callback_func(gqlite_result* result, void* handle) {
      if (result->errcode != ECode_Success) return result->errcode;
      IGQLExecutor* excutor = (IGQLExecutor*)handle;
      if (excutor) {
        excutor->_rs._result = result;
        return true;
      }
      return false;
    }

    namespace {

    class QueryExecutor : public IGQLExecutor {
    public:
      QueryExecutor(CivetStorage* storage, const std::string&) : _storage(storage) {}
      virtual bool execute() {
        return true;
      }
      virtual const ResultSet& get() const {
        return _rs;
      }

    private:
      CivetStorage* _storage;
    };

    class UpsetTagExecutor : public IGQLExecutor {
    public:
      UpsetTagExecutor(CivetStorage* storage, const std::string&) {
        _storage = storage;
      }
      virtual bool execute() {
        return true;
      }

      virtual const ResultSet& get() const {}
    private:
      CivetStorage* _storage;
    };

    std::vector<WordIndex> transform(const std::vector<std::string>& words, std::map<std::string, WordIndex>& mIndexes) {
      std::vector<WordIndex> vIndexes;
      vIndexes.resize(words.size());
      std::transform(words.begin(), words.end(), vIndexes.begin(), [&mIndexes](const std::string& word)->WordIndex {
        return mIndexes[word];
        });
      return vIndexes;
    }

  }

  CivetStorage::CivetStorage(const std::string& dbdir, int flag, const std::string& meta/* = ""*/)
  {
    _flag = (flag == 0 ? ReadWrite : ReadOnly);
    T_LOG("Storage", "Create Database in %s", dbdir.c_str());
    if (gqlite_open(&_pHandle, dbdir.c_str()) != ECode_Success) {
        printf("Create Database Error\n");
        return;
    }
    T_LOG("Storage", "begin init table");
    // open all database
    InitTable(meta);
    //if (_flag == ReadWrite) {
    //  if (!IsClassExist(ROOT_CLASS_PATH)) {
    //    InitClass(ROOT_CLASS_PATH, 0, 0);
    //  }
    //}
    //TryUpdate(meta);
  }

  CivetStorage::~CivetStorage()
  {
    if (_pHandle) {
      gqlite_close(_pHandle);
    }
    ClearTasks();
  }

  void CivetStorage::execGQL(const std::string& gql, IGQLExecutor* executor) {
    char* ptr = nullptr;
    gqlite_exec(_pHandle, gql.c_str(), gqlite_callback_func, executor, &ptr);
    if (ptr) gqlite_free(ptr);
  }

  void CivetStorage::InitTable(const std::string& meta) {
    auto group = ParseMeta(meta);
    std::string groupTag = "{" TABLE_TAG ": 'name' }";
    std::string groupClass = "{" TABLE_CLASS ": '" TABLE_CLASS_TITLE "' }";
    std::string groupKeyword = "{" TABLE_KEYWORD ": 'name' }";
    std::string tagRelation = "'" TABLE_RELATION_TAG "'";
    std::string clsRelation = "'" TABLE_RELATION_CLASS "'";
    std::string keyordRelation = "'" TABLE_RELATION_KEYWORD "'";
    std::string gql = std::string("{create: '" GRAPH_NAME "', group: [") + group.dump()
      + ", " + groupTag + ", " + groupClass + ", " + groupKeyword
      + ", " + tagRelation + ", " + clsRelation + ", " + keyordRelation
      + "]};";
    /*
      {create: 'civet', group: [
        {file_meta:['color','size','path','filename','type','datetime','addtime','width','height'],
          index:['color','size','type','datetime','addtime']},
        'tag', 'clazz', 'kw'
       ]};
    */
    gql = caxios::normalize(gql);
    execGQL(gql, nullptr);
  }

  std::vector<caxios::FileID> CivetStorage::GenerateNextFilesID(int cnt /*= 1*/)
  {
    std::vector<FileID> filesID;
    if (_flag == ReadOnly) return filesID;
    
    return filesID;
  }

  bool CivetStorage::AddClasses(const std::vector<std::string>& classes, const std::vector<FileID>& filesID)
  {
    if (_flag == ReadOnly || classes.size() == 0) return false;
    for (auto& val : classes) {
      std::string gql = "{upset: '" TABLE_CLASS "', vertex: ['" + val + "']};";
      execGQL(gql, nullptr);
    }

    for (FileID fid : filesID) {
      std::string gqlClear = std::string("{remove: '") + TABLE_RELATION_CLASS + "', edge: [" + std::to_string(fid) + ", --, *]};";
      execGQL(gqlClear, nullptr);

      for (auto& val : classes) {
        std::string gqlUpset = std::string("{upset: '") + TABLE_RELATION_CLASS + "', edge: [" + std::to_string(fid) + ", --, '" + val + "']};";
        execGQL(gqlUpset, nullptr);
      }
    }

    

    return true;
  }

  bool CivetStorage::AddClasses(const std::vector<std::string>& classes)
  {
    if (_flag == ReadOnly || classes.size() == 0) return false;
    
    for (auto& cls : classes) {
      std::string gql = "{upset: '" TABLE_CLASS "', vertex: ['" + cls + "']};";
      execGQL(gql, nullptr);
    }
    return true;
  }

  bool CivetStorage::AddMeta(const std::vector<FileID>& files, const nlohmann::json& meta)
  {
    std::string name = meta["name"];
    std::string type = meta["type"];
    for (FileID fid : files) {
      std::string gql;
      std::string value;
      if (type == "bin") {
        value = meta["value"];
        gql = "{upset: '" TABLE_FILE "', property: [{'" + name + ":" + value + "'}], where: {id: " + std::to_string(fid) + "}};";
      }
      else {
        value = meta["value"];
        gql = "{upset: '" TABLE_FILE "', property: [{'" + name + ":" + value + "'}], where: {id: " + std::to_string(fid) + "}};";
      }
      execGQL(gql, nullptr);
    }
    return true;
  }

  bool CivetStorage::RemoveFiles(const std::vector<FileID>& filesID)
  {
    for (auto fileID : filesID) {
      std::string fid = std::to_string(fileID);
      std::string gql = "{remove: '" TABLE_FILE "', vertex: [" + fid + "]};";
      execGQL(gql, nullptr);
      gql = "{remove: '" TABLE_RELATION_TAG "', edge: [" + fid + ", --, *]};";
      execGQL(gql, nullptr);
      gql = "{remove: '" TABLE_RELATION_CLASS "', edge: [" + fid + ", --, *]};";
      execGQL(gql, nullptr);
      gql = "{remove: '" TABLE_RELATION_KEYWORD "', edge: [" + fid + ", --, *]};";
      execGQL(gql, nullptr);
    }
    return true;
  }

  bool CivetStorage::RemoveTags(const std::vector<FileID>& filesID, const Tags& tags)
  {
    //auto mWordsIndex = m_pDatabase->GetWordsIndex(tags);
    //std::vector<WordIndex> vTags;
    //std::for_each(mWordsIndex.begin(), mWordsIndex.end(), [&vTags](const std::pair<std::string, WordIndex>& item) {
    //  vTags.emplace_back(item.second);
    //});
    //void* pData = nullptr;
    //uint32_t len = 0;
    //for (auto fileID : filesID)
    //{
    //  m_pDatabase->Get(TABLE_FILE2TAG, fileID, pData, len);
    //  if (len) {
    //    std::vector<WordIndex> vTagIdx((WordIndex*)pData, (WordIndex*)pData + len / sizeof(WordIndex));
    //    eraseData(vTagIdx, vTags);
    //    m_pDatabase->Put(TABLE_FILE2TAG, fileID, vTagIdx.data(), vTagIdx.size() * sizeof(WordIndex));
    //    if (vTagIdx.size() == 0) {
    //      SetSnapStep(fileID, BIT_TAG_OFFSET, false);
    //    }
    //  }
    //}
    //for (auto tagID : vTags) {
    //  m_pDatabase->Get(TABLE_TAG2FILE, tagID, pData, len);
    //  if (len) {
    //    std::vector<FileID> vFils((FileID*)pData, (FileID*)pData + len / sizeof(FileID));
    //    eraseData(vFils, filesID);
    //    m_pDatabase->Put(TABLE_TAG2FILE, tagID, vFils.data(), vFils.size() * sizeof(FileID));
    //  }
    //}
    std::string sTags;
    for (const std::string& tag : tags) {
      sTags += "'" + tag + "',";
    }
    sTags.pop_back();

    for (FileID fid : filesID) {
      std::string gql = "{remove: '" TABLE_RELATION_TAG "', where: [" + std::to_string(fid) + ", --, [" + sTags + "]]};";
      execGQL(gql, nullptr);
    }
    RemoveKeywords(filesID, tags);
    return true;
  }

  bool CivetStorage::RemoveClasses(const std::vector<std::string>& classes)
  {
    for (auto& clsPath : classes) {
      T_LOG("class", "remove class %s", clsPath.c_str());
      std::string gql = "{remove: '" TABLE_CLASS "', where: {" TABLE_CLASS_TITLE ": '" + clsPath + "' } }; ";
      execGQL(gql, nullptr);
    }

    return true;
  }

  bool CivetStorage::RemoveClasses(const std::vector<std::string>& classes, const std::vector<FileID>& filesID)
  {
    // update keyword of files
    //RemoveKeywords(filesID, classes);

    //std::vector<ClassID> vClassID;
    //for (auto& cPath : classes) {
    //  ClassID cid;
    //  std::string _s;
    //  std::tie(cid, _s) = EncodePath2Hash(cPath);
    //  vClassID.emplace_back(cid);
    //}
    //void* pData = nullptr;
    //uint32_t len = 0;
    //// remove file 2 classes
    //for (FileID fid : filesID) {
    //  m_pDatabase->Get(TABLE_FILE2CLASS, fid, pData, len);
    //  if (len) {
    //    std::vector<ClassID> vSrcID((ClassID*)pData, (ClassID*)pData + len / sizeof(ClassID));
    //    eraseData(vSrcID, vClassID);
    //    m_pDatabase->Put(TABLE_FILE2CLASS, fid, vSrcID.data(), vSrcID.size() * sizeof(ClassID));
    //  }
    //}
    //// remove classes 2 file
    //for (ClassID cid : vClassID) {
    //  m_pDatabase->Get(TABLE_CLASS2FILE, cid, pData, len);
    //  if (len) {
    //    std::vector<FileID> vSrcID((FileID*)pData, (FileID*)pData + len / sizeof(FileID));
    //    eraseData(vSrcID, filesID);
    //    m_pDatabase->Put(TABLE_CLASS2FILE, cid, vSrcID.data(), vSrcID.size() * sizeof(FileID));
    //  }
    //}
    return true;
  }

  bool CivetStorage::SetTags(const std::vector<FileID>& filesID, const std::vector<std::string>& tags)
  {
    if (_flag == ReadOnly) return false;

    for (const std::string& val : tags) {
      std::string gql = "{upset: '" TABLE_TAG "', vertex: ['" + val + "']};";
      execGQL(gql, nullptr);
    }

    for (FileID fid : filesID) {
      std::string gqlClear = std::string("{remove: '") + TABLE_RELATION_TAG + "', edge: [" + std::to_string(fid) + ", --, *]};";
      execGQL(gqlClear, nullptr);

      for (const std::string& val : tags) {
        std::string gqlUpset = std::string("{upset: '") + TABLE_RELATION_TAG + "', edge: [" + std::to_string(fid) + ", --, '" + val + "']};";
        execGQL(gqlUpset, nullptr);
      }
    }
    return true;
  }

  bool CivetStorage::GetFilesInfo(const std::vector<FileID>& filesID, std::vector< FileInfo>& filesInfo)
  {
    T_LOG("files", "fileid: %s", format_vector(filesID).c_str());
    //READ_BEGIN(TABLE_FILE_META);
    for (auto fileID: filesID)
    {
      std::list<IGQLExecutor*> tasks;
      std::string gql = "{query: '" TABLE_FILE "', where: {id: " + std::to_string(fileID) + "}};";
      tasks.push_back(new QueryExecutor(this, gql));
      gql = "{query: '" TABLE_RELATION_TAG "', where: [" + std::to_string(fileID) + ", --, *]};";
      tasks.push_back(new QueryExecutor(this, gql));
      gql = "{query: '" TABLE_RELATION_KEYWORD "', where: [" + std::to_string(fileID) + ", --, *]};";
      tasks.push_back(new QueryExecutor(this, gql));
      gql = "{query: '" TABLE_RELATION_CLASS "', where: [" + std::to_string(fileID) + ", --, *]};";
      tasks.push_back(new QueryExecutor(this, gql));
    //  // binary data
    //  if (_binTables.size() == 0) {
    //    InitBinaryTables();
    //  }
    //  for (auto& name: _binTables) {
    //    auto pTable = m_pDatabase->GetMetaTable(name);
    //    T_LOG("files", "binary table name: %s, ptr: %x", name.c_str(), pTable);
    //    std::string bin;
    //    if (pTable->Get(fileID, bin)) {
    //      MetaItem item;
    //      item["name"] = name;
    //      item["type"] = "bin";
    //      item["value"] = bin;
    //      items.emplace_back(item);
    //    }
    //  }
    //  // tag
    //  Tags tags;
    //  if (m_pDatabase->Get(TABLE_FILE2TAG, fileID, pData, len)) {
    //    WordIndex* pWordIndex = (WordIndex*)pData;
    //    tags = GetWordByIndex(pWordIndex, len/sizeof(WordIndex));
    //    T_LOG("files", "file tags cnt: %llu", tags.size());
    //    for (auto& t : tags)
    //    {
    //      T_LOG("files", "file %d tags: %s", fileID, t.c_str());
    //    }
    //  }
    //  // ann
    //  // clazz
    //  Classes classes;
    //  if (m_pDatabase->Get(TABLE_FILE2CLASS, fileID, pData, len)) {
    //    uint32_t* pClassID = (uint32_t*)pData;
    //    for (size_t idx = 0; idx < len / sizeof(uint32_t); ++idx) {
    //      std::string clsName = GetClassByHash(*(pClassID + idx));
    //      classes.push_back(clsName);
    //      T_LOG("files", "class id: %d, name: %s", *(pClassID + idx), clsName.c_str());
    //    }
    //  }
    //  // keyword
    //  Keywords keywords;
    //  if (m_pDatabase->Get(TABLE_FILE2KEYWORD, fileID, pData, len)) {
    //    WordRef* pWordIndex = (WordRef*)pData;
    //    keywords = GetWordByIndex(pWordIndex, len / sizeof(WordRef));
    //    T_LOG("files", "keyword[%llu]: %s", keywords.size(), format_vector(keywords).c_str());
    //  }
    //  FileInfo fileInfo{ fileID, items, tags, classes, {}, keywords };
    //  filesInfo.emplace_back(fileInfo);
    }
    return true;
  }

  bool CivetStorage::GetFilesSnap(std::vector< Snap >& snaps)
  {
    //READ_BEGIN(TABLE_FILESNAP);
    //m_pDatabase->Filter(TABLE_FILESNAP, [&snaps](uint32_t k, void* pData, uint32_t len, void*& newVal, uint32_t& newLen) -> bool {
    //  if (k == 0) return false;
    //  using namespace nlohmann;
    //  std::vector<uint8_t> js((uint8_t*)pData, (uint8_t*)pData + len);
    //  json file=json::from_cbor(js);
    //  try {
    //    std::string display = trunc(to_string(file["value"]));
    //    std::string val = trunc(file["step"].dump());
    //    T_LOG("snap", "File step: %s", val.c_str());
    //    int step = std::stoi(val);
    //    T_LOG("snap", "File step: %d, %s", step, val.c_str());
    //    Snap snap{ k, display, step };
    //    snaps.emplace_back(snap);
    //  }catch(json::exception& e){
    //    T_LOG("snap", "ERR: %s", e.what());
    //  }
    //  return false;
    //});
    return true;
  }

  size_t CivetStorage::GetFileCountOfClass(ClassID cid)
  {
    //void* pData = nullptr;
    //uint32_t len = 0;
    //m_pDatabase->Get(TABLE_CLASS2FILE, cid, pData, len);
    //return len / sizeof(FileID);
  }

  size_t CivetStorage::GetAllFileCountOfClass(ClassID cid)
  {
    //size_t count = GetFileCountOfClass(cid);
    //auto key = GetClassKey(cid);
    //auto children = GetClassChildren(key);
    //for (int idx = 0; idx < children.size();) {
    //  ClassID child = children[idx];
    //  count += GetFileCountOfClass(child);
    //  children.erase(children.begin());
    //  auto ck = GetClassKey(child);
    //  auto cc = GetClassChildren(ck);
    //  if (cc.size()) children.insert(children.end(), cc.begin(),cc.end());
    //}
    //return count;
  }

  bool CivetStorage::GetUntagFiles(std::vector<FileID>& filesID)
  {
    //std::vector<Snap> vSnaps;
    //if (!GetFilesSnap(vSnaps)) return false;
    //T_LOG("tag", "File Snaps: %llu", vSnaps.size());
    //for (auto& snap : vSnaps) {
    //  char bits = std::get<2>(snap);
    //  T_LOG("tag", "untag file %d: %d", std::get<0>(snap), bits);
    //  if (!(bits & BIT_TAG)) {
    //    filesID.emplace_back(std::get<0>(snap));
    //  }
    //}
    return true;
  }

  bool CivetStorage::GetUnClassifyFiles(std::vector<FileID>& filesID)
  {
    std::vector<Snap> vSnaps;
    //if (!GetFilesSnap(vSnaps)) return false;
    //for (auto& snap : vSnaps) {
    //  char bits = std::get<2>(snap);
    //  T_LOG("class", "unclassify file %d: %d", std::get<0>(snap), bits);
    //  if (!(bits & BIT_CLASS)) {
    //    filesID.emplace_back(std::get<0>(snap));
    //  }
    //}
    return true;
  }

  bool CivetStorage::GetTagsOfFiles(const std::vector<FileID>& filesID, std::vector<Tags>& vTags)
  {
    //for (auto fileID : filesID) {
    //  Tags tags;
    //  GetFileTags(fileID, tags);
    //  vTags.emplace_back(tags);
    //}
    return true;
  }

  bool CivetStorage::GetClasses(const std::string& parent, nlohmann::json& classes)
  {
    //READ_BEGIN(TABLE_CLASS2FILE);
    //READ_BEGIN(TABLE_CLASS2HASH);
    //std::map<uint32_t, std::vector<FileID>> vFiles;
    //uint32_t parentID = 0;
    //std::string parentKey(ROOT_CLASS_PATH);
    //if (parent != ROOT_CLASS_PATH) {
    //  std::tie(parentID, parentKey) = EncodePath2Hash(parent);
    //  T_LOG("class", "panrent: %s, ID: %d, key: %s", parent.c_str(), parentID, format_x16(parentKey).c_str());
    //}
    //void* pData = nullptr;
    //uint32_t len = 0;
    //auto children = GetClassChildren(parentKey);
    ////std::string sParent = (parent == ROOT_CLASS_PATH ? "" : ROOT_CLASS_PATH);
    //for (auto& clsID : children) {
    //  nlohmann::json jCls;
    //  std::string name = GetClassByHash(clsID);
    //  jCls["name"] = name;
    //  uint32_t childID;
    //  std::string sChild;
    //  std::tie(childID, sChild) = EncodePath2Hash(name);
    //  if (childID == parentID) continue;
    //  auto clzChildren = GetClassChildren(sChild);
    //  if (clzChildren.size()) {
    //    nlohmann::json children;
    //    for (auto& clzID : clzChildren) {
    //      nlohmann::json clz;
    //      clz["id"] = clzID;
    //      std::string classPath = GetClassByHash(clzID);
    //      size_t pos = classPath.rfind('/');
    //      if (pos != std::string::npos) {
    //        clz["name"] = classPath.substr(pos + 1);
    //      }
    //      else {
    //        clz["name"] = classPath;
    //      }
    //      clz["count"] = GetAllFileCountOfClass(clzID);
    //      clz["type"] = "clz";
    //      children.push_back(clz);
    //    }
    //    jCls["children"] = children;
    //  }
    //  jCls["count"] = GetAllFileCountOfClass(childID);
    //  jCls["type"] = "clz";
    //  classes.push_back(jCls);
    //}
    //if (!classes.empty()) {
    //  T_LOG("class", "parent: %s(%s), get class: %d", parent.c_str(), parentKey.c_str(), classes.dump().c_str());
    //}
    return true;
  }

  bool CivetStorage::getClassesInfo(const std::string& parent, nlohmann::json& info)
  {
    //READ_BEGIN(TABLE_CLASS2FILE);
    //READ_BEGIN(TABLE_CLASS2HASH);
    //std::map<uint32_t, std::vector<FileID>> vFiles;
    //uint32_t parentID = 0;
    //std::string parentKey(ROOT_CLASS_PATH);
    //if (parent != ROOT_CLASS_PATH) {
    //  std::tie(parentID, parentKey) = EncodePath2Hash(parent);
    //  T_LOG("class", "panrent: %s, ID: %d, key: %s", parent.c_str(), parentID, format_x16(parentKey).c_str());
    //}
    //void* pData = nullptr;
    //uint32_t len = 0;
    //auto children = GetClassChildren(parentKey);
    ////std::string sParent = (parent == ROOT_CLASS_PATH ? "" : ROOT_CLASS_PATH);
    //for (auto& clsID : children) {
    //  nlohmann::json jCls;
    //  std::string name = GetClassByHash(clsID);
    //  jCls["name"] = name;
    //  uint32_t childID;
    //  std::string sChild;
    //  std::tie(childID, sChild) = EncodePath2Hash(name);
    //  if (childID == parentID) continue;
    //  auto clzChildren = GetClassChildren(sChild);
    //  auto files = GetFilesOfClass(childID);
    //  T_LOG("class", "clsID:%u, parentID: %u,  %d children %s, files: %s",
    //    clsID, parentID, childID, format_vector(clzChildren).c_str(), format_vector(files).c_str());
    //  if (clzChildren.size() || files.size()) {
    //    nlohmann::json children;
    //    for (auto& clzID : clzChildren) {
    //      nlohmann::json clz;
    //      clz["id"] = clzID;
    //      std::string classPath = GetClassByHash(clzID);
    //      size_t pos = classPath.rfind('/');
    //      if (pos != std::string::npos) {
    //        clz["name"] = classPath.substr(pos + 1);
    //      }
    //      else {
    //        clz["name"] = classPath;
    //      }
    //      clz["count"] = GetAllFileCountOfClass(clzID);
    //      clz["type"] = "clz";
    //      children.push_back(clz);
    //    }
    //    for (auto& fileID : files) {
    //      children.push_back(fileID);
    //    }
    //    jCls["children"] = children;
    //  }
    //  jCls["count"] = GetAllFileCountOfClass(childID);
    //  jCls["type"] = "clz";
    //  info.push_back(jCls);
    //}
    //m_pDatabase->Get(TABLE_CLASS2FILE, parentID, pData, len);
    //for (size_t idx = 0; idx < len / sizeof(FileID); ++idx) {
    //  FileID fid = *((FileID*)pData + idx);
    //  nlohmann::json jFile;
    //  jFile["id"] = fid;
    //  Snap snap = GetFileSnap(fid);
    //  jFile["name"] = std::get<1>(snap);
    //  info.push_back(jFile);
    //}
    //if (!info.empty()) {
    //  T_LOG("class", "parent: %s(%s), get class: %d", parent.c_str(), parentKey.c_str(), info.dump().c_str());
    //}
    return true;
  }

  bool CivetStorage::GetAllTags(TagTable& tags)
  {
    //READ_BEGIN(TABLE_TAG_INDX);
    //READ_BEGIN(TABLE_TAG2FILE);

    //m_pDatabase->Filter(TABLE_TAG_INDX, [this, &tags](const std::string& alphabet, void* pData, uint32_t len, void*& newVal, uint32_t& newLen)->bool {
    //  typedef std::tuple<std::string, std::string, std::vector<FileID>> TagInfo;
    //  WordIndex* pIndx = (WordIndex*)pData;
    //  size_t cnt = len / sizeof(WordIndex);
    //  std::vector<std::string> words = this->GetWordByIndex(pIndx, cnt);
    //  std::vector<std::vector<FileID>> vFilesID = this->GetFilesIDByTagIndex(pIndx, cnt);
    //  std::vector<TagInfo> vTags;
    //  for (size_t idx = 0; idx < words.size(); ++idx) {
    //    TagInfo tagInfo = std::make_tuple(alphabet, words[idx], vFilesID[idx]);
    //    vTags.emplace_back(tagInfo);
    //  }
    //  tags[alphabet[0]] = vTags;
    //  return false;
    //});
    std::string gql = "{query: '" TABLE_TAG "', from: '" GRAPH_NAME "'};";

    return true;
  }

  bool CivetStorage::UpdateFilesClasses(const std::vector<FileID>& filesID, const std::vector<std::string>& classes)
  {
    //return this->AddClasses(classes, filesID);
      return true;
  }

  bool CivetStorage::UpdateClassName(const std::string& oldName, const std::string& newName)
  {
    std::vector<std::string> vWords = split(newName, '/');
    std::vector<std::string> vOldWords = split(oldName, '/');


    //std::vector<std::string> names({ oldName, newName });
    //auto mIndexes = m_pDatabase->GetWordsIndex(names);
    //std::vector<WordIndex> vIndexes = transform(vWords, mIndexes);
    //std::string sNew = serialize(vIndexes);
    //T_LOG("class", "sNew: %s", format_x16(sNew).c_str());
    //if (IsClassExist(sNew)) return false;
    //WRITE_BEGIN();
    //auto oldPath = EncodePath2Hash(oldName);
    //auto newPath = EncodePath2Hash(newName);
    //void* pData = nullptr;
    //uint32_t len = 0;
    //// remove old
    //m_pDatabase->Get(TABLE_KEYWORD2CLASS, mIndexes[vOldWords[vOldWords.size() - 1]], pData, len);
    //std::vector<ClassID> vClassesID((ClassID*)pData, (ClassID*)pData + len / sizeof(ClassID));
    //m_pDatabase->Del(TABLE_KEYWORD2CLASS, mIndexes[vOldWords[vOldWords.size() - 1]]);
    //m_pDatabase->Get(TABLE_CLASS2FILE, oldPath.first, pData, len);
    //std::vector<FileID> vFilesID((FileID*)pData, (FileID*)pData + len/sizeof(FileID));
    //T_LOG("class", "remove, files: %s words: %s", format_vector(vFilesID).c_str(), format_vector(vOldWords).c_str());
    //RemoveKeywords(vFilesID, vOldWords);
    //// add new
    //m_pDatabase->Put(TABLE_KEYWORD2CLASS, mIndexes[vWords[vWords.size() - 1]], vClassesID.data(), vClassesID.size() * sizeof(ClassID));
    //T_LOG("class", "bind, files: %s words: %s", format_vector(vFilesID).c_str(), format_vector(vWords).c_str());
    //BindKeywordAndFile(vIndexes, vFilesID);
    //// update children class

    //T_LOG("class", "update keyword name from %s(%d) to %s(%d), children: %s",
    //  vOldWords[vOldWords.size() - 1].c_str(), mIndexes[vWords[vWords.size() - 1]],
    //  vWords[vWords.size() - 1].c_str(), mIndexes[vWords[vWords.size() - 1]],
    //  format_vector(vClassesID).c_str());

    //m_pDatabase->Get(TABLE_HASH2CLASS, oldPath.first, pData, len);
    //if (len) {
    //  T_LOG("class", "updated name %s(%d), len is %d", newName.c_str(), oldPath.first, newName.size());
    //  m_pDatabase->Put(TABLE_HASH2CLASS, oldPath.first, (void*)newName.data(), newName.size());
    //}
    //m_pDatabase->Get(TABLE_CLASS2HASH, oldPath.second, pData, len);
    //if (len) {
    //  std::vector<uint32_t> vHash((uint32_t*)pData, (uint32_t*)pData + len / sizeof(uint32_t));
    //  T_LOG("DEBUG", "key: %s, children: %s", format_x16(newPath.second).c_str(), format_vector(vHash).c_str());
    //  m_pDatabase->Put(TABLE_CLASS2HASH, newPath.second, vHash.data(), vHash.size() * sizeof(uint32_t));
    //}
    //UpdateChildrenClassName(oldPath.second, oldName, newName, vOldWords, vIndexes);

    //m_pDatabase->Del(TABLE_CLASS2HASH, oldPath.second);
    //WRITE_END();
    return true;
  }

  bool CivetStorage::UpdateFileMeta(const std::vector<FileID>& filesID, const nlohmann::json& mutation)
  {
    //WRITE_BEGIN();
    //auto existFiles = mapExistFiles(filesID);
    //void* pData = nullptr;
    //uint32_t len = 0;
    //using namespace nlohmann;
    //for (FileID fileID : existFiles)
    //{
    //  for (auto itr = mutation.begin(); itr != mutation.end(); ++itr) {
    //    T_LOG("file", "mutation item[%s]=%s", itr.key().c_str(), itr.value().dump().c_str());
    //    if (itr.key() == "filename") {
    //      // if mutaion is filename, update snap
    //      m_pDatabase->Get(TABLE_FILESNAP, fileID, pData, len);
    //      std::vector<uint8_t> s((uint8_t*)pData, (uint8_t*)pData + len);
    //      json jsn = json::from_cbor(s);
    //      jsn["name"] = itr.value().dump();
    //      s = json::to_cbor(jsn);
    //      m_pDatabase->Put(TABLE_FILESNAP, fileID, s.data(), s.size());
    //      // TODO: how to update keyword
    //    }
    //    m_pDatabase->Get(TABLE_FILE_META, fileID, pData, len);
    //    std::vector<uint8_t> sMeta((uint8_t*)pData, (uint8_t*)pData + len);
    //    json meta = json::from_cbor(sMeta);
    //    //T_LOG("file", "mutation meta: %s", meta.dump().c_str());
    //    for (auto& m : meta) {
    //      if (m["name"] == itr.key()) {
    //        m["value"] = itr.value();
    //        break;
    //      }
    //    }
    //    sMeta = json::to_cbor(meta);
    //    m_pDatabase->Put(TABLE_FILE_META, fileID, sMeta.data(), sMeta.size());
    //  }
    //}
    //WRITE_END();
    return true;
  }

  bool CivetStorage::InitBinaryTables()
  {
    //m_pDatabase->Filter(TABLE_MATCH_META, [&](const std::string& k, void* pData, uint32_t len, void*& newVal, uint32_t& newLen)->bool {
    //  std::vector<uint8_t> vInfo((uint8_t*)pData, (uint8_t*)pData + len);
    //  nlohmann::json info = nlohmann::json::from_cbor(vInfo);
    //  if (info["type"] == "bin") {
    //    _binTables.emplace_back(k);
    //  }
    //  return false;
    //});
    return true;
  }

  bool CivetStorage::CanBeQuery(const nlohmann::json& meta)
  {
    //std::string name = meta["name"];
    //if (m_pDatabase->GetMetaTable(name)) return true;
    //std::string type = meta["type"];
    //if (meta.contains("query") && meta["query"] == true && type != "bin") {
    //  return true;
    //}
    return false;
  }

  bool CivetStorage::Query(const std::string& query, std::vector<FileInfo>& filesInfo)
  {
    //RPN rpn(generate(query));
    ////rpn.debug();
    //T_LOG("query", "rpn generate finish");
    //std::stack<std::unique_ptr<ISymbol>> sSymbol;
    //for (auto itr = rpn.begin(); itr != rpn.end(); ++itr)
    //{
    //  switch ((*itr)->symbolType())
    //  {
    //  case Table:
    //    sSymbol.emplace(std::move(*itr));
    //    T_LOG("query", "emplace table");
    //    break;
    //  case Condition:
    //    T_LOG("query", "emplace condition");
    //    sSymbol.emplace(std::move(*itr));
    //    break;
    //  case Expression:
    //  {
    //    // pop stack
    //    T_LOG("query", "IExpression: %s", (*itr)->Value().c_str());
    //    std::unique_ptr<IExpression> pExpr = dynamic_unique_cast<IExpression>(std::move(*itr));
    //    if (pExpr->Value() == OP_In) { // array
    //      std::unique_ptr<ValueArray> pArray(new ValueArray);
    //      int loop = (pExpr->statementCount() == 0 ? sSymbol.size() : pExpr->statementCount());
    //      for (int idx =0; idx< loop; ++idx)
    //      {
    //        std::unique_ptr<ISymbol> pRight = std::move(sSymbol.top());
    //        T_LOG("query", "pop array");
    //        sSymbol.pop();
    //        std::unique_ptr< ValueInstance > pValue = dynamic_unique_cast<ValueInstance>(std::move(pRight));
    //        pArray->push(pValue);
    //      }
    //      T_LOG("query", "emplace array");
    //      sSymbol.emplace(std::move(pArray));
    //      T_LOG("query", "array count: %d", sSymbol.size());
    //    }
    //    else {
    //      T_LOG("query", "stack count: %d", sSymbol.size());
    //      std::unique_ptr<ISymbol> pLeft = std::move(sSymbol.top());
    //      T_LOG("query", "pop left value");
    //      sSymbol.pop();
    //      if (pLeft->symbolType() == Table) {
    //        std::unique_ptr<ISymbol> pRight = std::move(sSymbol.top());
    //        std::unique_ptr< ValueInstance > pValue = dynamic_unique_cast<ValueInstance>(std::move(pRight));
    //        T_LOG("query", "pop right value");
    //        sSymbol.pop();
    //        std::unique_ptr<ITableProxy> pLeftValue = dynamic_unique_cast<ITableProxy>(std::move(pLeft));
    //        auto result = caxios::Query(m_pDatabase, std::move(pExpr), std::move(pLeftValue), std::move(pValue));
    //        T_LOG("query", "emplace table result");
    //        sSymbol.emplace(std::move(result));
    //      }
    //      else { // And/Or
    //        std::unique_ptr<ValueInstance> pResult = dynamic_unique_cast<ValueInstance>(std::move(pLeft));
    //        while (sSymbol.size()!=0) { // pop all query result
    //          std::unique_ptr<ISymbol> pSymbol = std::move(sSymbol.top());
    //          sSymbol.pop();
    //          std::unique_ptr< ValueInstance > pValue = dynamic_unique_cast<ValueInstance>(std::move(pSymbol));
    //          if (pExpr->Value() == OP_And) pResult = caxios::Interset(std::move(pResult), std::move(pValue));
    //        }
    //        //auto result = caxios::Query(m_pDatabase, std::move(pExpr), std::move(pLeftValue), std::move(pArray));
    //        T_LOG("query", "emplace result");
    //        sSymbol.emplace(std::move(pResult));
    //      }
    //    }
    //  }
    //    break;
    //  default:
    //    break;
    //  }
    //}
    //T_LOG("query", "result count: %d", sSymbol.size());
    //if (sSymbol.size() == 0) return false;
    //auto result = std::move(sSymbol.top());
    //std::unique_ptr<ValueArray> array = dynamic_unique_cast<ValueArray>(std::move(result));
    //return GetFilesInfo(array->GetArray<FileID>(), filesInfo);
      return true;
  }

  void CivetStorage::CreateGQLTasks(const std::list<IGQLExecutor*>& task)
  {
    ClearTasks();
    _executors = task;
  }

  void CivetStorage::TryUpdate(const std::string& meta)
  {
    //if (m_pDatabase->TryUpdate()) {
    //  InitTable(meta);
    //  upgrade(m_pDatabase, CStorageProxy::_curVersion, SCHEMA_VERSION);
    //}
  }

  bool CivetStorage::AddFile(FileID fileid, const MetaItems& meta, const Keywords& keywords)
  {
    using namespace nlohmann;
    json data;
    for (MetaItem m : meta)
    {
      std::string key(m["name"]);
      std::string value(m["value"]);
      if (m["type"] == "int") {
        data[key] = atoi(value.c_str());
      }
      else if (m["type"] == "date") {
        size_t pos = value.find_first_of('.');
        value = value.substr(0, pos);
        data[key] = value;
      }
      else {
        data[key] = value;
      }
    }
    for (const std::string& kw : keywords) {
      data["keyword"].push_back(kw);
    }
    std::string gql = json2gql(data);
    gql = std::string("{upset: '") + TABLE_FILE + "', vertex: [[" + std::to_string(fileid) + ", " + gql + "]]};";
    T_LOG("CivetStorage", "%s", gql.c_str());
    execGQL(gql, nullptr);
    return true;
  }

  bool CivetStorage::AddFileID2Tag(const std::vector<FileID>& vFilesID, WordIndex index)
  {
    //void* pData = nullptr;
    //uint32_t len = 0;
    //std::vector<FileID> vID;
    //m_pDatabase->Get(TABLE_TAG2FILE, index, pData, len);
    //if (len) {
    //  std::vector<FileID> vTemp((FileID*)pData, (FileID*)pData + len/sizeof(FileID));
    //  addUniqueDataAndSort(vID, vTemp);
    //}
    //addUniqueDataAndSort(vID, vFilesID);
    //m_pDatabase->Put(TABLE_TAG2FILE, index, (void*)vID.data(), vID.size() * sizeof(FileID));
    return true;
  }

  bool CivetStorage::AddFileID2Keyword(FileID fileID, WordIndex keyword)
  {
    //void* pData = nullptr;
    //uint32_t len = 0;
    //m_pDatabase->Get(TABLE_KEYWORD2FILE, keyword, pData, len);
    //FileID* pIDs = (FileID*)pData;
    //size_t cnt = len / sizeof(FileID);
    //std::vector<FileID> vFilesID(pIDs, pIDs + cnt);
    //if (addUniqueDataAndSort(vFilesID, fileID)) return true;
    //T_LOG("keyword", "add fileID: %d, keyword: %d, current: %s", fileID, keyword, format_vector(vFilesID).c_str());
    //if (!m_pDatabase->Put(TABLE_KEYWORD2FILE, keyword, &(vFilesID[0]), sizeof(FileID) * vFilesID.size())) return false;
    return true;
  }

  void CivetStorage::UpdateChildrenClassName(const std::string& clssKey, const std::string& oldParentName, const std::string& newParentName,
    const std::vector<std::string>& oldStrings, const std::vector<WordIndex>& vWordsIndex)
  {
    void* pData = nullptr;
    uint32_t len = 0;
    //auto children = GetClassChildren(clssKey);
    //for (ClassID cid : children) {
    //  m_pDatabase->Get(TABLE_HASH2CLASS, cid, pData, len);
    //  if (len) {
    //    std::string oldPath((char*)pData, (char*)pData + len);
    //    std::string clsName = oldPath;
    //    auto start = clsName.begin();
    //    clsName.replace(start, start + oldParentName.size(), newParentName);
    //    m_pDatabase->Put(TABLE_HASH2CLASS, cid, (void*)clsName.data(), clsName.size());
    //    // Update hash key -> class
    //    auto newKey = GetClassKey(cid);
    //    T_LOG("class", "updated name %s(%d) %s,  len is %d", clsName.c_str(), cid, format_x16(newKey).c_str(), clsName.size());
    //    UpdateChildrenClassName(oldPath, oldParentName, newParentName,oldStrings, vWordsIndex);
    //    // bind class hash 2 key
    //    auto oldKey = GetClassKey(oldPath);
    //    m_pDatabase->Get(TABLE_CLASS2HASH, oldKey, pData, len);
    //    m_pDatabase->Put(TABLE_CLASS2HASH, newKey, pData, len);
    //    m_pDatabase->Del(TABLE_CLASS2HASH, oldKey);
    //  }
    //  m_pDatabase->Get(TABLE_CLASS2FILE, cid, pData, len);
    //  std::vector<FileID> vFiles((FileID*)pData, (FileID*)pData + len / sizeof(FileID));
    //  RemoveKeywords(vFiles, oldStrings);
    //  BindKeywordAndFile(vWordsIndex, vFiles);
    //}
  }

  bool CivetStorage::AddKeyword2File(WordIndex keyword, FileID fileID)
  {
    void* pData = nullptr;
    uint32_t len = 0;
    //m_pDatabase->Get(TABLE_FILE2KEYWORD, fileID, pData, len);
    //T_LOG("keyword", "add keyword Index: %d, len: %d", keyword, len);
    //WordRef wref = { keyword , 1 };
    //WordRef* pWords = (WordRef*)pData;
    //size_t cnt = len / sizeof(WordRef);
    //std::vector<WordRef> vWordIndx(pWords, pWords + cnt);
    //if (addUniqueDataAndSort(vWordIndx, wref)) return true;
    //T_LOG("keyword", "add keywords: %d, fileID: %d", keyword, fileID);
    //if (!m_pDatabase->Put(TABLE_FILE2KEYWORD, fileID, &(vWordIndx[0]), sizeof(WordRef) * vWordIndx.size())) return false;
    return true;
  }

  void CivetStorage::BindKeywordAndFile(WordIndex wid, FileID fid)
  {
      //this->AddKeyword2File(wid, fid);
      //this->AddFileID2Keyword(fid, wid);
  }

  void CivetStorage::BindKeywordAndFile(const std::vector<WordIndex>& mIndexes, const std::vector<FileID>& existID)
  {
    //for (auto fid : existID) {
    //  for (auto& item : mIndexes) {
    //    BindKeywordAndFile(item, fid);
    //  }
    //}
  }

  bool CivetStorage::AddTagPY(const std::string& tag, WordIndex indx)
  {
    //T_LOG("tag", "input py: %s", tag.c_str());
    //std::wstring wTag = string2wstring(tag);
    //T_LOG("tag", "input wpy: %ls, %d", wTag.c_str(), wTag[0]);
    //auto py = getLocaleAlphabet(wTag[0]);
    //T_LOG("tag", "Pinyin: %ls -> %ls", wTag.c_str(), py[0].c_str());
    //WRITE_BEGIN();
    //std::set<WordIndex> sIndx;
    //void* pData = nullptr;
    //uint32_t len = 0;
    //if (m_pDatabase->Get(TABLE_TAG_INDX, py[0], pData, len)) {
    //  WordIndex* pWords = (WordIndex*)pData;
    //  size_t cnt = len / sizeof(WordIndex);
    //  sIndx = std::set<WordIndex>(pWords, pWords + cnt);
    //}
    //sIndx.insert(indx);
    //std::vector< WordIndex> vIndx(sIndx.begin(), sIndx.end());
    //m_pDatabase->Put(TABLE_TAG_INDX, py[0], vIndx.data(), vIndx.size() * sizeof(WordIndex));
    //WRITE_END();
    return true;
  }

  bool CivetStorage::AddClass2FileID(uint32_t key, const std::vector<FileID>& vFilesID)
  {
    //std::vector<FileID> vFiles(vFilesID);
    ////std::sort(vFiles.begin(), vFiles.end());
    //void* pData = nullptr;
    //uint32_t len = 0;
    //m_pDatabase->Get(TABLE_CLASS2FILE, key, pData, len);
    //std::vector<FileID> vID;
    //if (len) {
    //  vID.assign((FileID*)pData, (FileID*)pData + len / sizeof(FileID));
    //}
    //addUniqueDataAndSort(vID, vFilesID);
    //T_LOG("class", "add files to class %d: %s", key, format_vector(vID).c_str());
    //m_pDatabase->Put(TABLE_CLASS2FILE, key, (void*)vID.data(), vID.size() * sizeof(FileID));
    return true;
  }

  bool CivetStorage::AddFileID2Class(const std::vector<FileID>& filesID, uint32_t clsID)
  {
    //for (auto fileID : filesID) {
    //  void* pData = nullptr;
    //  uint32_t len = 0;
    //  m_pDatabase->Get(TABLE_FILE2CLASS, fileID, pData, len);
    //  std::vector<uint32_t> vClasses;
    //  if (len) {
    //    vClasses.assign((uint32_t*)pData, (uint32_t*)pData + len / sizeof(uint32_t));
    //  }
    //  addUniqueDataAndSort(vClasses, clsID);
    //  T_LOG(TABLE_FILE2CLASS, "add class to file, fileID: %u, class: %s, new class: %u", fileID, format_vector(vClasses).c_str(), clsID);
    //  m_pDatabase->Put(TABLE_FILE2CLASS, fileID, vClasses.data(), vClasses.size()*sizeof(uint32_t));
    //}
    return true;
  }

  std::vector<uint32_t> CivetStorage::AddClassImpl(const std::vector<std::string>& classes)
  {
    std::vector<uint32_t> vClasses;
    //auto mIndexes = m_pDatabase->GetWordsIndex(classes);
    //for (auto& clazz : classes) {
    //  std::vector<std::string> vTokens = split(clazz, '/');
    //  std::vector<WordIndex> vClassPath;
    //  std::for_each(vTokens.begin(), vTokens.end(), [&mIndexes, &vClassPath](const std::string& token) {
    //    T_LOG("class", "class token: %s, %d", token.c_str(), mIndexes[token]);
    //    vClassPath.emplace_back(mIndexes[token]);
    //  });
    //  std::string sChild = serialize(vClassPath);
    //  T_LOG("class", "serialize result: %s", format_x16(sChild).c_str());
    //  ClassID child = GenerateClassHash(sChild);
    //  if (child == 0) {
    //    T_LOG("class", "error: %s", clazz.c_str());
    //    continue;
    //  }
    //  vClasses.emplace_back(child);
    //  MapHash2Class(child, clazz);
    //  uint32_t parent = ROOT_CLASS_ID;
    //  std::string sParent(ROOT_CLASS_PATH);
    //  void* pData = nullptr;
    //  uint32_t len = 0;
    //  for (auto& clsPath : vClassPath) {
    //    // add word map to class hash
    //    m_pDatabase->Get(TABLE_KEYWORD2CLASS, clsPath, pData, len);
    //    std::vector<ClassID> vClassesID((ClassID*)pData, (ClassID*)pData + len / sizeof(ClassID));
    //    addUniqueDataAndSort(vClassesID, child);
    //    m_pDatabase->Put(TABLE_KEYWORD2CLASS, clsPath, vClassesID.data(), vClassesID.size() * sizeof(ClassID));
    //    T_LOG("class", "add keyword %s(%d) map to %s ", clazz.c_str(), clsPath, format_vector(vClassesID).c_str());
    //  }
    //  if (vClassPath.size() > 0) {
    //    vClassPath.pop_back();
    //    if (vClassPath.size() != 0) {
    //      sParent = serialize(vClassPath);
    //      parent = GenerateClassHash(sParent);
    //    }
    //    T_LOG("class", "parent(%llu): %s, %u", vClassPath.size(),(sParent).c_str(), parent);
    //  }
    //  // update parent 
    //  m_pDatabase->Get(TABLE_CLASS2HASH, sChild, pData, len);
    //  if (len == 0) {
    //    // add . and ..
    //    InitClass(sChild, child, parent);
    //    T_LOG("class", "add class [%s] to hash [%u(%s), %u]", format_x16(sChild).c_str(), child, clazz.c_str(), parent);
    //  }
    //  else {
    //    std::vector<ClassID> cIDs((ClassID*)pData, (ClassID*)pData + len / sizeof(ClassID));
    //    T_LOG("class", "class children: %s", format_vector(cIDs).c_str());
    //  }
    //  m_pDatabase->Get(TABLE_CLASS2HASH, sParent, pData, len);
    //  std::vector<uint32_t> children(len + 1);
    //  if (len) {
    //    children.assign((uint32_t*)pData + CHILD_START_OFFSET, (uint32_t*)pData + len / sizeof(uint32_t));
    //  }
    //  addUniqueDataAndSort(children, child);
    //  if (len) {
    //    children.insert(children.begin(), (uint32_t*)pData, (uint32_t*)pData + CHILD_START_OFFSET);
    //  }
    //  T_LOG("class", "add class to class, parent: %s(%d), current: %d, name: %s(%s), children: %s",
    //    format_x16(sParent).c_str(), parent, child, clazz.c_str(), format_x16(sChild).c_str(), format_vector(children).c_str());
    //  T_LOG("DEBUG", "TABLE_CLASS2HASH, key: %s, children: %s", format_x16(sParent).c_str(), format_vector(children).c_str());
    //  m_pDatabase->Put(TABLE_CLASS2HASH, sParent, children.data(), children.size() * sizeof(uint32_t));
    //  
    //}
    return std::move(vClasses);
  }

  bool CivetStorage::RemoveFile(FileID fileID)
  {
    using namespace nlohmann;

    std::string sID = std::to_string(fileID);
    std::string gql = "{remove: '" TABLE_FILE "', where: {id: " + sID + "}};";

    execGQL(gql, nullptr);
    return true;
  }

  void CivetStorage::RemoveFile(FileID fileID, const std::string& file2type, const std::string& type2file)
  {
    //void* pData = nullptr;
    //uint32_t len = 0;
    //T_LOG("file", "remove file 0");
    //m_pDatabase->Get(file2type, fileID, pData, len);
    ////if (len == 0) return;
    //T_LOG("file", "remove file 1");
    //std::vector<WordIndex> vTypesID((WordIndex*)pData, (WordIndex*)pData + len / sizeof(WordIndex));
    //for (auto typeID : vTypesID)
    //{
    //  m_pDatabase->Get(type2file, typeID, pData, len);
    //  T_LOG("file", "remove file type: %d", typeID);
    //  std::vector<FileID> vFiles((FileID*)pData, (FileID*)pData + len / sizeof(FileID));
    //  eraseData(vFiles, fileID);
    //  m_pDatabase->Put(type2file, typeID, vFiles.data(), vFiles.size() * sizeof(FileID));
    //}
    //T_LOG("file", "remove file: %d", fileID);
    //m_pDatabase->Del(file2type, fileID);
  }

  bool CivetStorage::RemoveTag(FileID fileID, const Tags& tags)
  {
    //READ_BEGIN(TABLE_FILE2TAG);
    //void* pData = nullptr;
    //uint32_t len = 0;
    //m_pDatabase->Get(TABLE_FILE2TAG, fileID, pData, len);
    //if (len == 0) return true;
    //auto indexes = m_pDatabase->GetWordsIndex(tags);
    //WordIndex* pWordIndx = (WordIndex*)pData;
    //size_t cnt = len / sizeof(WordIndex);
    //for (auto& item : indexes)
    //  for (size_t idx = 0; idx < cnt; ++idx) {
    //  {
    //    if (pWordIndx[idx] == item.second) {
    //      pWordIndx[idx] = 0;
    //      break;
    //    }
    //  }
    //}
    //m_pDatabase->Put(TABLE_FILE2TAG, fileID, pData, len);
    return true;
  }

  bool CivetStorage::RemoveClassImpl(const std::string& classPath)
  {
    if (classPath == "/") return false;
    //ClassID clsID = ROOT_CLASS_ID;
    //std::string clsKey(ROOT_CLASS_PATH);
    //std::vector<std::string> vWords;
    //auto vW = split(classPath, '/');
    //addUniqueDataAndSort(vWords, vW);
    //auto mIndexes = m_pDatabase->GetWordsIndex(vWords);
    //std::vector<WordIndex> vIndexes = transform(vW, mIndexes);
    //clsKey = serialize(vIndexes);
    //clsID = GetClassHash(clsKey);
    //T_LOG("class", "trace  classID: %d, key: %s", clsID, format_x16(clsKey).c_str());
    //auto children = GetClassChildren(clsKey); // use TABLE_CLASS2HASH
    //for (auto child : children) {
    //  if (child == clsID) continue;
    //  std::string clsPath = GetClassByHash(child);
    //  T_LOG("DEBUG", "children: %s, %d", clsPath.c_str(), child);
    //  RemoveClassImpl(clsPath);
    //}
    //// children is empty now, DEL it
    //// clean: TABLE_KEYWORD2CLASS, TABLE_HASH2CLASS, TABLE_CLASS2HASH, TABLE_FILE2CLASS, TABLE_CLASS2FILE,
    //// 1. clean TABLE_FILE2CLASS
    //void* pData = nullptr;
    //uint32_t len = 0;
    //m_pDatabase->Get(TABLE_CLASS2FILE, clsID, pData, len);
    //if (len) {
    //  std::vector<FileID> vFilesID((FileID*)pData, (FileID*)pData + len / sizeof(FileID));
    //  // remove file to class
    //  for (FileID fid : vFilesID) {
    //    m_pDatabase->Get(TABLE_FILE2CLASS, fid, pData, len);
    //    if (len) {
    //      std::vector<ClassID> vSrcID((ClassID*)pData, (ClassID*)pData + len / sizeof(ClassID));
    //      eraseData(vSrcID, clsID);
    //      m_pDatabase->Put(TABLE_FILE2CLASS, fid, vSrcID.data(), vSrcID.size() * sizeof(ClassID));
    //    }
    //  }
    //}
    ////T_LOG("class", "assert %s, %d", classPath.c_str(), clsID);
    ////assert(classPath != "/" && clsID != 0);
    //T_LOG("class", "remove %s(%d, %s)", classPath.c_str(), clsID, format_x16(clsKey).c_str());
    //// remove it from parent: TABLE_CLASS2HASH
    //ClassID parentID = GetClassParent(clsKey);
    //auto parentKey = GetClassKey(parentID);
    //m_pDatabase->Get(TABLE_CLASS2HASH, parentKey, pData, len);
    //if (len > CHILD_START_OFFSET * sizeof(ClassID)) {
    //  std::vector<ClassID> parentChildren((ClassID*)pData + CHILD_START_OFFSET, (ClassID*)pData + len / sizeof(ClassID));
    //  T_LOG("class", "remove %d from parent %d(%s), children: %s ", clsID, parentID, parentKey.c_str(), format_vector(parentChildren).c_str());
    //  eraseData(parentChildren, clsID);
    //  parentChildren.insert(parentChildren.begin(), (ClassID*)pData, (ClassID*)pData + CHILD_START_OFFSET);
    //  T_LOG("DEBUG", "TABLE_CLASS2HASH, key: %s, children: %s", format_x16(parentKey).c_str(), format_vector(parentChildren).c_str());
    //  m_pDatabase->Put(TABLE_CLASS2HASH, parentKey, parentChildren.data(), parentChildren.size() * sizeof(ClassID));
    //}
    //// 2. clean TABLE_CLASS2FILE
    //m_pDatabase->Del(TABLE_CLASS2FILE, clsID);
    //// 3. clean TABLE_HASH2CLASS
    //m_pDatabase->Del(TABLE_HASH2CLASS, clsID);
    //// 4. clean TABLE_CLASS2HASH
    //m_pDatabase->Del(TABLE_CLASS2HASH, clsKey);
    //// 5. TODO: clean TABLE_KEYWORD2CLASS
    
    // TODO: remove keyword but same tag how to process?
    return true;
  }

  bool CivetStorage::RemoveKeywords(const std::vector<FileID>& filesID, const std::vector<std::string>& keyword)
  {
    std::string sKeyword;
    for (const std::string& kw : keyword) {
      sKeyword += "'" + kw + "',";
    }
    sKeyword.pop_back();

    for (FileID fid : filesID) {
      std::string gql = "{remove: '" TABLE_RELATION_KEYWORD "', where: [" + std::to_string(fid) + ", --, [" + sKeyword + "]]};";
      execGQL(gql, nullptr);
    }
    return true;
  }

  bool CivetStorage::GetFileInfo(FileID fileID, MetaItems& meta, Keywords& keywords, Tags& tags, Annotations& anno)
  {
    // meta
    return true;
  }

  bool CivetStorage::GetFileTags(FileID fileID, Tags& tags)
  {
    //READ_BEGIN(TABLE_FILE2TAG);
    //void* pData = nullptr;
    //uint32_t len = 0;
    //if (!m_pDatabase->Get(TABLE_FILE2TAG, fileID, pData, len)) return true;
    //WordIndex* pWordIndx = (WordIndex*)pData;
    //size_t cnt = len / sizeof(WordIndex);
    //tags = GetWordByIndex(pWordIndx, cnt);
    return true;
  }

  std::vector<caxios::FileID> CivetStorage::GetFilesByClass(const std::vector<WordIndex>& clazz)
  {
    std::vector<caxios::FileID> vFilesID;
    std::string sPath = serialize(clazz);
    //void* pData = nullptr;
    //uint32_t len = 0;
    //m_pDatabase->Get(TABLE_CLASS2FILE, sPath, pData, len);
    //if (len == 0) {
    //  T_LOG("file", "get files by class error: %s", format_vector(clazz).c_str());
    //}
    //else {
    //  vFilesID.assign((FileID*)pData, (FileID*)pData + len / sizeof(FileID));
    //}
    return std::move(vFilesID);
  }

  bool CivetStorage::IsFileExist(FileID fileID)
  {
    void* pData = nullptr;
    uint32_t len = 0;
    //m_pDatabase->Get(TABLE_FILESNAP, fileID, pData, len);
    //if (len > 0) return true;
    return false;
  }

  bool CivetStorage::IsClassExist(const std::string& clazz)
  {
    void* pData = nullptr;
    uint32_t len = 0;
    //m_pDatabase->Get(TABLE_CLASS2HASH, clazz, pData, len);
    //if (len == 0) return false;
    return true;
  }

  uint32_t CivetStorage::GenerateClassHash(const std::string& clazz)
  {
    //if (IsClassExist(clazz)) return GetClassHash(clazz);
    //auto e = encode(clazz);
    //uint32_t offset = 1;
    //do {
    //  void* pData = nullptr;
    //  uint32_t len = 0;
    //  m_pDatabase->Get(TABLE_HASH2CLASS, e, pData, len);
    //  if (len == 0) {
    //    break;
    //  }
    //  e = createHash(e, offset);
    //} while (++offset);
    //T_LOG("class", "hash: %u", e);
    //return e;
  }

  ClassID CivetStorage::GetClassHash(const std::string& clazz)
  {
    void* pData = nullptr;
    //uint32_t len = 0;
    //m_pDatabase->Get(TABLE_CLASS2HASH, clazz, pData, len);
    //if (len == 0) return 0;
    return *(ClassID*)pData;
  }

  uint32_t CivetStorage::GetClassParent(const std::string& clazz)
  {
    void* pData = nullptr;
    //uint32_t len = 0;
    //if (!m_pDatabase->Get(TABLE_CLASS2HASH, clazz, pData, len)) return 0;
    return *((uint32_t*)pData+1);
  }

  std::pair<uint32_t, std::string> CivetStorage::EncodePath2Hash(const std::string& classPath)
  {
    if (classPath == "/") return std::pair<uint32_t, std::string>({ ROOT_CLASS_ID, ROOT_CLASS_PATH });
    //std::vector<std::string> vWords = split(classPath, '/');
    //auto mIndexes = m_pDatabase->GetWordsIndex(vWords);
    //std::vector<WordIndex> vIndexes = transform(vWords, mIndexes);
    //std::string sKey = serialize(vIndexes);
    //uint32_t hash = GenerateClassHash(sKey);
    //T_LOG("class", "encode path: %s to (%u, %s)", classPath.c_str(), hash, format_x16(sKey).c_str());
    //return std::make_pair(hash, sKey);
  }

  std::vector<ClassID> CivetStorage::GetClassChildren(const std::string& clazz)
  {
    std::vector<ClassID> vChildren;
    //void* pData = nullptr;
    //uint32_t len = 0;
    //m_pDatabase->Get(TABLE_CLASS2HASH, clazz, pData, len);
    //size_t cnt = len / sizeof(ClassID);
    //if (cnt > CHILD_START_OFFSET) {
    //  T_LOG("class", "%s children count: %d", clazz.c_str(), cnt);
    //  vChildren.assign((ClassID*)pData + CHILD_START_OFFSET, (ClassID*)pData + cnt);
    //}
    //T_LOG("class", "%s[%d] children: %s", clazz.c_str(), cnt, format_vector(vChildren).c_str());
    return std::move(vChildren);
  }

  std::string CivetStorage::GetClassByHash(uint32_t hs)
  {
    std::string cls(ROOT_CLASS_PATH);
    //if (hs != 0) {
    //  void* pData = nullptr;
    //  uint32_t len = 0;
    //  m_pDatabase->Get(TABLE_HASH2CLASS, hs, pData, len);
    //  if (len) {
    //    cls.assign((char*)pData, (char*)pData + len);
    //  }
    //}
    return std::move(cls);
  }

  std::string CivetStorage::GetClassKey(ClassID hs)
  {
    //std::string str = GetClassByHash(hs);
    //return GetClassKey(str);
  }

  std::string CivetStorage::GetClassKey(const std::string& clsPath)
  {
    if (clsPath == "/") return clsPath;
    //auto vWords = split(clsPath, '/');
    //auto token = m_pDatabase->GetWordsIndex(vWords);
    //std::vector<WordIndex> vIndexes = transform(vWords, token);
    //return serialize(vIndexes);
  }

  std::vector<caxios::FileID> CivetStorage::GetFilesOfClass(uint32_t clsID)
  {
    std::vector<caxios::FileID> vFilesID;
    //void* pData = nullptr;
    //uint32_t len = 0;
    //m_pDatabase->Get(TABLE_CLASS2FILE, clsID, pData, len);
    //for (size_t idx = 0; idx < len / sizeof(FileID); ++idx) {
    //  FileID fid = *((FileID*)pData + idx);
    //  vFilesID.push_back(fid);
    //}
    //m_pDatabase->Filter(TABLE_CLASS2FILE, [](uint32_t k, void* pData, uint32_t len, void*& newVal, uint32_t& newLen)->bool {
    //  std::vector<FileID> vFiles((FileID*)pData,(FileID*)pData + len/sizeof(FileID));
    //  T_LOG("class", "view result: %u, %s", k, format_vector(vFiles).c_str());
    //  return false;
    //  });
    return vFilesID;
  }

  nlohmann::json CivetStorage::ParseMeta(const std::string& meta)
  {
    nlohmann::json jsn = nlohmann::json::parse(meta);
    // looks like: {group name: ['index 1', 'index 2']}
    nlohmann::json group;
    for (auto item : jsn)
    {
      //T_LOG("query", "schema item: %s", item.dump().c_str());
      std::string name = trunc(item["name"].dump());
      group[TABLE_FILE].push_back(name);
      if (item["query"] == true) {
        group["index"].push_back(name);
      }
    }
    T_LOG("construct", "schema: %s", meta.c_str());
    return group;
  }

  void CivetStorage::SetSnapStep(FileID fileID, int offset, bool bSet)
  {
    //nlohmann::json jSnap;
    //int step = GetSnapStep(fileID, jSnap);
    //bool bs = step & (1 << offset);
    //// T_LOG("snap", "step: %d, offset: %d, result: %d", step, offset, bs);
    //if (bs == bSet) return;
    //if (bSet) {
    //  step |= (1 << offset);
    //}
    //else {
    //  step &= ~(1 << offset);
    //}
    //jSnap["step"] = step;
    //// std::string snap = jSnap.dump();
    //auto snap = nlohmann::json::to_cbor(jSnap);
    //// T_LOG("snap", "put snap value: file %d, %d, bit %d", fileID, step, offset);
    //m_pDatabase->Put(TABLE_FILESNAP, fileID, (void*)(&snap[0]), snap.size());
  }

  char CivetStorage::GetSnapStep(FileID fileID, nlohmann::json& jSnap)
  {
    //void* pData = nullptr;
    //uint32_t len = 0;
    //m_pDatabase->Get(TABLE_FILESNAP, fileID, pData, len);
    //if (len == 0) return 0;
    //std::vector<uint8_t> snap((uint8_t*)pData, (uint8_t*)pData + len);
    //// T_LOG("snap", "snap: %s", snap.c_str());
    //jSnap = nlohmann::json::from_cbor(snap);
    //// jSnap = nlohmann::json::parse(snap);
    //int step = atoi(jSnap["step"].dump().c_str());
    //return step;
  }

  Snap CivetStorage::GetFileSnap(FileID fileID)
  {
    Snap snap;
    void* pData = nullptr;
    uint32_t len = 0;
    //m_pDatabase->Get(TABLE_FILESNAP, fileID, pData, len);
    //if (len) {
    //  std::vector<uint8_t> tmp((uint8_t*)pData, (uint8_t*)pData + len);
    //  nlohmann::json jSnap = nlohmann::json::from_cbor(tmp);
    //  std::get<2>(snap) = atoi(jSnap["step"].dump().c_str());
    //  std::get<0>(snap) = fileID;
    //  std::get<1>(snap) = trunc(jSnap["value"].dump());
    //}
    return std::move(snap);
  }

  WordIndex CivetStorage::GetWordIndex(const std::string& word)
  {
    void* pData = nullptr;
    uint32_t len = 0;
    //if (!m_pDatabase->Get(TABLE_KEYWORD_INDX, word, pData, len)) return 0;
    return *(WordIndex*)pData;
  }

  std::vector<std::vector<FileID>> CivetStorage::GetFilesIDByTagIndex(const WordIndex* const wordsIndx, size_t cnt)
  {
    std::vector<std::vector<FileID>> vFilesID;
    //for (size_t idx = 0; idx < cnt; ++idx) {
    //  WordIndex wordIdx = wordsIndx[idx];
    //  void* pData = nullptr;
    //  uint32_t len = 0;
    //  if(!m_pDatabase->Get(TABLE_TAG2FILE, wordIdx, pData, len)) continue;
    //  FileID* pFileID = (FileID*)pData;
    //  std::vector<FileID> filesID(pFileID, pFileID + len / sizeof(FileID));
    //  vFilesID.emplace_back(filesID);
    //}
    return std::move(vFilesID);
  }

  void CivetStorage::ClearTasks()
  {
    for (auto ptr : _executors) {
      delete ptr;
    }
    _executors.clear();
  }

}