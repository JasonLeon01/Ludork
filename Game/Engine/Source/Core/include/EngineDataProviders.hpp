#pragma once

#include <EngineRuntimeApi.hpp>
#include <Runtime/StrictFunction.hpp>
#include <Curve.hpp>
#include <UI/PlainTextConfig.hpp>

#include <memory>
#include <string>

class LUDORK_ENGINE_API EngineDataProviders {
public:
    using CurveResolver =
        ludork::runtime::StrictFunction<std::shared_ptr<Curve>(
            const std::string&)>;
    using TextConfigResolver =
        ludork::runtime::StrictFunction<std::shared_ptr<PlainTextConfig>(
            const std::string&)>;

    static void install(CurveResolver curveResolver,
                        TextConfigResolver plainTextConfigResolver);
    static void clear() noexcept;

    static std::shared_ptr<Curve> curve(const std::string& name);
    static std::shared_ptr<PlainTextConfig> plainTextConfig(
        const std::string& name);

private:
    struct Impl;
    static Impl& impl();
};
