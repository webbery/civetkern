#include "civetkern.h"
#include <iostream>
#include "util/util.h"
#include "log.h"
#include "db_manager.h"

#define JS_CLASS_NAME  "Caxios"

using namespace v8;

namespace caxios {

	CivetKernel::CivetKernel(const std::string& str, int flag, const std::string& meta) {
    T_LOG("CivetKernel", "new CivetKernel(%s)", str.c_str());
    if (m_pStorage == nullptr) {
      m_pStorage = new CivetStorage(str, flag, meta);
    }
  }

  std::vector<FileID> CivetKernel::GenNextFilesID(int cnt)
  {
    return m_pStorage->GenerateNextFilesID(cnt);
  }

  bool CivetKernel::AddFiles(const std::vector <std::tuple< FileID, MetaItems, Keywords >>& files)
  {
    return m_pStorage->AddFiles(files);
  }

  bool CivetKernel::AddClasses(const std::vector<std::string>& classes, const std::vector<FileID>& filesID)
  {
    return m_pStorage->AddClasses(classes, filesID);
  }

  bool CivetKernel::AddClasses(const std::vector<std::string>& classes)
  {
    return m_pStorage->AddClasses(classes);
  }

  bool CivetKernel::AddMeta(const std::vector<FileID>& files, const nlohmann::json& meta)
  {
    return m_pStorage->AddMeta(files, meta);
  }

  bool CivetKernel::SetTags(const std::vector<FileID>& filesID, const std::vector<std::string>& tags)
  {
    return m_pStorage->SetTags(filesID, tags);
  }

  bool CivetKernel::GetFilesSnap(std::vector<Snap>& snaps)
  {
    return m_pStorage->GetFilesSnap(snaps);
  }

  bool CivetKernel::GetFilesInfo(const std::vector<FileID>& filesID, std::vector< FileInfo>& filesInfo)
  {
    return m_pStorage->GetFilesInfo(filesID, filesInfo);
  }

  bool CivetKernel::GetUntagFiles(std::vector<FileID>& filesID)
  {
    return m_pStorage->GetUntagFiles(filesID);
  }

  bool CivetKernel::GetUnclassifyFiles(std::vector<FileID>& filesID)
  {
    return m_pStorage->GetUnClassifyFiles(filesID);
  }

  bool CivetKernel::GetTagsOfFiles(const std::vector<FileID>& filesID, std::vector<Tags>& tags)
  {
    return m_pStorage->GetTagsOfFiles(filesID, tags);
  }

  bool CivetKernel::GetClasses(const std::string& parent, nlohmann::json& classes)
  {
    return m_pStorage->GetClasses(parent, classes);
  }

  bool CivetKernel::getClassesInfo(const std::string& classPath, nlohmann::json& clsInfo)
  {
    return m_pStorage->getClassesInfo(classPath, clsInfo);
  }

  bool CivetKernel::GetAllTags(TagTable& tags)
  {
    return m_pStorage->GetAllTags(tags);
  }

  bool CivetKernel::UpdateFileMeta(const std::vector<FileID>& filesID, const nlohmann::json& mutation)
  {
    return m_pStorage->UpdateFileMeta(filesID, mutation);
  }

  bool CivetKernel::UpdateFilesClasses(const std::vector<FileID>& filesID, const std::vector<std::string>& classes)
  {
    return m_pStorage->UpdateFilesClasses(filesID, classes);
  }

  bool CivetKernel::UpdateClassName(const std::string& oldName, const std::string& newName)
  {
    return m_pStorage->UpdateClassName(oldName, newName);
  }

  bool CivetKernel::RemoveFiles(const std::vector<FileID>& files)
  {
    return m_pStorage->RemoveFiles(files);
  }

  bool CivetKernel::RemoveTags(const std::vector<FileID>& files, const Tags& tags)
  {
    return m_pStorage->RemoveTags(files, tags);
  }

  bool CivetKernel::RemoveClasses(const std::vector<std::string>& classes)
  {
    return m_pStorage->RemoveClasses(classes);
  }

  bool CivetKernel::RemoveClasses(const std::vector<std::string>& classes, const std::vector<FileID>& filesID)
  {
    return m_pStorage->RemoveClasses(classes, filesID);
  }

  bool CivetKernel::Query(const std::string& query, std::vector< FileInfo>& filesInfo)
  {
    return m_pStorage->Query(query, filesInfo);
  }

  CivetKernel::~CivetKernel() {
    T_LOG("CAxios", "Begin ~CAxios()");
    if (m_pStorage) {
      delete m_pStorage;
      m_pStorage = nullptr;
    }
    T_LOG("CAxios", "Finish ~CAxios()");
  }

}