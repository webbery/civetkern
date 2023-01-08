#include "db_manager.h"
#include <cstdint>
#include <iostream>
#include "log.h"
#include "util/util.h"
#include <set>
#include <string>
#include <utility>
#include <deque>
#include "util/pinyin.h"
#include <stack>
#include <vector>
#include "upgrader.h"

#define TABLE_SCHEMA        "dbinfo"
#define TABLE_FILESNAP      "file_snap"
#define TABLE_FILE          "file_meta"
#define TABLE_KEYWORD       "keywords"
#define TABLE_TAG           "tags"
#define TABLE_CLASS         "classes"
#define TABLE_RELATION_KEYWORD "kw_rel"
#define TABLE_RELATION_CLASS_FILE "cls_rel_file"  // file id -- cls id
#define TABLE_RELATION_CLASS "cls_rel_cls"      // parent id -- child id
#define TABLE_RELATION_TAG  "tag_rel"
#define TABLE_CLASS_TITLE   "name"
#define TABLE_COUNT         "count"         // about count of some statistic
#define TABLE_ANNOTATION    "annotation"
#define TABLE_MATCH_META    "match_meta"
#define TABLE_MATCH         "match_t"
#define TABLE_RECYCLE_ID    "recycle"
#define GRAPH_NAME          "civet"

#define SCHEMA_VERSION    2

#define CHILD_START_OFFSET  2

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
    return false;
  }


  int gqlite_callback_query(gqlite_result* result, void* handle) {
    if (result->errcode != ECode_Success) {
      printf("GQL ERROR: %s\n", result->infos[0]);
      return result->errcode;
    }
    auto a = static_cast<std::function<void(gqlite_result*)>*>(handle);
    (*a)(result);
    return false;
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
  }

  void CivetStorage::execGQL(const std::string& gql) {
    char* ptr = nullptr;
    gqlite_exec(_pHandle, gql.c_str(), gqlite_callback_func, nullptr, &ptr);
    if (ptr) gqlite_free(ptr);
  }

  void CivetStorage::execGQL(const std::string& gql, std::function<void(gqlite_result*)> func)
  {
    char* ptr = nullptr;
    gqlite_exec(_pHandle, gql.c_str(), gqlite_callback_query, &func, &ptr);
    if (ptr) gqlite_free(ptr);
  }

  void CivetStorage::InitTable(const std::string& meta) {
    auto group = ParseMeta(meta);
    std::string groupTag = "{" TABLE_TAG ": 'name' }";
    std::string groupClass = "{" TABLE_CLASS ": '" TABLE_CLASS_TITLE "' }";
    std::string groupKeyword = "{" TABLE_KEYWORD ": 'name' }";
    std::string groupSnap = "{" TABLE_FILESNAP ": ['fid', 'name', 'step'] }";
    std::string tagRelation = "['" TABLE_FILE "','" TABLE_RELATION_TAG "', '" TABLE_TAG "']";
    std::string clsFileRelation = "['" TABLE_FILE "', '" TABLE_RELATION_CLASS_FILE "', '" TABLE_CLASS "']";
    std::string clsRelation = "['" TABLE_CLASS "', '" TABLE_RELATION_CLASS "', '" TABLE_CLASS "']";
    std::string keyordRelation = "['" TABLE_FILE "', '" TABLE_RELATION_KEYWORD "', '" TABLE_KEYWORD "']";
    std::string gql = std::string("{create: '" GRAPH_NAME "', group: [") + group.dump()
      + ", " + groupTag + ", " + groupClass + ", " + groupKeyword + ", " + groupSnap
      + ", " + tagRelation + ", " + clsFileRelation + ", " + keyordRelation + ", " + clsRelation
      + "]};";
    /*
      {create: 'civet', group: [
        {file_meta:['color','size','path','filename','type','datetime','addtime','width','height'],
          index:['color','size','type','datetime','addtime']},
        'tag', 'clazz', 'kw'
       ]};
    */
    gql = caxios::normalize(gql);
    execGQL(gql);
  }

  bool CivetStorage::AddClasses(const std::vector<std::string>& classes, const std::vector<FileID>& filesID)
  {
    if (_flag == ReadOnly || classes.size() == 0) return false;
    auto classesID = AddClassImpl(classes);

    for (FileID fid : filesID) {
      std::string gqlClear = std::string("{remove: '") + TABLE_RELATION_CLASS_FILE + "', edge: [" + std::to_string(fid) + ", --, *]};";
      execGQL(gqlClear);

      for (auto& clsID : classesID) {
        std::string gqlUpset = std::string("{upset: '") + TABLE_RELATION_CLASS_FILE + "', edge: [" + std::to_string(fid) + ", --, " + std::to_string(clsID) + "]};";
        execGQL(gqlUpset);
      }

      std::string gqlSnap = "{query: '" TABLE_FILESNAP "', in: '" GRAPH_NAME "', where: {id: " + std::to_string(fid) + "}};";

      uint8_t stepBit = 0;
      execGQL(gqlSnap, [&](gqlite_result* result) {
        nlohmann::json snap = nlohmann::json::parse(result->nodes->_vertex->properties);
        stepBit = (uint8_t)snap["step"];
        });

      std::string gqlUpsetSnap = "{upset: '" TABLE_FILESNAP "', property: [{step: " + std::to_string(stepBit | BIT_CLASS) + "}], where: {id: "
        + std::to_string(fid) + "}};";
      execGQL(gqlUpsetSnap);

      for (auto& s: classes) {
        std::string gqlKeyword = "{upset: '" TABLE_RELATION_KEYWORD "', edge: [" + std::to_string(fid) + ", --, '" + s + "']};";
        // printf("upset keyword: %s", gqlKeyword.c_str());
        execGQL(gqlKeyword);
      }
    }
    return true;
  }

  bool CivetStorage::AddClasses(const std::vector<std::string>& classes)
  {
    if (classes.size() == 0) return false;
    
    this->AddClassImpl(classes);
    return true;
  }

  bool CivetStorage::AddMeta(const std::vector<FileID>& files, const nlohmann::json& meta)
  {
    std::string name, type;
    std::string value;
    if (meta.size() == 1) {
      auto item = meta.begin();
      name = item.key();
      value = caxios::normalize(item.value().dump());
    } else {
      name = meta["name"];
      type = meta["type"];
      if (type == "color") {
        value = caxios::normalize(meta["value"].dump());
      }
      else if (type == "bin") {
        std::string s(meta["value"]);
        std::vector<uint8_t> bin(s.begin(), s.end());
        value = "0b'" + caxios::base64_encode(bin) + "'";
      }
      else if (type == "date") {
        value = meta["value"];
      }
      else {
        value = "'" + std::string(meta["value"]) + "'";
      }
    }
    for (FileID fid : files) {
      std::string gql = "{upset: '" TABLE_FILE "', property: [{" + name + ":" + value + "}], where: {id: " + std::to_string(fid) + "}};";
      execGQL(gql);
      // printf("upset gql: %s\n", gql.c_str());

      if (name == "tag" || name == "keyword" || name == "class") {
        std::string gqlKeyword = "{upset: '" TABLE_RELATION_KEYWORD "', edge: [" + std::to_string(fid) + ", --, " + value + "]};";
        execGQL(gql);
      }
    }
    return true;
  }

  bool CivetStorage::RemoveFiles(const std::vector<FileID>& filesID)
  {
    for (auto fileID : filesID) {
      std::string fid = std::to_string(fileID);
      std::string gql = "{remove: '" TABLE_FILE "', vertex: {id: " + fid + "}};";
      execGQL(gql);
      gql = "{remove: '" TABLE_RELATION_TAG "', edge: [" + fid + ", --, *]};";
      execGQL(gql);
      gql = "{remove: '" TABLE_RELATION_CLASS_FILE "', edge: [" + fid + ", --, *]};";
      execGQL(gql);
      gql = "{remove: '" TABLE_RELATION_KEYWORD "', edge: [" + fid + ", --, *]};";
      execGQL(gql);
    }
    return true;
  }

  bool CivetStorage::RemoveTags(const std::vector<FileID>& filesID, const Tags& tags)
  {
    std::string sTags;
    for (const std::string& tag : tags) {
      sTags += "'" + tag + "',";
    }
    sTags.pop_back();

    for (FileID fid : filesID) {
      std::string gql = "{remove: '" TABLE_RELATION_TAG "', edge: [" + std::to_string(fid) + ", --, " + sTags + "]};";
      execGQL(gql);
    }
    RemoveKeywords(filesID, tags);
    return true;
  }

  bool CivetStorage::RemoveClasses(const std::vector<std::string>& classes)
  {
    for (auto& clsPath : classes) {
      T_LOG("class", "remove class %s", clsPath.c_str());
      std::string gql = "{remove: '" TABLE_CLASS "', vertex: {" TABLE_CLASS_TITLE ": '" + clsPath + "' } };";
      // printf("remove class: %s\n", gql.c_str());
      execGQL(gql);
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
    for (const std::string& val : tags) {
      std::wstring ws = caxios::string2wstring(val);
      std::vector<std::string> alphabet = getLocaleAlphabet(ws[0]);
      std::string gql = "{upset: '" TABLE_TAG "', vertex: [['" + val + "', {alphabet: '" + alphabet[0][0] + "'}]]};";
      execGQL(gql);
    }

    for (FileID fid : filesID) {
      std::string gqlClear = "{remove: '" TABLE_RELATION_TAG "', edge: [" + std::to_string(fid) + ", --, *]};";
      execGQL(gqlClear);

      for (const std::string& val : tags) {
        std::string gqlUpset = "{upset: '" TABLE_RELATION_TAG "', edge: [" + std::to_string(fid) + ", --, '" + val + "']};";
        execGQL(gqlUpset);

        std::string gqlKeyword = "{upset: '" TABLE_RELATION_KEYWORD "', edge: [" + std::to_string(fid) + ", --, '" + val + "']};";
        execGQL(gqlKeyword);
        // printf("upset keyword: %s\n", gqlKeyword.c_str());
      }

      std::string gqlSnap = "{query: '" TABLE_FILESNAP "', in: '" GRAPH_NAME "', where: {id: " + std::to_string(fid) + "}};";
      
      uint8_t stepBit = 0;
      execGQL(gqlSnap, [&] (gqlite_result* result) {
        nlohmann::json snap = nlohmann::json::parse(result->nodes->_vertex->properties);
        stepBit = (uint8_t)snap["step"];
        });
      std::string gqlUpsetSnap = "{upset: '" TABLE_FILESNAP "', property: [{step: " + std::to_string(stepBit | BIT_TAG) + "}], where: {id: "
        + std::to_string(fid) + "}};";
      execGQL(gqlUpsetSnap);
    }

    // std::string queryTest = "{query: '" TABLE_RELATION_TAG "', in: '" GRAPH_NAME "'};";
    // execGQL(queryTest, [](gqlite_result* result) {
    //   printf("relation tag: %lld -- %s\n", result->nodes->_edge->from->uid, result->nodes->_edge->to->cid);
    // });
    return true;
  }

  std::string CivetStorage::GetClassInfo(ClassID clsID) {
    std::string name;
    std::string query = "{query: '" TABLE_CLASS "', in: '" GRAPH_NAME "', where: {id: " + std::to_string(clsID) + "'}};";
    execGQL(query, [&](gqlite_result* result) {
      if (result->count == 0) return;
      gqlite_vertex* vertex = result->nodes->_vertex;
      nlohmann::json clsInfo = nlohmann::json::parse(vertex->properties);
      name = clsInfo["name"];
    });
    return name;
  }

  bool CivetStorage::GetFilesInfo(const std::vector<FileID>& filesID, std::vector< FileInfo>& filesInfo)
  {
    T_LOG("files", "fileid: %s", format_vector(filesID).c_str());
    for (auto fileID: filesID)
    {
      std::list<IGQLExecutor*> tasks;
      std::string gql = "{query: '" TABLE_FILE "', in: '" GRAPH_NAME "', where: {id: " + std::to_string(fileID) + "}};";
      MetaItems items;
      execGQL(gql, [&](gqlite_result* result) {
        if (result->count == 0) return;

        gqlite_vertex* node = result->nodes->_vertex;
        nlohmann::json meta = nlohmann::json::parse(node->properties);
        T_LOG("files", "properties: %s", node->properties);
        // printf("properties: %s\n", node->properties);

        for (auto obj: meta.items()) {
          MetaItem item;
          item["name"] = obj.key();
          if (obj.value().is_number()) {
            item["type"] = "num";
            item["value"] = std::to_string((long)obj.value());
          } else {
            std::string val = obj.value();
            if (val[0] == '0' && val[1] == 'b') {
              item["type"] = "bin";
              std::vector<uint8_t> bin = base64_decode(val.substr(2));
              item["value"] = std::string(bin.begin(), bin.end());
            } else {
              item["type"] = "str";
              item["value"] = val;
            }
          }
          items.emplace_back(item);
        }
        });

      Tags tags;
      gql = "{query: '" TABLE_RELATION_TAG "', in: '" GRAPH_NAME "', where: [" + std::to_string(fileID) + ", --, *]};";
      execGQL(gql, [&](gqlite_result* result) {
        if (result->count == 0) return;
        tags.push_back(result->nodes->_edge->to->cid);
        });

      Classes classes;
      gql = "{query: '" TABLE_RELATION_CLASS_FILE "', in: '" GRAPH_NAME "', where: [" + std::to_string(fileID) + ", --, *]};";
      std::list<uint64_t> classID;
      execGQL(gql, [&](gqlite_result* result) {
        if (result->count == 0) return;
        classID.emplace_back(result->nodes->_edge->to->uid);
      });

      for (uint64_t cid: classID) {
        classes.push_back(GetClassInfo(cid));
      }

      Keywords keywords;
      gql = "{query: '" TABLE_RELATION_KEYWORD "', in: '" GRAPH_NAME "', where: [" + std::to_string(fileID) + ", --, *]};";
      execGQL(gql, [&](gqlite_result* result) {
        if (result->count == 0) return;
        keywords.push_back(result->nodes->_edge->to->cid);
        });
    
      FileInfo fileInfo{ fileID, items, tags, classes, {}, keywords };
      filesInfo.emplace_back(fileInfo);
    }
    return true;
  }

  bool CivetStorage::GetFilesSnap(std::vector< Snap >& snaps)
  {
    std::string gql = "{query: '" TABLE_FILESNAP "', in: '" GRAPH_NAME "'};";
    execGQL(gql, [&](gqlite_result* result) {
      gqlite_node* node = result->nodes;
      while (node) {
        gqlite_vertex* vertex = node->_vertex;
        nlohmann::json meta = nlohmann::json::parse(vertex->properties);
        int step = 0;
        if (meta.count("step") != 0) step = (int)meta["step"];
        Snap snap{vertex->uid , meta["name"], step };
        snaps.emplace_back(snap);
        node = node->_next;
      }
    });
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
    std::vector<Snap> vSnaps;
    if (!GetFilesSnap(vSnaps)) return false;
    T_LOG("tag", "File Snaps: %llu", vSnaps.size());
    for (auto& snap : vSnaps) {
      char bits = std::get<2>(snap);
      T_LOG("tag", "untag file %d: %d", std::get<0>(snap), bits);
      if (!(bits & BIT_TAG)) {
        filesID.emplace_back(std::get<0>(snap));
      }
    }
    return true;
  }

  bool CivetStorage::GetUnClassifyFiles(std::vector<FileID>& filesID)
  {
    std::vector<Snap> vSnaps;
    if (!GetFilesSnap(vSnaps)) return false;
    for (auto& snap : vSnaps) {
     char bits = std::get<2>(snap);
     T_LOG("class", "unclassify file %d: %d", std::get<0>(snap), bits);
     if (!(bits & BIT_CLASS)) {
       filesID.emplace_back(std::get<0>(snap));
     }
    }
    return true;
  }

  bool CivetStorage::GetTagsOfFiles(const std::vector<FileID>& filesID, std::vector<Tags>& vTags)
  {
    for (auto fileID : filesID) {
     Tags tags;
     GetFileTags(fileID, tags);
     if (tags.size()) vTags.emplace_back(tags);
    }
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
    // 返回当前分类下的所有子分类以及文件
    uint64_t clsID = 0;
    if (!parent.empty() && parent != "/") {
      clsID = GetClassID(parent);
    }

    auto getSimpleInfo = [this](ClassID cID, nlohmann::json& clz) {
      clz["id"] = cID;
      clz["name"] = GetClassInfo(cID);

      auto files = GetFilesOfClass(cID);
      auto count = files.size();
      clz["type"] = "clz";
      clz["count"] = count;
      return files;
    };

    // 获取当前根目录下的所有文件
    auto filesID = GetFilesOfClass(clsID);
    for (auto fid: filesID) {
      nlohmann::json jFile;
      jFile["id"] = fid;
      Snap snap = GetFileSnap(fid);
      jFile["name"] = std::get<1>(snap);
      info.emplace_back(jFile);
    }

    // 获取对应的子目录
    auto&& childrenCls = GetClassChildren(clsID);
    // printf("parent: %s, %ld, children: %ld\n", parent.c_str(), clsID, childrenCls.size());
    for (auto clzID: childrenCls) {
      nlohmann::json clz;
      auto&& children = GetClassChildren(clzID);
      for (auto& childID: children) {
        nlohmann::json jCls;
        getSimpleInfo(childID, jCls);
        // printf("child(%ld): %s\n", childID, jCls.dump().c_str());
        clz["children"].push_back(jCls);
      }

      auto&& files = getSimpleInfo(clzID, clz);
      for (auto fID: files) {
        clz["children"].push_back(fID);
      }
      info.emplace_back(clz);
    }
    
    // printf("getClassesInfo: %s\n%s\n", parent.c_str(), info.dump().c_str());
    // printf("input: %s\n\t%s\n", parent.c_str(), info.dump().c_str());
    
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

  uint64_t CivetStorage::GetClassID(const std::string& name) {
    std::string gql = "{query: '" TABLE_CLASS "', in: '" GRAPH_NAME "', where: {name: '" + name + "'}};";
    uint64_t clsID = 0;
    execGQL(gql, [&](gqlite_result* result) {
      clsID = result->nodes->_vertex->uid;
    });
    return clsID;
  }

  bool CivetStorage::GetAllTags(TagTable& tags)
  {
    //m_pDatabase->Filter(TABLE_TAG_INDX, [this, &tags](const std::string& alphabet, void* pData, uint32_t len, void*& newVal, uint32_t& newLen)->bool {
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

    typedef std::tuple<std::string, std::string, std::vector<FileID>> TagInfo;
    std::vector<TagInfo> vTags;
    std::string gql = "{query: '" TABLE_TAG "', in: '" GRAPH_NAME "'};";
    execGQL(gql, [&](gqlite_result* result) {
      switch(result->nodes->_vertex->type) {
        case gqlite_id_type::bytes: {
          // printf("tag: %s, props: %s\n", result->nodes->_vertex->cid, result->nodes->_vertex->properties);
          nlohmann::json props = nlohmann::json::parse(result->nodes->_vertex->properties);
          std::vector<FileID> fids;
          std::string alphabet = props["alphabet"];
          TagInfo tagInfo = std::make_tuple(alphabet, result->nodes->_vertex->cid, fids);
          tags[alphabet[0]].emplace_back(tagInfo);
        }
          break;
        default:
          break;
      }
    });
    return true;
  }

  bool CivetStorage::UpdateFilesClasses(const std::vector<FileID>& filesID, const std::vector<std::string>& classes)
  {
    return this->AddClasses(classes, filesID);
  }

  bool CivetStorage::UpdateClassName(const std::string& oldName, const std::string& newName)
  {
    if (oldName == newName) return true;
    std::vector<std::string> vWords = split(newName, '/');
    std::vector<std::string> vOldWords = split(oldName, '/');

    auto updateName = [&](const std::string& oldName, const std::string& newName) {
      // 更新分类名
      std::string gql = "{upset: '" TABLE_CLASS "', property: {" TABLE_CLASS_TITLE ": '"
        + newName + "'}, where: {" TABLE_CLASS_TITLE ": '" + oldName + "'}};";
      execGQL(gql);
    };

    auto updateChildrenName = [&](ClassID oldClsID, const std::string& oldName, const std::string& newName) {
      // 更新子类所有title名
      std::map<uint64_t, std::string> childrenTitles;

      std::string gql = "{query: '" TABLE_CLASS "', in: '" GRAPH_NAME "', where: {pid: " + std::to_string(oldClsID) + "}};";
      execGQL(gql, [&](gqlite_result* result) {
        nlohmann::json properties = nlohmann::json::parse(result->nodes->_vertex->properties);
        childrenTitles[result->nodes->_vertex->uid] = properties[TABLE_CLASS_TITLE];
      });
      // printf("GQL(%d):%s\n", childrenTitles.size(), gql.c_str());

      // gql = "{query: '" TABLE_CLASS "', in: '" GRAPH_NAME "'};";
      // execGQL(gql, [](gqlite_result* result) {
      //   printf("Update class name(%ld): %s\n", result->nodes->_vertex->uid, result->nodes->_vertex->properties);
      // });

      for (auto& child: childrenTitles) {
        size_t pos = child.second.rfind('/');
        std::string latestName(newName);
        if (pos != std::string::npos) {
          latestName += child.second.substr(pos);
        }
        gql = "{upset: '" TABLE_CLASS "', property: {" TABLE_CLASS_TITLE ": '"
          + latestName + "'}, where: {id: " + std::to_string(child.first) + "}};";
        // printf("\tchildren, GQL:%s\n", gql.c_str());
        execGQL(gql);
      }
    };

    auto updateKeyword = [&] (ClassID oldClsID, const std::string& oldName, const std::string& newName) {
      // 更新关键词关联的文件id
      auto filesID = GetFilesOfClass(oldClsID);
      for (auto fid: filesID) {
        std::string gql = "{remove: '" TABLE_RELATION_KEYWORD "', edge: [" + std::to_string(fid) + ", --, '" + oldName + "']};";
        execGQL(gql);

        gql = "{upset: '" TABLE_RELATION_KEYWORD "', edge: [" + std::to_string(fid) + ", --, '" + newName + "']};";
        execGQL(gql);
      }
    };

    if (vWords.size() == 1 && vOldWords.size() == 1) {
      auto clsID = GetClassID(vOldWords[0]);
      updateName(vOldWords[0], vWords[0]);
      updateKeyword(clsID, vOldWords[0], vWords[0]);
      updateChildrenName(clsID, vOldWords[0], vWords[0]);
                
    }
    else if(vWords.size() == vOldWords.size()) {
      int size = vWords.size();
      std::string parentCls;
      for (int i = 0; i < size; ++i) {
        if (vWords[i] != vOldWords[i]) {
          uint64_t parentID = 0;
          if (!parentCls.empty()) {
            parentCls.pop_back();
            parentID = GetClassID(parentCls);
          }

          std::string oldName = parentCls + "/" + vOldWords[i];
          std::string newName = parentCls + "/" + vWords[i];
          auto clsID = GetClassID(oldName);

          // printf("update old name: %s with %s\n", oldName.c_str(), newName.c_str());
          updateName(oldName, newName);
          if (clsID != 0) {
            updateKeyword(clsID, oldName, newName);
          // printf("233333\n");
          }
          updateChildrenName(clsID, vOldWords[0], vWords[0]);
          
          continue;
        }
        parentCls += vOldWords[i] + "/";
      }
    }
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
    nlohmann::json condition = nlohmann::json::parse(query);
    std::set<FileID> sFilesID;

    auto getVectorData = [](const nlohmann::json& data) {
      std::vector<std::string> ret;
      // printf("dump: %s\n", data.dump().c_str());
      if (data.is_array()) {
        // printf("dump array\n");
        ret = data.get<std::vector<std::string>>();
      } else {
        std::string word = normalize(data.dump());
        ret = split(word, ',');
      }
      return ret;
    };
    if (condition.count("keyword")) {
      std::vector<std::string>&& vKeywords = getVectorData(condition["keyword"]);
      for (auto& word: vKeywords) {
        if (word[0] != '\'') {
          word = "'" + word + "'";
        }
        std::string gql = "{query: '" TABLE_RELATION_KEYWORD "', in: '" GRAPH_NAME "', where: [" + word + ", --, *]};";
        // std::string gql = "{query: '" TABLE_RELATION_KEYWORD "', in: '" GRAPH_NAME "'};";
        // printf("keyword gql: %s\n", gql.c_str());
        execGQL(gql, [&](gqlite_result* result) {
          if (result->count == 0) return;
          sFilesID.insert(result->nodes->_edge->from->uid);
          // printf("keyword relation: %lld -- %s\n", result->nodes->_edge->from->uid, result->nodes->_edge->to->cid);
        });
      }

      condition.erase("keyword");
    }
    if (condition.count("tag")) {
      std::vector<std::string>&& vTags = getVectorData(condition["tag"]);
      std::string tags;
      for (auto tag: vTags) {
        tags += tag + ",";
      }
      tags.pop_back();

      auto tagsFileID = GetFilesIDByTag(vTags);
      for (auto& vFileID: tagsFileID) {
        sFilesID.insert(vFileID.begin(), vFileID.end());
      }
      condition.erase("tag");
    }
    if (condition.count("class")) {
      std::vector<std::string>&& vClasses = getVectorData(condition["class"]);
      for (auto cls: vClasses) {
        if (cls[0] == '\'') {
          cls = cls.substr(1, cls.size() - 2);
        }
        auto clsID = GetClassID(cls);
        auto&& filesID = GetFilesOfClass(clsID);
        sFilesID.insert(filesID.begin(), filesID.end());
      }
      condition.erase("class");
    }

    std::string restCond;
    bool queryAll = false;
    for (auto& item: condition.items()) {
      std::string value = normalize(item.value().dump());
      if (value == "'*'") {
        value = "*";
      }
      
      restCond += "{" + std::string(item.key()) + ": " + value + "}";
      restCond += ",";
      if (value == "*") break;
    }
    if (restCond.size()) {
      restCond.pop_back();
      // printf("rest condition: %s\n", restCond.c_str());

      std::string gql = "{query: '" TABLE_FILE "', in: '" GRAPH_NAME "'";
      
      gql += ", where: " + normalize(restCond);
      gql += "};";

      printf("query gql: %s\n", gql.c_str());
      execGQL(gql, [&](gqlite_result* result) {
        // printf("query %s\n", result->nodes->_vertex->properties);
        sFilesID.insert(result->nodes->_vertex->uid);
      });
      // printf("query end\n");
      // execGQL("{query: 'file_meta', in: 'civet'};", [&](gqlite_result* result) {
      //   printf("query result %s\n", result->nodes->_vertex->properties);
      // });
    }

    std::vector<FileID> filesID{sFilesID.begin(), sFilesID.end()};
    // printf("files: %ld\n", filesID.size());
    // sCond = json2gql(condition);
    // std::string gql = "{query: '" TABLE_FILE "', in: '" GRAPH_NAME "', where: " + sCond + "};";
    // printf("query gql: %s\n", gql.c_str());
    // std::vector<FileID> filesID;
    // execGQL(gql, [&] (gqlite_result* result) {
    //   filesID.push_back(result->nodes->_vertex->uid);
    //   printf("search result: %lld\n", result->nodes->_vertex->uid);
    // });

    return GetFilesInfo(filesID, filesInfo);
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
        if (pos != std::string::npos) {
          value = value.substr(0, pos);
        }
        if (value.size() > 10) {
          value = value.substr(0, value.size() - 3);
        }
        data[key] = value;
      }
      else {
        data[key] = value;
      }
    }
    for (const std::string& kw : keywords) {
      data["keyword"].push_back(kw);
    }
    std::string gql = normalize(data.dump());
    if (gql == "null") {
      gql = std::string("{upset: '") + TABLE_FILE + "', vertex: [[" + std::to_string(fileid) + "]]};";
    }
    else {
      gql = std::string("{upset: '") + TABLE_FILE + "', vertex: [[" + std::to_string(fileid) + ", " + gql + "]]};";
      // printf("add file: %s\n", gql.c_str());
    }
    T_LOG("CivetStorage", "%s", gql.c_str());
    execGQL(gql);

    gql = std::string("{upset: '") + TABLE_FILESNAP + "', vertex: [["
      + std::to_string(fileid) + ", {name: '" + std::string(data["filename"]) +"', step: 1}]]};";
    execGQL(gql);

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

  std::vector<uint64_t> CivetStorage::AddClassImpl(const std::vector<std::string>& classes)
  {
    std::vector<uint64_t> vClasses;
    for (std::string cls: classes) {
      uint64_t clsID = GetClassID(cls);
      if (clsID == 0) {
        auto clsCode = encode(cls);
        clsID = snowflake2(clsCode);
        vClasses.emplace_back(clsID);
      }
      else {
        vClasses.emplace_back(clsID);
        continue;
      }
      std::vector<std::string> vClsToken = split(cls, '/');
      std::string parent;
      uint64_t parentID = 0;
      if (vClsToken.size() > 1) {
        for (int i = 0; i < vClsToken.size() - 1; ++i) {
          parent += vClsToken[i];
        }
        parentID = GetClassID(parent);
        // printf("parent: %s, %s, %ld\n", cls.c_str(), parent.c_str(), parentID);
      }
      std::string gql = "{upset: '" TABLE_CLASS "', vertex: [[" + std::to_string(clsID) + ", {name: '" + cls + "', pid: " + std::to_string(parentID) + "}]]};";
      // printf("add class: %s\n", gql.c_str());
      execGQL(gql);

      gql = "{upset: '" TABLE_RELATION_CLASS "', edge: [" + std::to_string(parentID) + ", ->, " + std::to_string(clsID) + "]};";
      execGQL(gql);
    }
    
    return vClasses;
  }

  bool CivetStorage::RemoveFile(FileID fileID)
  {
    using namespace nlohmann;

    std::string sID = std::to_string(fileID);
    std::string gql = "{remove: '" TABLE_FILE "', where: {id: " + sID + "}};";

    execGQL(gql);
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
      std::string gql = "{remove: '" TABLE_RELATION_KEYWORD "', edge: [" + std::to_string(fid) + ", --, " + sKeyword + "]};";
      execGQL(gql);
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
    std::string gql = "{query: '" TABLE_RELATION_TAG "', in: '" GRAPH_NAME "', where: ["
                      + std::to_string(fileID) + ", --, *]};";
    execGQL(gql, [&](gqlite_result* result) {
      // printf("file %ld tags: %s\n", fileID, result->nodes->_edge->to->cid);
      tags.push_back(result->nodes->_edge->to->cid);
    });
    return true;
  }

  std::vector<caxios::FileID> CivetStorage::GetFilesByClass(const std::vector<std::string>& clazz)
  {
    std::vector<caxios::FileID> vFilesID;
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

  std::vector<ClassID> CivetStorage::GetClassChildren(ClassID parentClsID)
  {
    std::vector<ClassID> vChildren;
    std::string gql = "{query: '" TABLE_RELATION_CLASS "', in: '" GRAPH_NAME "', where: [" + std::to_string(parentClsID) + ", ->, *]};";
    execGQL(gql, [&](gqlite_result* result) {
      vChildren.push_back(result->nodes->_edge->to->uid);
    });
    return vChildren;
  }

  std::vector<caxios::FileID> CivetStorage::GetFilesOfClass(uint32_t clsID)
  {
    std::vector<caxios::FileID> vFilesID;
    std::string gql = "{query: '" TABLE_RELATION_CLASS_FILE "', in: '" GRAPH_NAME "', where: [*, --, " + std::to_string(clsID) + "]};";
    execGQL(gql, [&](gqlite_result* result) {
      vFilesID.push_back(result->nodes->_edge->from->uid);
    });
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

  Snap CivetStorage::GetFileSnap(FileID fileID)
  {
    Snap snap;
    std::string gql = "{query: '" TABLE_FILESNAP "', in: '" GRAPH_NAME "', where: {id: " + std::to_string(fileID) + "}};";
    execGQL(gql, [&](gqlite_result* result) {
      gqlite_node* node = result->nodes;
      gqlite_vertex* vertex = node->_vertex;
      nlohmann::json meta = nlohmann::json::parse(vertex->properties);
      int step = 0;
      if (meta.count("step") != 0) step = (int)meta["step"];
      snap = Snap{vertex->uid , meta["name"], step };
    });
    return snap;
  }

  std::vector<std::vector<FileID>> CivetStorage::GetFilesIDByTag(const std::vector<std::string>& tags)
  {
    std::vector<std::vector<FileID>> vFilesID;
    for (auto& tag: tags) {
      // std::string gql = "{query: '" TABLE_RELATION_TAG "', in: '" GRAPH_NAME "'};";
      std::string gql = "{query: '" TABLE_RELATION_TAG "', in: '" GRAPH_NAME "', where: [" + tag + ", --, *]};";
      std::vector<FileID> filesID;
      execGQL(gql, [&] (gqlite_result* result) {
        filesID.push_back(result->nodes->_edge->from->uid);
      });
      vFilesID.emplace_back(filesID);
    }
    return std::move(vFilesID);
  }

}