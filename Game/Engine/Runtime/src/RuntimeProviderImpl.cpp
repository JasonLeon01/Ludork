#include "RuntimeProviderImpl.hpp"

#include "LuaServices/RuntimeBindingTraits.hpp"
#include <LudorkRuntimeBinding/FunctionAdapter.hpp>
#include <Runtime/NodeGraph/Graph.hpp>
#include <Runtime/RuntimeSession.hpp>

#include <stdexcept>
#include <utility>

namespace ludork::runtime::detail {

namespace {

template <typename Signature>
StrictFunction<Signature> providerCallback(const RuntimeIdentityPtr& identity,
                                           const std::string& name) {
    RuntimeScope runtime;
    if (!identity) {
        throw std::invalid_argument("Runtime " + name + " must not be nil");
    }
    const lua_glue::Object value = binding::writeOpaqueIdentity(
        lua_glue::StateView(runtime.state()), identity);
    if (!value.is<lua_glue::Function>()) {
        throw std::invalid_argument(
            "Runtime " + name + " must be a function from the active Lua VM");
    }
    return binding::strictFunctionFromLua<Signature>(value);
}

template <typename Signature>
StrictFunction<Signature> installedProvider(
    std::mutex& mutex, const StrictFunction<Signature>& provider,
    const char* name) {
    const std::lock_guard<std::mutex> lock(mutex);
    if (!provider) {
        throw std::runtime_error(std::string("Runtime ") + name +
                                 " is not installed");
    }
    return provider;
}

}  // namespace

RuntimeProviderImpl& RuntimeProviderImpl::instance() {
    static RuntimeProviderImpl providers;
    return providers;
}

void RuntimeProviderImpl::installBlueprint(
    const RuntimeIdentityPtr& classDataByPath,
    const RuntimeIdentityPtr& compileGraph,
    const RuntimeIdentityPtr& instantiateGraphTemplate) {
    RuntimeScope runtime;
    StrictFunction<ClassDataResolver> classData =
        providerCallback<ClassDataResolver>(classDataByPath,
                                            "Blueprint class data resolver");
    StrictFunction<GraphCompiler> compiler = providerCallback<GraphCompiler>(
        compileGraph, "Blueprint graph compiler");
    StrictFunction<GraphInstantiator> instantiator =
        providerCallback<GraphInstantiator>(
            instantiateGraphTemplate, "Blueprint graph template instantiator");
    const std::lock_guard<std::mutex> lock(mutex_);
    if (classData_ || compileGraph_ || instantiateGraph_) {
        throw std::runtime_error(
            "Runtime Blueprint class data resolver is already installed");
    }
    classData_ = std::move(classData);
    compileGraph_ = std::move(compiler);
    instantiateGraph_ = std::move(instantiator);
}

void RuntimeProviderImpl::installConfig(
    const RuntimeIdentityPtr& configResolver) {
    RuntimeScope runtime;
    StrictFunction<ConfigResolver> resolver =
        providerCallback<ConfigResolver>(configResolver, "config resolver");
    const std::lock_guard<std::mutex> lock(mutex_);
    if (config_) {
        throw std::runtime_error(
            "Runtime config resolver is already installed");
    }
    config_ = std::move(resolver);
}

void RuntimeProviderImpl::clear() noexcept {
    const std::lock_guard<std::mutex> lock(mutex_);
    classData_ = {};
    compileGraph_ = {};
    instantiateGraph_ = {};
    config_ = {};
}

RuntimeIdentityPtr RuntimeProviderImpl::classData(
    const std::string& classPath) const {
    RuntimeScope runtime;
    return installedProvider(mutex_, classData_,
                             "Blueprint class data resolver")(classPath);
}

std::shared_ptr<Graph> RuntimeProviderImpl::compileGraph(
    const RuntimeIdentityPtr& graphData,
    const RuntimeIdentityPtr& classType) const {
    RuntimeScope runtime;
    return installedProvider(mutex_, compileGraph_, "Blueprint graph compiler")(
        graphData, classType);
}

std::shared_ptr<Graph> RuntimeProviderImpl::instantiateGraph(
    const std::shared_ptr<Graph>& graphTemplate,
    const RuntimeIdentityPtr& parent) const {
    RuntimeScope runtime;
    return installedProvider(mutex_, instantiateGraph_,
                             "Blueprint graph template instantiator")(
        graphTemplate, parent);
}

std::string RuntimeProviderImpl::config(const std::string& configName,
                                        const std::string& settingName) const {
    RuntimeScope runtime;
    return installedProvider(mutex_, config_, "config resolver")(configName,
                                                                 settingName);
}

}  // namespace ludork::runtime::detail
