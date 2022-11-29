#include "util.h"
#include <iostream>
#include "log.h"
#if defined(__APPLE__) || defined(__gnu_linux__) || defined(__linux__) 
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#elif defined(WIN32)
#include <windows.h>
#include <direct.h>
#include <io.h>
#endif
#include <regex>
#include <iomanip>
#include <sstream>
#include <codecvt>
#include <locale>
#include <iostream>
#define CHAR_BIT_S  255

namespace caxios {
  
  bool exist(const std::string& filepath) {
#if defined(__APPLE__) || defined(__gnu_linux__) || defined(__linux__)
    if (access(filepath.c_str(), 0) != -1) return true;
#elif defined(WIN32)
    if (_access(filepath.c_str(), 0) == 0) return true;
#endif
    return false;
  }

  bool replace(const std::string& oldpath, const std::string& newpath)
  {
#if defined(__APPLE__) || defined(__gnu_linux__) || defined(__linux__) 
    int rc = remove(newpath.c_str());
    if (rc) {
      T_LOG("upgrade", "error: %s", strerror(errno));
    }
    rename(oldpath.c_str(), newpath.c_str());
#else
    auto wpath = string2wstring(newpath);
    DeleteFileW(wpath.c_str());
    auto opath = string2wstring(oldpath);
    MoveFileW(opath.c_str(), wpath.c_str());
#endif
    return true;
}

  void MkDir(
#if defined(__APPLE__) || defined(__gnu_linux__) || defined(__linux__) 
    const std::string& dir
#else
    const std::wstring& dir
#endif
  ) {
#if defined(__APPLE__) || defined(__gnu_linux__) || defined(__linux__) 
    mkdir(dir.c_str()
      , 0777
#else
    _wmkdir(dir.c_str()
#endif
    );
  }
  bool isDirectoryExist(
#if defined(__APPLE__) || defined(UNIX) || defined(__linux__)
    const std::string& dir
#elif defined(WIN32)
    const std::wstring& dir
#endif
  ) {
#if defined(__APPLE__) || defined(UNIX) || defined(__linux__)
    if (access(dir.c_str(), 0) != -1) return true;
#elif defined(WIN32)
    if (_waccess(dir.c_str(), 0) == 0) return true;
#endif
    return false;
  }
  void createDirectories(
#if defined(__APPLE__) || defined(UNIX) || defined(__linux__)
    const std::string& dir
#elif defined(WIN32)
    const std::wstring& dir
#endif
  ) {
    if (isDirectoryExist(dir)) return;
    size_t pos = dir.rfind(UNIVERSAL_DELIMITER);
    if (pos == std::string::npos) {
      pos = dir.rfind(OS_DELIMITER);
      if (pos == std::string::npos) {
        MkDir(dir);
        return;
      }
    }
#if defined(__APPLE__) || defined(UNIX) || defined(__linux__)
    std::string parentDir;
#elif defined(WIN32)
    std::wstring parentDir;
#endif
    parentDir = dir.substr(0, pos);
    createDirectories(parentDir);
    MkDir(dir);
  }
  
  std::string serialize(const std::vector< std::vector<WordIndex> >& classes)
  {
    // len wordindexes len wordindexes ...
    std::string s;
    for (auto& v : classes) {
      char len = v.size();
      s.push_back(len);
      std::string t = serialize(v);
      for (auto itr = t.begin(); itr<t.end();++itr)
      {
        s.push_back(*itr);
      }
      //std::copy(t.begin(), t.end(), s.end());
      T_LOG("util", "serialize %s(%d)", format_x16(t).c_str(), len);
    }
    return s;
  }

  void deserialize(const std::string& s, std::vector<WordIndex>& v)
  {
    for (size_t idx = 0; idx < s.size(); idx += 4) {
      WordIndex wi = ((s[idx] >> 24) | (s[idx + 1] >> 16) | (s[idx + 2] >> 8) | s[idx + 3]);
      T_LOG("util", "deserialize %s: %d", format_x16(s).c_str(), wi);
      v.emplace_back(wi);
    }
  }

  void deserialize(const std::string& s, std::vector< std::vector<WordIndex> >& v)
  {
    for (size_t idx = 0; idx < s.size();) {
      char len = s[idx];
      std::vector<WordIndex> wds = deserialize<std::vector<WordIndex>>(s.substr(idx + 1, len));
      v.push_back(wds);
      idx += len + 1;
    }
  }

  uint32_t encode(const std::string& str)
  {
    // TODO: avoid hash clide
    if (str == "/") return 0;
    uint32_t hash, idx;
    for (hash = str.size(), idx = 0; idx < str.size(); ++idx) {
      hash = (hash << 4) ^ (hash >> 28) ^ str[idx];
    }
    return createHash(hash);
  }

  uint32_t createHash(uint32_t hs, uint32_t di) 
  {
    return (hs + di) % std::numeric_limits<uint32_t>::max();
  }

  bool eraseData(std::vector<WordRef>& vDest, WordIndex tgt)
  {
    for (auto itr = vDest.begin(); itr != vDest.end();) {
      if (itr->_wid == tgt) {
        itr->_ref -= 1;
      }
      if (itr->_ref == 0) {
        itr = vDest.erase(itr);
        return true;
      }
      else {
        ++itr;
      }
    }
    return false;
  }

  bool isDate(const std::string& input)
  {
    std::regex reg("\\\^[0-9]+.[0-9]*|[0-9]+-[0-9]+-[0-9]+T[0-9]+:[0-9]+:[0-9]+.[0-9]+Z");
    return std::regex_match(input, reg);
  }

  bool isColor(const std::string& input)
  {
    std::regex reg("^#([A-Fa-f0-9]{6}|[A-Fa-f0-9]{3})$");
    return std::regex_match(input, reg);
  }

  time_t str2time(const std::string& input)
  {
    size_t pos = input.find_last_of('.');
    int milliSec = 0;
    if (pos != std::string::npos) {
      std::string mill = input.substr(pos + 1, 3);
      milliSec = atoi(mill.c_str());
    }
    std::istringstream iss(input);
    std::tm stm = {};
    iss >> std::get_time(&stm, "%Y-%m-%dT%H:%M:%SZ");
    return std::mktime(&stm)*1000 + milliSec;
  }

  uint32_t str2color(const std::string& input)
  {
    // #FF
    char* p;
    return strtoul(input.substr(1).c_str(), &p, 16);
  }

  std::string serialize(const std::vector<WordIndex>& classes)
  {
    std::string s;
    if (classes.size() == 0) return ROOT_CLASS_PATH;
    for (const WordIndex& wi : classes) {
      char c4 = wi & (CHAR_BIT_S << 24);
      s.push_back(c4);
      char c3 = wi & (CHAR_BIT_S << 16);
      s.push_back(c3);
      char c2 = wi & (CHAR_BIT_S << 8);
      s.push_back(c2);
      char c1 = wi & CHAR_BIT_S;
      s.push_back(c1);
    }
    T_LOG("util", "serialize %s(%d, %d)", format_x16(s).c_str(), classes.size(), s.size());
    return s;
  }

  std::string replace_all(const std::string& input, const std::string& origin, const std::string& newer)
  {
    std::string data(input);
    size_t pos = 0;
    while ((pos = data.find(origin, pos)) != std::string::npos) {
      data = data.replace(pos, origin.size(), newer);
      pos += newer.size();
    }
    return data;
  }

  std::string normalize(const std::string& gql)
  {
    std::string result(gql);
    enum class ChangeStat {
      Start,
      Splash,
      End
    };
    ChangeStat previous = ChangeStat::End;
    ChangeStat cur = ChangeStat::End;
    for (size_t pos = 0, len = result.size(); pos != len; ++pos) {
      if (result[pos] == '"') {
        switch (previous) {
        case ChangeStat::Splash:
          previous = cur;
          continue;
        case ChangeStat::Start:
          cur = ChangeStat::End;
          previous = ChangeStat::End;
          break;
        case ChangeStat::End:
          cur = ChangeStat::Start;
          previous = ChangeStat::Start;
          break;
        default:break;
        }
        result[pos] = '\'';
      }
      else if (result[pos] == '\\') {
        previous = ChangeStat::Splash;
        continue;
      }
      previous = cur;
    }

    result = replace_all(result, "\\u0000", "");
    // convert datetime, array
    std::regex patternDatetime("'(\\w+)':");
    return std::regex_replace(result, patternDatetime, "$1:");
  }

  std::string json2gql(const nlohmann::json& input)
  {
    std::string gql = input.dump();
    // ��Ҫ����: keyȥ��˫����, ����\\ ת\, value�е�˫�����ַ����ĳɵ������ַ��� 
    enum class Status {
      Start,
      Splash,
      End
    };
    Status previous = Status::End;
    int prevPos = 0;
    Status cur = Status::End;
    for (int index = 0; index < gql.size(); ++index) {
      if (gql[index] == '"') {
        switch (previous) {
        case Status::Splash:
          previous = cur;
          continue;
        case Status::Start:
          cur = Status::End;
          previous = Status::End;
          if (gql[index + 1] == ':') {
            // match key
            gql.erase(gql.begin() + index, gql.begin() + index + 1);
            gql.erase(gql.begin() + prevPos, gql.begin() + prevPos + 1);
            index -= 1;
            continue;
          }
          break;
        case Status::End:
          cur = Status::Start;
          previous = Status::Start;
          prevPos = index;
          break;
        default:break;
        }
        gql[index] = '\'';
      }
      else if (gql[index] == '\\') {
        if (gql[index + 1] == '\\') {
          gql.erase(gql.begin() + index, gql.begin() + index + 1);
          index -= 1;
        }
      }
    }
    return gql;
  }

  bool HasAttr(Napi::Object obj, std::string attr)
  {
    return obj.Has(attr);
  }

  std::string AttrAsStr(Napi::Object obj, const std::string& attrs)
  {
    //T_LOG("util", "---display: %s", attrs.c_str());
    if (attrs[0] != '/') {
      return obj.Get(attrs).As<Napi::String>();
    }
    size_t offset = attrs.find_first_of('/', 1);
    if(offset!= std::string::npos){
      std::string attr = attrs.substr(1, offset - 1);
      size_t count = attrs.size() - attr.size();
      std::string child = attrs.substr(offset, count);
      //T_LOG("util", "display: %s, %s", attr.c_str(), child.c_str());
      obj = obj.Get(attr).As<Napi::Object>();
      return AttrAsStr(obj, child);
    }
    return AttrAsStr(obj, attrs.substr(1, attrs.size() - 1));
  }

  std::string AttrAsStr(Napi::Object obj, int attrs)
  {
    //T_LOG("display: %s", attrs.c_str());
    return obj.Get(attrs).As<Napi::String>();
  }

  std::string AttrAsStr(Napi::Object obj, unsigned int const attr)
  {
    return obj.Get(attr).As<Napi::String>();
  }

  uint32_t AttrAsUint32(Napi::Object obj, std::string attr)
  {
    return obj.Get(attr).As<Napi::Number>().Uint32Value();
  }

  uint32_t AttrAsUint32(Napi::Object obj, unsigned int const attr)
  {
    return obj.Get(attr).As<Napi::Number>().Uint32Value();
  }

  int32_t AttrAsInt32(Napi::Object obj, std::string attr)
  {
    return obj.Get(attr).As<Napi::Number>().Int32Value();
  }

  int32_t AttrAsInt32(Napi::Object obj, unsigned int const attr)
  {
    return obj.Get(attr).As<Napi::Number>().Int32Value();
  }

  double AttrAsDouble(Napi::Object obj, std::string attr)
  {
    return obj.Get(attr).As<Napi::Number>().DoubleValue();
  }

  double AttrAsDouble(Napi::Object obj, unsigned int const attr)
  {
    return obj.Get(attr).As<Napi::Number>().DoubleValue();
  }

  bool AttrAsBool(Napi::Object obj, std::string attr)
  {
    return obj.Get(attr).As<Napi::Boolean>().Value();
  }


  Napi::Array AttrAsArray(Napi::Object obj, std::string attr)
  {
    return obj.Get(attr).As<Napi::Array>();
  }

  Napi::Object AttrAsObject(Napi::Object obj, std::string attr)
  {
    return obj.Get(attr).As<Napi::Object>();
  }

  std::vector<int32_t> AttrAsInt32Vector(Napi::Object obj, std::string attr)
  {
    Napi::Array array = obj.Get(attr).As<Napi::Array>();
    std::vector<int32_t> vector(array.Length());
    for (unsigned int i = 0; i < array.Length(); i++) {
      vector[i] = AttrAsInt32(array, i);
    }
    return vector;
  }

  std::vector<uint32_t> AttrAsUint32Vector(Napi::Object obj, std::string attr)
  {
    Napi::Array array = obj.Get(attr).As<Napi::Array>();
    std::vector<uint32_t> vector(array.Length());
    for (unsigned int i = 0; i < array.Length(); i++) {
      vector[i] = AttrAsUint32(array, i);
    }
    return vector;
  }

  std::vector<std::string> AttrAsStringVector(Napi::Object obj, std::string attr)
  {
    Napi::Array array = obj.Get(attr).As<Napi::Array>();
    std::vector<std::string> vector(array.Length());
    for (unsigned int i = 0; i < array.Length(); i++) {
      vector[i] = AttrAsStr(array, i);
    }
    return vector;
  }

  Napi::Object FindObjectFromArray(Napi::Object obj, std::function<bool(Napi::Object)> func)
  {
    Napi::Array array = obj.As<Napi::Array>();
    for (unsigned int i = 0; i < array.Length(); i++) {
      auto item = array.Get(i).As<Napi::Object>();
      if (func(item)) {
        return item;
      }
    }
    return Napi::Object();
  }

  std::vector<uint32_t> ArrayAsUint32Vector(Napi::Array arr)
  {
    std::vector< uint32_t > vec;
    ForeachArray(arr, [&vec](Napi::Value item) {
      uint32_t fid = item.As<Napi::Number>().Uint32Value();
      vec.emplace_back(fid);
    });
    return std::move(vec);
  }

  std::vector<std::string> ArrayAsStringVector(Napi::Array arr)
  {
    std::vector< std::string > vec;
    ForeachArray(arr, [&vec](Napi::Value item) {
      std::string fid = item.As<Napi::String>();
      vec.emplace_back(fid);
    });
    return std::move(vec);
  }

  std::string AttrAsBinary(Napi::Object obj, std::string attr)
  {
    auto bin = obj.Get(attr).As<Napi::Uint8Array>();
    size_t len = bin.ElementLength();
    char* ptr = reinterpret_cast<char*>(bin.ArrayBuffer().Data());
    std::string s(ptr, len);
    return std::move(s);
  }

  Napi::Array Vector2Array(Napi::Env env, const std::vector<std::string>& vStr)
  {
    int cnt = vStr.size();
    Napi::Array arr = Napi::Array::New(env, cnt);
    for (unsigned int idx = 0; idx < cnt; ++idx) {
      arr.Set(idx, vStr[idx]);
    }
    return arr;
  }

  void ForeachObject(Napi::Value value, std::function<void(const std::string&, Napi::Value)> func)
  {
    auto item = value.As<Napi::Object>();
    auto props = item.GetPropertyNames();
    for (int i = 0, l = props.Length(); i < l; i++) {
      std::string key = props.Get(i).As<Napi::String>();
      func(key, item);
    }
  }

  void ForeachArray(Napi::Value arr, std::function<void(Napi::Value)> func)
  {
    Napi::Array lArr = arr.As<Napi::Array>();
    for (int i = 0, l = lArr.Length(); i < l; i++)
    {
      Napi::Value localItem = lArr.Get(i);
      func(localItem);
    }
  }

  std::string Stringify(Napi::Env env, Napi::Object obj)
  {
    Napi::Object json = env.Global().Get("JSON").As<Napi::Object>();
    Napi::Function stringify = json.Get("stringify").As<Napi::Function>();
    return stringify.Call(json, { obj }).As<Napi::String>();
  }

  Napi::Object Parse(Napi::Env env, const std::string& str) {
    Napi::Object json = env.Global().Get("JSON").As<Napi::Object>();
    Napi::Function parse = json.Get("parse").As<Napi::Function>();
    Napi::String jstr = Napi::String::New(env, str.c_str());
    return parse.Call(json, { jstr }).As<Napi::Object>();
  }

  std::string trunc(const std::string& elm)
  {
    size_t start = 0;
    size_t end = elm.size();
    if (elm[0] == '"' && elm[elm.size() - 1] == '"') {
      start += 1;
      end -= 1;
    }
    else if (elm[0] == '\'' && elm[elm.size() - 1] == '\'') {
      start += 1;
      end -= 1;
    }
    return elm.substr(start, end - start);
  }

  std::vector<std::string> split(const std::string& str, char delim)
  {
    std::vector<std::string> vStr;
    size_t start = 0;
    for (size_t idx = 0; idx < str.size(); ++idx)
    {
      if (str[idx] == delim && idx > start) {
        auto t = str.substr(start, idx - start);
        vStr.emplace_back(t);
        start = idx + 1;
      }
    }
    vStr.emplace_back(str.substr(start, str.size() - start));
    return std::move(vStr);
  }

  bool isInteger(Napi::Env& env, Napi::Value& num)
  {
    return env.Global()
      .Get("Number")
      .ToObject()
      .Get("isInteger")
      .As<Napi::Function>()
      .Call({ num })
      .ToBoolean()
      .Value();
  }

  bool isNumber(const std::string& input)
  {
    std::regex reg("[0-9]+.[0-9]*");
    return regex_match(input, reg);
  }

  void Call(Napi::Env env, const std::string& func, const std::vector<std::string>& params)
  {
    auto global = env.Global();
    auto function = global.Get(func);
    std::vector<napi_value> vArgs;
    for (const std::string& arg : params) {
      Napi::String v = Napi::String::New(env, arg.c_str());
      vArgs.emplace_back(v);
    }
    auto result = function.As<Napi::Function>().Call(vArgs);
  }

  std::wstring string2wstring(const std::string& str)
  {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.from_bytes(str);
    //std::locale::global(std::locale("Chinese-simplified"));
  }
}