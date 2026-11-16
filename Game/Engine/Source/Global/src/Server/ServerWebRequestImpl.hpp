#pragma once

#include "ServerRequestData.hpp"

namespace ludork::global::server_impl {

class ServerWebRequestImpl {
public:
    explicit ServerWebRequestImpl(std::shared_ptr<PendingRequest> pending);
    ~ServerWebRequestImpl();
    bool poll();

private:
    std::shared_ptr<PendingRequest> pending_;
    std::string project_;
    int handle_ = 0;
};

}  // namespace ludork::global::server_impl
