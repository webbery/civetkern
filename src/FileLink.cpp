#include "FileLink.h"
#ifdef WIN32
#include <windows.h>
#include <shobjidl.h>
#include <shlguid.h>
#else
#endif
#include "util/util.h"

namespace caxios {

    void FileLink::SplitPath(const std::string& fullpath, std::string& filename, std::string& filedir) {
#ifdef WIN32
        size_t pos = fullpath.find_last_of('\\');
#else
        size_t pos = fullpath.find_last_of('/');
#endif
        if (pos == std::string::npos) {
            filename = fullpath;
            filedir = ".";
        }
        else {
            filedir = fullpath.substr(0, pos);
            filename = fullpath.substr(pos + 1, fullpath.size() - pos - 1);
        }
    }

    bool FileLink::CreateLinkDirectory(const char* fullpath) {
        return true;
    }

    bool FileLink::IsLinkDirectoryExist(const char* fullpath) {}

    bool FileLink::CreateLinkFile(const char* fullpath, const char* linkpath) {
        std::string destpath(fullpath), srcpath(linkpath);
        if (exist(destpath)) return true;
        if (!exist(srcpath)) return false;
        std::string destDir, destName;
        SplitPath(destpath, destName, destDir);
#ifdef WIN32
        HRESULT hres;
        IShellLink* psl;
        hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl);
        if (SUCCEEDED(hres)) {}
#else
#endif
        return true;
    }

    bool FileLink::EraseLinkFile(const char* fullpath){}

}