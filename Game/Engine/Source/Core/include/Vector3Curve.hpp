#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>
#include <Vector3CurveKey.hpp>

BIND_CLASS()
class LUDORK_ENGINE_API Vector3Curve : public RuntimeObject {
public:
    LUDORK_CAST_DERIVED(Vector3Curve, RuntimeObject)

    BIND_CLASS(copyable = true, table_init = true)
    struct Vector3CurveData {
        BIND_PROPERTY()
        std::string type = "vector3Curve";

        BIND_PROPERTY()
        std::string name;

        BIND_PROPERTY()
        std::array<float, 3> defaultValue = {0.0f, 0.0f, 0.0f};

        BIND_PROPERTY()
        std::string preInfinity = "constant";

        BIND_PROPERTY()
        std::string postInfinity = "constant";

        BIND_PROPERTY()
        std::vector<Vector3CurveKey> keys;
    };

    BIND_INIT()
    explicit Vector3Curve(Vector3Curve::Vector3CurveData data);

    BIND_METHOD(Pure = true)
    static std::shared_ptr<Vector3Curve> fromData(
        const Vector3Curve::Vector3CurveData& data);

    BIND_METHOD(Pure = true)
    Vector3Curve::Vector3CurveData toData() const;

    BIND_METHOD(Pure = true)
    bool isEmpty() const;

    BIND_METHOD(Pure = true)
    float getDuration() const;

    BIND_METHOD(Pure = true)
    std::array<float, 3> evaluate(float time) const;

    BIND_PROPERTY()
    std::string name;

    BIND_PROPERTY()
    std::array<float, 3> defaultValue = {0.0f, 0.0f, 0.0f};

    BIND_PROPERTY()
    std::string preInfinity = "constant";

    BIND_PROPERTY()
    std::string postInfinity = "constant";

    BIND_PROPERTY()
    std::vector<Vector3CurveKey> keys;
};
