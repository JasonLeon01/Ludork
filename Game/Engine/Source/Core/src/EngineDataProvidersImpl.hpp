#pragma once

#include <EngineDataProviders.hpp>

#include <mutex>

struct EngineDataProviders::Impl {
    std::mutex mutex;
    CurveResolver curve;
    TextConfigResolver plainTextConfig;
};
