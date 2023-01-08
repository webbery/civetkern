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
  static const std::string base64_chars(
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/");

  std::string base64_encode(const std::vector<uint8_t>& bin)
  {
    std::string ret;
    int i = 0;
    int j = 0;
    uint8_t char_array_3[3];
    uint8_t char_array_4[4];

    size_t offset = 0;
    size_t bufLen = bin.size();
    while (bufLen--)
    {
      char_array_3[i++] = bin[offset++];
      if (i == 3)
      {
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (i = 0; (i < 4); i++)
          ret += base64_chars[char_array_4[i]];
        i = 0;
      }
    }

    if (i)
    {
      for (j = i; j < 3; j++)
        char_array_3[j] = '\0';

      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;

      for (j = 0; (j < i + 1); j++)
        ret += base64_chars[char_array_4[j]];

      while ((i++ < 3))
        ret += '=';
    }

    return ret;
  }

  std::vector<uint8_t> base64_decode(const std::string& base64) {
    int in_len = strlen(base64.c_str());
    int i = 0;
    int j = 0;
    int in_ = 0;
    uint8_t char_array_4[4], char_array_3[3];
    std::vector<uint8_t> ret;

    while (in_len-- && (base64[in_] != '='))
    {
        char_array_4[i++] = base64[in_];
        in_++;
        if (i == 4)
        {
            for (i = 0; i < 4; i++)
                char_array_4[i] = base64_chars.find(char_array_4[i]);

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; (i < 3); i++)
                ret.push_back(char_array_3[i]);
            i = 0;
        }
    }

    if (i)
    {
        for (j = i; j < 4; j++)
            char_array_4[j] = 0;

        for (j = 0; j < 4; j++)
            char_array_4[j] = base64_chars.find(char_array_4[j]);

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (j = 0; (j < i - 1); j++)
            ret.push_back(char_array_3[j]);
    }

    return ret;
  }
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
    return std::mktime(&stm);
    // return std::mktime(&stm)*1000 + milliSec;
  }

  uint32_t str2color(const std::string& input)
  {
    // #FF
    char* p;
    // printf("color: %s, %s\n", input.c_str(), input.substr(1).c_str());
    return strtoul(input.substr(1).c_str(), &p, 16);
  }

  std::vector<u_char> ul2color(uint32_t input) {
    uint32_t bit = 0xFF;
    std::vector<u_char> color;
    int rgbIndex = 3;
    if (input >= 0xFFFFFF00) {
      rgbIndex = 4;
    }
    for (char i = 0; i < rgbIndex; ++i) {
      color.insert(color.begin(), (input & bit) >> (i*8));
      bit <<= 8;
    }
    
    if (rgbIndex == 3) {
      color.push_back(0);
    }
    return color;
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

  std::string replace_strdate(const std::string& input) {

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
    std::regex patternStrDate(R"('(0d\w+)')");
    result = std::regex_replace(result, patternStrDate, "$1");

    std::regex patternDatetime(R"((\^[0-9]+.[0-9]*|'[0-9]+-[0-9]+-[0-9]+T[0-9]+:[0-9]+:[0-9]+.[0-9]+Z'))");
    std::smatch mrs;
    if (std::regex_search(result, mrs, patternDatetime)) {
      // std::cout<<"match:" << mrs[0] << ", "<< mrs[1] <<std::endl;
      auto st = mrs.str(1);
      time_t t = str2time(st.substr(1, st.size() - 2));
      result = std::regex_replace(result, patternDatetime, "0d" + std::to_string(t));
    }

    std::smatch mrs2;
    std::regex patternColor("'#([A-Fa-f0-9]{6}|[A-Fa-f0-9]{3})'");
    if (std::regex_search(result, mrs2, patternColor)) {
      auto st = mrs2.str(1);
      char* p;
      auto c = strtoul(st.c_str(), &p, 16);
      auto&& color = ul2color(c);
      std::string sColor;
      for (auto elem: color) {
        sColor += std::to_string((int)elem) + ",";
      }
      sColor.pop_back();
      result = std::regex_replace(result, patternColor, "[" + sColor + "]");
    }
    std::regex patternKey(R"('([\$]*\w+)':)");
    result = std::regex_replace(result, patternKey, "$1:");
    return result;
  }

  std::string json2gql(const nlohmann::json& input)
  {
    std::string gql = input.dump();
    enum class Status {
      Start,
      Splash,
      End
    };
    Status previous = Status::End;
    int prevPos = 0;
    Status cur = Status::End;
    for (size_t index = 0; index < gql.size(); ++index) {
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

  uint64_t AttrAsUint64(Napi::Object obj, std::string attr) {
    return obj.Get(attr).As<Napi::Number>().Int64Value();
  }

  uint32_t AttrAsUint32(Napi::Object obj, unsigned int const attr)
  {
    return obj.Get(attr).As<Napi::Number>().Uint32Value();
  }

  uint64_t AttrAsUint64(Napi::Object obj, unsigned int const attr) {
    return obj.Get(attr).As<Napi::Number>().Int64Value();
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

  std::vector<uint64_t> AttrAsUint64Vector(Napi::Object obj, std::string attr) {
    Napi::Array array = obj.Get(attr).As<Napi::Array>();
    std::vector<uint64_t> vec(array.Length());
    for (unsigned int i = 0; i < array.Length(); i++) {
      vec[i] = AttrAsUint64(array, i);
    }
    return vec;
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

  std::vector<uint64_t> ArrayAsUint64Vector(Napi::Array arr) {
    std::vector< uint64_t > vec;
    ForeachArray(arr, [&vec](Napi::Value item) {
      uint64_t fid = item.As<Napi::Number>().Int64Value();
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
    size_t cnt = vStr.size();
    Napi::Array arr = Napi::Array::New(env, cnt);
    for (size_t idx = 0; idx < cnt; ++idx) {
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
    function.As<Napi::Function>().Call(vArgs);
  }

  std::wstring string2wstring(const std::string& str)
  {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.from_bytes(str);
    //std::locale::global(std::locale("Chinese-simplified"));
  }

  uint32_t snowflake2(uint8_t inputID) {
    static constexpr long sequenceBit = 8;
    static constexpr long inputBit = 8;
    static constexpr long timestampShift = sequenceBit + inputBit;
    static constexpr long TWEPOCH = 1420041600000;
    thread_local long sequence = 0;
    thread_local long sequenceMask = -1L ^ (-1L << sequenceBit);
    thread_local auto lastTimeStamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto curTimeStamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if (curTimeStamp == lastTimeStamp) {
      sequence = (sequenceMask + 1) & sequenceMask;
    }
    else {
      lastTimeStamp = curTimeStamp;
    }
    return ((curTimeStamp - TWEPOCH) << timestampShift)
      | (uint32_t(inputID) << sequenceBit)
      | sequence;
  }
}