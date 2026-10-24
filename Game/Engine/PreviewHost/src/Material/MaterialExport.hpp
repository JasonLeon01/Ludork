#pragma once

#include <Runtime/RuntimeData.hpp>

namespace ludork::preview_host {

class MaterialExport {
public:
    static RuntimeData compile(const RuntimeData::Map& request);

private:
    static constexpr char CompiledResponse[] = "compiledMaterial";
};

}  // namespace ludork::preview_host
