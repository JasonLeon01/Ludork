#pragma once

#include <CurveInfinityMode.hpp>

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>
#include <Vector4CurveKey.hpp>

struct Vector4CurveData;

BIND_CLASS()
class LUDORK_ENGINE_API Vector4Curve : public RuntimeObject {
public:
    LUDORK_CAST_DERIVED(Vector4Curve, RuntimeObject)

    BIND_INIT()
    explicit Vector4Curve(Vector4CurveData data);

    BIND_METHOD(Pure = true)
    static std::shared_ptr<Vector4Curve> fromData(const Vector4CurveData& data);

    BIND_METHOD(Pure = true)
    Vector4CurveData toData() const;

    BIND_METHOD(Pure = true)
    bool isEmpty() const;

    BIND_METHOD(Pure = true)
    float getDuration() const;

    BIND_METHOD(Pure = true)
    std::array<float, 4> evaluate(float time) const;

    BIND_PROPERTY()
    std::string name;

    BIND_PROPERTY()
    std::array<float, 4> defaultValue = {0.0f, 0.0f, 0.0f, 0.0f};

    BIND_PROPERTY(default = "constant")
    std::string preInfinity = CurveInfinityMode::CONSTANT;

    BIND_PROPERTY(default = "constant")
    std::string postInfinity = CurveInfinityMode::CONSTANT;

    BIND_PROPERTY()
    std::vector<Vector4CurveKey> keys;
};
