#include <EngineDataProviders.hpp>

#include "EngineDataProvidersImpl.hpp"
#include <Curve.hpp>
#include <Runtime/RuntimeSession.hpp>
#include <UI/PlainTextConfig.hpp>

#include <stdexcept>
#include <utility>

EngineDataProviders::Impl& EngineDataProviders::impl() {
    static Impl providers;
    return providers;
}

void EngineDataProviders::install(CurveResolver curveResolver,
                                  TextConfigResolver plainTextConfigResolver) {
    ludork::runtime::RuntimeScope runtime;
    if (!curveResolver) {
        throw std::invalid_argument("Runtime curve resolver must not be nil");
    }
    if (!plainTextConfigResolver) {
        throw std::invalid_argument(
            "Runtime plain text config resolver must not be nil");
    }
    Impl& state = impl();
    const std::lock_guard<std::mutex> lock(state.mutex);
    if (state.curve || state.plainTextConfig) {
        throw std::runtime_error("Runtime curve resolver is already installed");
    }
    state.curve = std::move(curveResolver);
    state.plainTextConfig = std::move(plainTextConfigResolver);
}

void EngineDataProviders::clear() noexcept {
    Impl& state = impl();
    const std::lock_guard<std::mutex> lock(state.mutex);
    state.curve = {};
    state.plainTextConfig = {};
}

std::shared_ptr<Curve> EngineDataProviders::curve(const std::string& name) {
    ludork::runtime::RuntimeScope runtime;
    CurveResolver resolver;
    {
        Impl& state = impl();
        const std::lock_guard<std::mutex> lock(state.mutex);
        resolver = state.curve;
    }
    if (!resolver) {
        throw std::runtime_error("Runtime curve resolver is not installed");
    }
    return resolver(name);
}

std::shared_ptr<PlainTextConfig> EngineDataProviders::plainTextConfig(
    const std::string& name) {
    ludork::runtime::RuntimeScope runtime;
    TextConfigResolver resolver;
    {
        Impl& state = impl();
        const std::lock_guard<std::mutex> lock(state.mutex);
        resolver = state.plainTextConfig;
    }
    if (!resolver) {
        throw std::runtime_error(
            "Runtime plain text config resolver is not installed");
    }
    return resolver(name);
}
