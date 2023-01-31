#pragma once
#include <string>

namespace caxios {

class FileLink {
public:
  FileLink();
  ~FileLink();
  bool CreateLinkDirectory(const char* fullpath);

  bool IsLinkDirectoryExist(const char* fullpath);

  bool CreateLinkFile(const char* fullpath, const char* linkpath);

  bool ReadLinkFile(const char* linkpath, std::string& realPath);

  bool EraseLinkFile(const char* fullpath);

private:
	void SplitPath(const std::string& fullpath, std::string& filename, std::string& filedir);

private:
	std::string _filename;
	std::string _directory;
};

} // namespace caxios