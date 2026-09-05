#include <UI/UiVector4CurveResource.hpp>

#include <Runtime/ConcurrentResourceCache.hpp>
#include <Curve.hpp>
#include <Runtime/RuntimeValueReader.hpp>
#include <Runtime/Json.hpp>

#include <Utf8Path.hpp>

#include <array>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

ludork::runtime::ConcurrentResourceCache<Curve, true>& scalarCurveCache() {
    static ludork::runtime::ConcurrentResourceCache<Curve, true> cache;
    return cache;
}

ludork::runtime::ConcurrentResourceCache<Vector4Curve, true>&
vector4CurveCache() {
    static ludork::runtime::ConcurrentResourceCache<Vector4Curve, true> cache;
    return cache;
}

using ludork::runtime::value_reader::findValue;
using ludork::runtime::value_reader::requireArray;
using ludork::runtime::value_reader::requireFloat;
using ludork::runtime::value_reader::requireMap;
using ludork::runtime::value_reader::requireString;

std::array<float, 4> requireVector4(const RuntimeData& value,
                                    const std::string& source) {
    const RuntimeData::Array& array = requireArray(value, source);
    if (array.size() != 4) {
        throw std::invalid_argument(source + " must have four components");
    }
    return {
        requireFloat(array[0], source + "[0]"),
        requireFloat(array[1], source + "[1]"),
        requireFloat(array[2], source + "[2]"),
        requireFloat(array[3], source + "[3]"),
    };
}

float floatValue(const RuntimeData::Map& values, const std::string& name,
                 float fallback, const std::string& source) {
    const RuntimeData* value = findValue(values, name);
    return value == nullptr ? fallback
                            : requireFloat(*value, source + "." + name);
}

std::string stringValue(const RuntimeData::Map& values, const std::string& name,
                        const std::string& fallback,
                        const std::string& source) {
    const RuntimeData* value = findValue(values, name);
    return value == nullptr ? fallback
                            : requireString(*value, source + "." + name);
}

std::array<float, 4> vector4Value(const RuntimeData::Map& values,
                                  const std::string& name,
                                  const std::array<float, 4>& fallback,
                                  const std::string& source) {
    const RuntimeData* value = findValue(values, name);
    return value == nullptr ? fallback
                            : requireVector4(*value, source + "." + name);
}

CurveKey scalarCurveKey(const RuntimeData& value, const std::string& source) {
    const RuntimeData::Map& values = requireMap(value, source);
    CurveKey result;
    result.time = floatValue(values, "time", result.time, source);
    result.value = floatValue(values, "value", result.value, source);
    result.interpolation =
        stringValue(values, "interpolation", result.interpolation, source);
    result.arriveTangent =
        floatValue(values, "arriveTangent", result.arriveTangent, source);
    result.leaveTangent =
        floatValue(values, "leaveTangent", result.leaveTangent, source);
    return result;
}

CurveData scalarCurveData(const RuntimeData& value, const std::string& source) {
    const RuntimeData::Map& values = requireMap(value, source);
    const RuntimeData* type = findValue(values, "type");
    if (type == nullptr || requireString(*type, source + ".type") != "curve") {
        throw std::invalid_argument("UI curve must have type curve: " + source);
    }
    CurveData result;
    result.name = stringValue(values, "name", result.name, source);
    result.defaultValue =
        floatValue(values, "defaultValue", result.defaultValue, source);
    result.preInfinity =
        stringValue(values, "preInfinity", result.preInfinity, source);
    result.postInfinity =
        stringValue(values, "postInfinity", result.postInfinity, source);
    if (const RuntimeData* keys = findValue(values, "keys")) {
        const RuntimeData::Array& array = requireArray(*keys, source + ".keys");
        result.keys.reserve(array.size());
        for (std::size_t index = 0; index < array.size(); ++index) {
            result.keys.push_back(scalarCurveKey(
                array[index], source + ".keys[" + std::to_string(index) + "]"));
        }
    }
    return result;
}

Vector4CurveKey vector4CurveKey(const RuntimeData& value,
                                const std::string& source) {
    const RuntimeData::Map& values = requireMap(value, source);
    Vector4CurveKey result;
    if (const RuntimeData* time = findValue(values, "time")) {
        result.time = requireFloat(*time, source + ".time");
    }
    result.value = vector4Value(values, "value", result.value, source);
    result.interpolation =
        stringValue(values, "interpolation", result.interpolation, source);
    result.arriveTangent =
        vector4Value(values, "arriveTangent", result.arriveTangent, source);
    result.leaveTangent =
        vector4Value(values, "leaveTangent", result.leaveTangent, source);
    return result;
}

Vector4CurveData vector4CurveData(const RuntimeData& value,
                                  const std::string& source) {
    const RuntimeData::Map& values = requireMap(value, source);
    const RuntimeData* type = findValue(values, "type");
    if (type == nullptr ||
        requireString(*type, source + ".type") != "vector4Curve") {
        throw std::invalid_argument(
            "UI gradient curve must have type vector4Curve: " + source);
    }
    Vector4CurveData result;
    result.name = stringValue(values, "name", result.name, source);
    result.defaultValue =
        vector4Value(values, "defaultValue", result.defaultValue, source);
    result.preInfinity =
        stringValue(values, "preInfinity", result.preInfinity, source);
    result.postInfinity =
        stringValue(values, "postInfinity", result.postInfinity, source);
    if (const RuntimeData* keys = findValue(values, "keys")) {
        const RuntimeData::Array& array = requireArray(*keys, source + ".keys");
        result.keys.reserve(array.size());
        for (std::size_t index = 0; index < array.size(); ++index) {
            result.keys.push_back(vector4CurveKey(
                array[index], source + ".keys[" + std::to_string(index) + "]"));
        }
    }
    return result;
}

bool withinRoot(const std::filesystem::path& root,
                const std::filesystem::path& candidate) {
    auto rootIterator = root.begin();
    auto candidateIterator = candidate.begin();
    while (rootIterator != root.end()) {
        if (candidateIterator == candidate.end() ||
            *rootIterator != *candidateIterator) {
            return false;
        }
        ++rootIterator;
        ++candidateIterator;
    }
    return true;
}

std::filesystem::path curvePath(const std::filesystem::path& projectRoot,
                                const std::string& assetKey) {
    if (assetKey.empty() || assetKey.find('\0') != std::string::npos) {
        throw std::invalid_argument("UI curve asset key must be non-empty");
    }
    std::filesystem::path relative =
        ludork::standard::pathFromUtf8(assetKey).lexically_normal();
    if (relative.empty() || relative == "." || relative.is_absolute() ||
        relative.has_root_name() || relative.has_root_directory()) {
        throw std::invalid_argument("UI curve asset key must be relative: " +
                                    assetKey);
    }
    for (const std::filesystem::path& part : relative) {
        if (part == "..") {
            throw std::invalid_argument(
                "UI curve asset key cannot traverse directories: " + assetKey);
        }
    }
    if (relative.extension().empty()) {
        relative += ".json";
    } else if (relative.extension() != ".json") {
        throw std::invalid_argument("UI curve asset key must reference JSON: " +
                                    assetKey);
    }
    const std::filesystem::path root =
        std::filesystem::weakly_canonical(projectRoot / "Data" / "Curves");
    const std::filesystem::path candidate =
        std::filesystem::weakly_canonical(root / relative);
    if (!withinRoot(root, candidate)) {
        throw std::invalid_argument("UI curve asset key escapes Data/Curves: " +
                                    assetKey);
    }
    if (!std::filesystem::is_regular_file(candidate)) {
        std::filesystem::path encrypted = candidate;
        encrypted.replace_extension(".ldc");
        encrypted = std::filesystem::weakly_canonical(encrypted);
        if (!withinRoot(root, encrypted)) {
            throw std::invalid_argument(
                "UI curve asset key escapes Data/Curves: " + assetKey);
        }
    }
    if (!jsonExists(candidate)) {
        throw std::runtime_error("UI curve was not found: " +
                                 ludork::standard::pathToUtf8(candidate));
    }
    return candidate;
}

}  // namespace

std::shared_ptr<Curve> loadUiCurveResource(const std::string& assetKey) {
    return loadUiCurveResource(std::filesystem::current_path(), assetKey);
}

std::shared_ptr<Curve> loadUiCurveResource(
    const std::filesystem::path& projectRoot, const std::string& assetKey) {
    if (!std::filesystem::is_directory(projectRoot)) {
        throw std::invalid_argument("UI curve project root is not a directory");
    }
    const std::filesystem::path path =
        curvePath(std::filesystem::weakly_canonical(projectRoot), assetKey);
    const std::string cacheKey = ludork::standard::pathToUtf8(path);
    return scalarCurveCache().getOrLoad(cacheKey, [&]() {
        return Curve::fromData(scalarCurveData(getJSONData(path), cacheKey));
    });
}

std::shared_ptr<Vector4Curve> loadUiVector4CurveResource(
    const std::string& assetKey) {
    return loadUiVector4CurveResource(std::filesystem::current_path(),
                                      assetKey);
}

std::shared_ptr<Vector4Curve> loadUiVector4CurveResource(
    const std::filesystem::path& projectRoot, const std::string& assetKey) {
    if (!std::filesystem::is_directory(projectRoot)) {
        throw std::invalid_argument("UI curve project root is not a directory");
    }
    const std::filesystem::path path =
        curvePath(std::filesystem::weakly_canonical(projectRoot), assetKey);
    const std::string cacheKey = ludork::standard::pathToUtf8(path);
    return vector4CurveCache().getOrLoad(cacheKey, [&]() {
        return Vector4Curve::fromData(
            vector4CurveData(getJSONData(path), cacheKey));
    });
}

void clearUiVector4CurveResourceCache() noexcept {
    scalarCurveCache().clear();
    vector4CurveCache().clear();
}
