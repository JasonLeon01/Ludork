#pragma once

#include <Runtime/RuntimeIdentity.hpp>
#include <Runtime/StrictFunction.hpp>

#include <memory>
#include <mutex>
#include <string>

class Graph;

namespace ludork::runtime::detail {

class RuntimeProviderImpl {
public:
    using ClassDataResolver = RuntimeIdentityPtr(const std::string&);
    using GraphCompiler = std::shared_ptr<Graph>(const RuntimeIdentityPtr&,
                                                 const RuntimeIdentityPtr&);
    using GraphInstantiator = std::shared_ptr<Graph>(
        const std::shared_ptr<Graph>&, const RuntimeIdentityPtr&);
    using ConfigResolver = std::string(const std::string&, const std::string&);

    static RuntimeProviderImpl& instance();

    void installBlueprint(const RuntimeIdentityPtr& classDataByPath,
                          const RuntimeIdentityPtr& compileGraph,
                          const RuntimeIdentityPtr& instantiateGraphTemplate);
    void installConfig(const RuntimeIdentityPtr& configResolver);
    void clear() noexcept;

    RuntimeIdentityPtr classData(const std::string& classPath) const;
    std::shared_ptr<Graph> compileGraph(
        const RuntimeIdentityPtr& graphData,
        const RuntimeIdentityPtr& classType) const;
    std::shared_ptr<Graph> instantiateGraph(
        const std::shared_ptr<Graph>& graphTemplate,
        const RuntimeIdentityPtr& parent) const;
    std::string config(const std::string& configName,
                       const std::string& settingName) const;

private:
    mutable std::mutex mutex_;
    StrictFunction<ClassDataResolver> classData_;
    StrictFunction<GraphCompiler> compileGraph_;
    StrictFunction<GraphInstantiator> instantiateGraph_;
    StrictFunction<ConfigResolver> config_;
};

}  // namespace ludork::runtime::detail
