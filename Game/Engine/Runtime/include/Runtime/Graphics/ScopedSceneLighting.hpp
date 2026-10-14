#pragma once

#include <Runtime/Graphics/SceneLighting.hpp>

namespace ludork::runtime::graphics {

class LUDORK_RUNTIME_API ScopedSceneLighting {
public:
    explicit ScopedSceneLighting(const SceneLightingContext& context);
    ~ScopedSceneLighting();
    ScopedSceneLighting(const ScopedSceneLighting&) = delete;
    ScopedSceneLighting& operator=(const ScopedSceneLighting&) = delete;

private:
    SceneLightingContext context_;
    const SceneLightingContext* previous_ = nullptr;
};

}  // namespace ludork::runtime::graphics
