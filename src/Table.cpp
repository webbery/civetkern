#pragma once
#include "Table.h"
#include "log.h"

namespace caxios {

  template<DataType Q> struct CQueryType {
    static typename data_traits<Q>::type policy(MDB_val& val) {
      return *(typename data_traits<Q>::type*)val.mv_data;
    }
  };
  template<> struct CQueryType<QT_String> {
    static std::string policy(MDB_val& val) {
      return std::string((char*)val.mv_data, val.mv_size);
    }
  };
  template<> struct CQueryType<QT_Color> {
    static uint32_t policy(MDB_val& val) {
      std::string v = std::string((char*)val.mv_data, val.mv_size);
      return str2color(v);
    }
  };

  template<DataType T>
  bool compare(std::unique_ptr < IExpression >& pExpr,
    MDB_val& left, std::unique_ptr<ValueInstance>& rightValue) {
    typename data_traits<T>::type leftValue = CQueryType<T>::policy(left);
    return pExpr->compare(leftValue, rightValue->As< typename data_traits<T>::type >());
  }
  template<DataType T>
  bool compare(std::unique_ptr < IExpression >& pExpr,
    typename data_traits<T>::type& leftValue, std::unique_ptr<ValueInstance>& rightValue) {
    return pExpr->compare(leftValue, rightValue->As< typename data_traits<T>::type >());
  }
  template<DataType T>
  bool compare(std::unique_ptr < IExpression >& pExpr,
    MDB_val& left, typename data_traits<T>::type& right) {
    typename data_traits<T>::type leftValue = CQueryType<T>::policy(left);
    return pExpr->compare(leftValue, right);
  }

  std::unique_ptr<caxios::ValueArray> QueryInArray(
    CStorageProxy* pDB,
    const std::string& tableName,
    std::unique_ptr<ValueInstance>& rightValue)
  {
    std::vector<std::string> condition;
    if (rightValue->isArray()) {
      std::unique_ptr< ValueArray > right = dynamic_unique_cast<ValueArray>(std::move(rightValue));
      condition = right->GetArray< std::string >();
    }
    else {
      condition.push_back(rightValue->As<std::string>());
    }
    auto wIndexes = pDB->GetWordsIndex(condition);
    std::vector<caxios::FileID> outFilesID;
    std::set<FileID> sFilesID;
    std::vector<uint32_t> keys;
    void* pData = nullptr;
    uint32_t len = 0;
    for (auto item : wIndexes)
    {
      uint32_t key = item.second;
      if (tableName == TB_Class) {
        pDB->Get(TABLE_KEYWORD2CLASS, key, pData, len);
        addUniqueDataAndSort(keys, std::vector((ClassID*)pData, (ClassID*)pData + len / sizeof(ClassID)));
      }
      else {
        keys.push_back(key);
      }
    }
    T_LOG("query", "query word index: %s, keys: %s", format_vector(wIndexes).c_str(), format_vector(keys).c_str());
    for (auto k : keys) {
      pDB->Get(tableName, k, pData, len);
      if (len) {
        sFilesID.insert((FileID*)pData, (FileID*)pData + len / sizeof(FileID));
      }
    }
    outFilesID.assign(sFilesID.begin(), sFilesID.end());
    T_LOG("query", "query result: %s", format_vector(outFilesID).c_str());
    return std::move(std::unique_ptr<ValueArray>(new ValueArray(outFilesID)));
  }

  void StringCompare(
    std::unique_ptr<caxios::ValueArray>& out,
    std::unique_ptr < IExpression >& pExpr,
    std::pair<MDB_val, MDB_val>& item,
    std::unique_ptr<ValueInstance>& rightValue
  ) {
    T_LOG("query", "StringCompare");
    if (compare<QT_String>(pExpr, item.first, rightValue)) {
        FileID* start = (FileID*)(item.second.mv_data);
        T_LOG("query", "size: %d", item.second.mv_size / sizeof(FileID));
        out->push((FileID*)(item.second.mv_data), item.second.mv_size / sizeof(FileID));
      }
  }
  //void ArrayLogic(
  //  std::unique_ptr<caxios::ValueArray>& out,
  //  std::unique_ptr < IExpression >& pExpr,
  //  std::pair<MDB_val,MDB_val>& item,
  //  std::unique_ptr<ValueInstance>& rightValue
  //) {
  //  std::unique_ptr<ValueArray> pArray = dynamic_unique_cast<ValueArray>(std::move(rightValue));
  //  DataType arrType = pArray->dataType();
  //  switch (arrType) {
  //  case QT_Color:
  //    break;
  //  case QT_String:
  //  {
  //    auto array = pArray->GetArray<std::string>();
  //    for (int idx = 0; idx < array.size(); ++idx)
  //    {
  //      ItemLogic(out, pExpr, item, )
  //    }
  //  }
  //  break;
  //  default:
  //    break;
  //  }
  //}

  std::unique_ptr<caxios::ValueArray> Query(
    CStorageProxy* pDB,
    std::unique_ptr < IExpression > pExpr,
    std::unique_ptr < ITableProxy > leftValue,
    std::unique_ptr<ValueInstance> rightValue)
  {
    std::unique_ptr<caxios::ValueArray> pResult(new ValueArray);
    std::string tableName = leftValue->Value();
    auto pMetaTable = pDB->GetMetaTable(tableName);
    if (pMetaTable) {
      T_LOG("query", "meta(%s)", tableName.c_str());
      // meta
      auto cursor = std::begin(*pMetaTable);
      std::unique_ptr<ValueArray> pArray;
      if (rightValue->isArray()) {
        pArray = dynamic_unique_cast<ValueArray>(std::move(rightValue));
      }
      auto end = Iterator();
      for (; cursor != end; ++cursor) {
        auto item = *cursor;
        auto val = item.first;
        if(pArray){
          DataType arrType = pArray->originType();
          T_LOG("query", "loop array: %d", arrType);
          switch (arrType) {
          case QT_Color:
            break;
          case QT_String:
          {
            auto array = pArray->GetArray<std::string>();
            for (int idx = 0; idx < array.size(); ++idx)
            {
              if (compare<QT_String>(pExpr, item.first, array[idx])) {
                FileID* start = (FileID*)(item.second.mv_data);
                pResult->push((FileID*)(item.second.mv_data), item.second.mv_size / sizeof(FileID));
                break;
              }
            }
          }
          default:
            break;
          }
        }
        else {
          DataType dtype = rightValue->originType();
          switch (dtype)
          {
          case caxios::QT_String:
            T_LOG("query", "right: %s", rightValue->Value().c_str());
            StringCompare(pResult, pExpr, item, rightValue);
            break;
          case caxios::QT_Number:
            break;
          case caxios::QT_DateTime:
            if (compare<QT_DateTime>(pExpr, item.first, rightValue)) {
              FileID* start = (FileID*)(item.second.mv_data);
              pResult->push((FileID*)(item.second.mv_data), item.second.mv_size / sizeof(FileID));
            }
            break;
          case caxios::QT_Color:
            if (compare<QT_Color>(pExpr, item.first, rightValue)) {
              FileID* start = (FileID*)(item.second.mv_data);
              pResult->push((FileID*)(item.second.mv_data), item.second.mv_size / sizeof(FileID));
            }
            break;
          default:
            break;
          }
        }
      }
    }
    else {
      if (tableName == TB_Keyword || tableName == TB_Class || tableName == TB_Tag) {
        return QueryInArray(pDB, tableName, rightValue);
      }
      T_LOG("query", "keyword %s", tableName.c_str());
    }
    return pResult;
  }

  std::unique_ptr<caxios::ValueArray> Query(
    CStorageProxy* pDB,
    std::unique_ptr < IExpression > pExpr,
    std::unique_ptr<ValueInstance> leftValue,
    std::unique_ptr<ValueInstance> rightValue)
  {
    T_LOG("query", "pExpr %s", pExpr->Value().c_str());
    if (pExpr->Value() == OP_And) {
      return Interset(std::move(leftValue), std::move(rightValue));
    }
    else/* if (pExpr->Value() == OP_Or)*/ {
      std::unique_ptr<ValueArray> pLeftArray = dynamic_unique_cast<ValueArray>(std::move(leftValue));
      std::unique_ptr<ValueArray> pRightArray = dynamic_unique_cast<ValueArray>(std::move(rightValue));
      std::unique_ptr<caxios::ValueArray> pResult(new ValueArray);
      return pResult;
    }
  }

  std::unique_ptr<caxios::ValueArray> Interset(std::unique_ptr<ValueInstance> leftValue, std::unique_ptr<ValueInstance> rightValue)
  {
    std::unique_ptr<caxios::ValueArray> pResult(new ValueArray);
    std::vector< std::unique_ptr<ValueInstance> > vItems;
    std::unique_ptr<ValueArray> pLeftArray = dynamic_unique_cast<ValueArray>(std::move(leftValue));
    std::unique_ptr<ValueArray> pRightArray = dynamic_unique_cast<ValueArray>(std::move(rightValue));
    for (auto itr = pLeftArray->begin(); itr != pLeftArray->end(); ++itr)
    {
      auto ptr = std::lower_bound(vItems.begin(), vItems.end(), itr, [](
        const std::unique_ptr<ValueInstance>& left, typename ValueArray::iterator right)->bool
        {
          return *left.get() < *(*right).get();
        });
      if (ptr == vItems.end() || *(*ptr) != *(*itr)) {
        vItems.insert(ptr, std::move(*itr));
      }
    }
    for (auto itr = pRightArray->begin(); itr != pRightArray->end(); ++itr)
    {
      auto ptr = std::lower_bound(vItems.begin(), vItems.end(), itr, [](
        const std::unique_ptr<ValueInstance>& left, typename ValueArray::iterator right)->bool
        {
          return *left.get() < *(*right).get();
        });
      auto item = std::move(*itr);
      if (ptr != vItems.end() && *(*ptr) == *item) {
        // find the same value
        pResult->push(item);
      }
    }
    return pResult;
  }

}