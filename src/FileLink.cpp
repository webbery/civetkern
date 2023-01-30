#include "FileLink.h"
#include <system_error>
#ifdef WIN32
#include <windows.h>
#include <shobjidl.h>
#include <shlguid.h>
#else
#include <unistd.h>
#endif
#include <filesystem>
#include "util/util.h"
#include "log.h"

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


    FileLink::FileLink() {
      CoInitialize(NULL);
    }

    FileLink::~FileLink() {
      CoUninitialize();
    }

    bool FileLink::CreateLinkDirectory(const char* fullpath) {
        return true;
    }

    bool FileLink::IsLinkDirectoryExist(const char* fullpath) {}

    bool FileLink::CreateLinkFile(const char* fullpath, const char* linkpath) {
      if (!exist(fullpath)) {
        T_LOG("CivetLink", "source file %s not exist", fullpath);
        return true;
      }
      if (exist(linkpath)) {
        T_LOG("CivetLink", "link file %s exist", linkpath);
        return false;
      }
        std::string destDir, destName;
        SplitPath(fullpath, destName, destDir);
#ifdef WIN32
        HRESULT hres;
        IShellLink* psl;
        hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl);
        if (SUCCEEDED(hres)) {
            IPersistFile* ppf;

            psl->SetPath(fullpath);
            psl->SetDescription(destName.c_str());

            hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);

            if (SUCCEEDED(hres)) {
                WCHAR wsz[MAX_PATH];
                MultiByteToWideChar(CP_ACP, 0, linkpath, -1, wsz, MAX_PATH);
                hres = ppf->Save(wsz, TRUE); 
                ppf->Release();
            }
            psl->Release();
        }
        if (!SUCCEEDED(hres)) {
          T_LOG("CivetLink", "add link fail: %ld", hres);
          return false;
        }
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
      return std::filesystem::remove(fullpath);
    }

    bool FileLink::ReadLinkFile(const char* linkpath, std::string& realPath) {
#ifdef WIN32
      IShellLink* psl;
      HRESULT hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl);
      if (SUCCEEDED(hres)) {
        IPersistFile* ppf;
        hres = psl->QueryInterface(IID_IPersistFile, (void**)&ppf);

        if (SUCCEEDED(hres)) {
          WCHAR wsz[MAX_PATH];
          MultiByteToWideChar(CP_ACP, 0, linkpath, -1, wsz, MAX_PATH);

          while (SUCCEEDED(ppf->Load(wsz, STGM_READ)) && SUCCEEDED(psl->Resolve(NULL, 0))) {
            CHAR szGotPath[MAX_PATH];
            WIN32_FIND_DATA wfd;
            hres = psl->GetPath(szGotPath, MAX_PATH, (WIN32_FIND_DATA*)&wfd, SLGP_SHORTPATH);
            if (FAILED(hres)) break;

            realPath = szGotPath;
            CHAR szDescription[MAX_PATH];
            hres = psl->GetDescription(szDescription, MAX_PATH);
            if (FAILED(hres)) break;

            T_LOG("CivetLink", "Desc: %s, path: %s", szDescription, szGotPath);
            break;
          }
          ppf->Release();
        }
        psl->Release();
      }
      return SUCCEEDED(hres);
#else
      if (!exist(linkpath)) return false;
      realPath = linkpath;
      return true;
#endif
    }
}