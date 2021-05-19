#pragma once

namespace caxios {
    template<int N>
    struct CharVector{
        char _value[N];
    };

    typedef CharVector<3> Color3;
}