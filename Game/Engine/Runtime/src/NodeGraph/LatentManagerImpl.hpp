#pragma once

#include <Runtime/NodeGraph/Graph.hpp>

namespace ludork::runtime::latent_detail {

class UpdateScope {
public:
    explicit UpdateScope(bool& updating);
    ~UpdateScope();
    UpdateScope(const UpdateScope&) = delete;
    UpdateScope& operator=(const UpdateScope&) = delete;

private:
    bool& updating_;
};

class LocalGraphScope {
public:
    LocalGraphScope(Graph& graph, RuntimeIdentityPtr replacement,
                    std::string eventKey);
    ~LocalGraphScope() noexcept;
    LocalGraphScope(const LocalGraphScope&) = delete;
    LocalGraphScope& operator=(const LocalGraphScope&) = delete;

private:
    Graph& graph_;
    RuntimeIdentityPtr previous_;
    RuntimeIdentityPtr context_;
    std::string eventKey_;
    RuntimeValue previousContextGraph_;
    bool contextGraphSet_ = false;
};

}  // namespace ludork::runtime::latent_detail
