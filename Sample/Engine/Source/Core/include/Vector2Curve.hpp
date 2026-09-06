#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>
#include <Vector2CurveKey.hpp>

BIND_CLASS()
class LUDORK_ENGINE_API Vector2Curve : public RuntimeObject {
public:
    BIND_CLASS(copyable = true, table_init = true)
    struct Vector2CurveData {
        BIND_PROPERTY()
        std::string type = "vector2Curve";

        BIND_PROPERTY()
        std::string name;

        BIND_PROPERTY()
        std::array<float, 2> defaultValue = {0.0f, 0.0f};

        BIND_PROPERTY()
        std::string preInfinity = "constant";

        BIND_PROPERTY()
        std::string postInfinity = "constant";

        BIND_PROPERTY()
        std::vector<Vector2CurveKey> keys;
    };

    BIND_INIT()
    explicit Vector2Curve(Vector2Curve::Vector2CurveData data);

    BIND_METHOD(Pure = true)
    static std::shared_ptr<Vector2Curve> fromData(
        const Vector2Curve::Vector2CurveData& data);

    BIND_METHOD(Pure = true)
    Vector2Curve::Vector2CurveData toData() const;

    BIND_METHOD(Pure = true)
    bool isEmpty() const;

    BIND_METHOD(Pure = true)
    float getDuration() const;

    BIND_METHOD(Pure = true)
    std::array<float, 2> evaluate(float time) const;

    BIND_PROPERTY()
    std::string name;

    BIND_PROPERTY()
    std::array<float, 2> defaultValue = {0.0f, 0.0f};

    BIND_PROPERTY()
    std::string preInfinity = "constant";

    BIND_PROPERTY()
    std::string postInfinity = "constant";

    BIND_PROPERTY()
    std::vector<Vector2CurveKey> keys;
};
