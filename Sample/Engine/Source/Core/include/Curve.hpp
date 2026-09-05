#pragma once

#include <CoreMinimal.hpp>

#include <EngineRuntimeApi.hpp>

BIND_CLASS(copyable = true, table_init = true)
struct CurveKey {
    BIND_PROPERTY()
    float time = 0.0f;

    BIND_PROPERTY()
    float value = 0.0f;

    BIND_PROPERTY()
    std::string interpolation = "linear";

    BIND_PROPERTY()
    float arriveTangent = 0.0f;

    BIND_PROPERTY()
    float leaveTangent = 0.0f;
};

BIND_CLASS(copyable = true, table_init = true)
struct CurveData {
    BIND_PROPERTY()
    std::string type = "curve";

    BIND_PROPERTY()
    std::string name;

    BIND_PROPERTY()
    float defaultValue = 0.0f;

    BIND_PROPERTY()
    std::string preInfinity = "constant";

    BIND_PROPERTY()
    std::string postInfinity = "constant";

    BIND_PROPERTY()
    std::vector<CurveKey> keys;
};

BIND_CLASS()
class LUDORK_ENGINE_API Curve : public RuntimeObject {
public:
    BIND_INIT()
    explicit Curve(CurveData data);

    BIND_METHOD(Pure = true)
    static std::shared_ptr<Curve> fromData(const CurveData& data);

    BIND_METHOD(Pure = true)
    CurveData toData() const;

    BIND_METHOD(Pure = true)
    bool isEmpty() const;

    BIND_METHOD(Pure = true)
    float getDuration() const;

    BIND_METHOD(Pure = true)
    float evaluate(float time) const;

    BIND_PROPERTY()
    std::string name;

    BIND_PROPERTY()
    float defaultValue = 0.0f;

    BIND_PROPERTY()
    std::string preInfinity = "constant";

    BIND_PROPERTY()
    std::string postInfinity = "constant";

    BIND_PROPERTY()
    std::vector<CurveKey> keys;

private:
    static std::string normaliseInfinityMode(const std::string& mode);
    static std::string normaliseInterpolation(const std::string& mode);
    static float evaluateSegment(const CurveKey& start, const CurveKey& ending,
                                 float time);
    static float extrapolate(float time, const CurveKey& start,
                             const CurveKey& ending, const std::string& mode,
                             bool beforeFirst);
};

BIND_CLASS(copyable = true, table_init = true)
struct Vector2CurveKey {
    BIND_PROPERTY()
    float time = 0.0f;

    BIND_PROPERTY()
    std::array<float, 2> value = {0.0f, 0.0f};

    BIND_PROPERTY()
    std::string interpolation = "linear";

    BIND_PROPERTY()
    std::array<float, 2> arriveTangent = {0.0f, 0.0f};

    BIND_PROPERTY()
    std::array<float, 2> leaveTangent = {0.0f, 0.0f};
};

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

BIND_CLASS()
class LUDORK_ENGINE_API Vector2Curve : public RuntimeObject {
public:
    BIND_INIT()
    explicit Vector2Curve(Vector2CurveData data);

    BIND_METHOD(Pure = true)
    static std::shared_ptr<Vector2Curve> fromData(const Vector2CurveData& data);

    BIND_METHOD(Pure = true)
    Vector2CurveData toData() const;

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

BIND_CLASS(copyable = true, table_init = true)
struct Vector3CurveKey {
    BIND_PROPERTY()
    float time = 0.0f;

    BIND_PROPERTY()
    std::array<float, 3> value = {0.0f, 0.0f, 0.0f};

    BIND_PROPERTY()
    std::string interpolation = "linear";

    BIND_PROPERTY()
    std::array<float, 3> arriveTangent = {0.0f, 0.0f, 0.0f};

    BIND_PROPERTY()
    std::array<float, 3> leaveTangent = {0.0f, 0.0f, 0.0f};
};

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

BIND_CLASS()
class LUDORK_ENGINE_API Vector3Curve : public RuntimeObject {
public:
    BIND_INIT()
    explicit Vector3Curve(Vector3CurveData data);

    BIND_METHOD(Pure = true)
    static std::shared_ptr<Vector3Curve> fromData(const Vector3CurveData& data);

    BIND_METHOD(Pure = true)
    Vector3CurveData toData() const;

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

BIND_CLASS(copyable = true, table_init = true)
struct Vector4CurveKey {
    BIND_PROPERTY()
    float time = 0.0f;

    BIND_PROPERTY()
    std::array<float, 4> value = {0.0f, 0.0f, 0.0f, 0.0f};

    BIND_PROPERTY()
    std::string interpolation = "linear";

    BIND_PROPERTY()
    std::array<float, 4> arriveTangent = {0.0f, 0.0f, 0.0f, 0.0f};

    BIND_PROPERTY()
    std::array<float, 4> leaveTangent = {0.0f, 0.0f, 0.0f, 0.0f};
};

BIND_CLASS(copyable = true, table_init = true)
struct Vector4CurveData {
    BIND_PROPERTY()
    std::string type = "vector4Curve";

    BIND_PROPERTY()
    std::string name;

    BIND_PROPERTY()
    std::array<float, 4> defaultValue = {0.0f, 0.0f, 0.0f, 0.0f};

    BIND_PROPERTY()
    std::string preInfinity = "constant";

    BIND_PROPERTY()
    std::string postInfinity = "constant";

    BIND_PROPERTY()
    std::vector<Vector4CurveKey> keys;
};

BIND_CLASS()
class LUDORK_ENGINE_API Vector4Curve : public RuntimeObject {
public:
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

    BIND_PROPERTY()
    std::string preInfinity = "constant";

    BIND_PROPERTY()
    std::string postInfinity = "constant";

    BIND_PROPERTY()
    std::vector<Vector4CurveKey> keys;
};
