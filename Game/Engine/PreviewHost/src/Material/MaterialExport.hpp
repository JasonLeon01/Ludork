#pragma once

#include <Runtime/RuntimeData.hpp>

namespace ludork::preview_host {

class MaterialExport {
public:
    static RuntimeData compile(const RuntimeData::Map& request);
};

}  // namespace ludork::preview_host
