#include "FileLink.h"
#include <system_error>
#include <unistd.h>
#ifdef WIN32
#include <windows.h>
#include <shobjidl.h>
#include <shlguid.h>
#else
#endif
#include <filesystem>
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
        if (SUCCEEDED(hres)) {
            IPersistFile* ppf;

            psl->SetPath(lpszPathObj); 
            psl->SetDescription(lpszDesc);

            hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);

            if (SUCCEEDED(hres)) {
                WCHAR wsz[MAX_PATH];
                MultiByteToWideChar(CP_ACP, 0, lpszPathLink, -1, wsz, MAX_PATH);
                hres = ppf->Save(wsz, TRUE); 
                ppf->Release();
            }
            psl->Release();
        }
        if (!SUCCEEDED(hres)) return false;
#else
        int err = link(fullpath, linkpath);
        switch(err) {
            case EACCES:
            case EMLINK:
            case ENOENT:
            case ENOSPC:
            case EPERM:
            case EROFS:
            return false;
            case EXDEV: {// 链接在其他文件系统中,直接复制一份
                std::error_code ec;
                std::filesystem::copy(fullpath, linkpath, ec);
            }
            break;
            case EEXIST:
            default:
            break;
        }
#endif
        return true;
    }

    bool FileLink::EraseLinkFile(const char* fullpath){
#ifdef WIN32
#else
#endif
    }

}