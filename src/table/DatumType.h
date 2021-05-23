#pragma once
#include "util/util.h"

namespace caxios {
    template<int N>
    struct CharVector{
        char _value[N];
    };

    typedef CharVector<3> Color3;



    class DateTime{
    public:
      DateTime(const std::string& value) {
        if (isDate(value)) {
          _value = str2time(value);
        }
      }
    private:
      time_t _value;
    };
}