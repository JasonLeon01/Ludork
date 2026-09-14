#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>
#include <CurveKey.hpp>

BIND_CLASS()
class LUDORK_ENGINE_API Curve : public RuntimeObject {
public:
    LUDORK_CAST_DERIVED(Curve, RuntimeObject)

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

    BIND_INIT()
    explicit Curve(Curve::CurveData data);

    BIND_METHOD(Pure = true)
    static std::shared_ptr<Curve> fromData(const Curve::CurveData& data);

    BIND_METHOD(Pure = true)
    Curve::CurveData toData() const;

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
