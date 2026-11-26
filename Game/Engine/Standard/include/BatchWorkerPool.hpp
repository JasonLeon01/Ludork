#pragma once

#include <StandardApi.hpp>

#include <cstddef>
#include <functional>
#include <memory>

namespace ludork::standard {

class LUDORK_STANDARD_API BatchWorkerPool {
public:
    BatchWorkerPool();
    ~BatchWorkerPool();

    void run(std::size_t count,
             const std::function<void(std::size_t)>& work) const;

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};

}  // namespace ludork::standard
