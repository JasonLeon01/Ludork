#pragma once

#include <EngineRuntimeApi.hpp>

#include <filesystem>
#include <memory>
#include <string>

class Vector4Curve;

LUDORK_ENGINE_API std::shared_ptr<Vector4Curve> loadUiVector4CurveResource(
    const std::string& assetKey);

LUDORK_ENGINE_API std::shared_ptr<Vector4Curve> loadUiVector4CurveResource(
    const std::filesystem::path& projectRoot, const std::string& assetKey);

LUDORK_ENGINE_API void clearUiVector4CurveResourceCache() noexcept;
