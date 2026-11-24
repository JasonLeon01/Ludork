#pragma once

#include <StandardApi.hpp>

#include <cstddef>
#include <functional>

namespace ludork::standard {

LUDORK_STANDARD_API std::size_t fileWorkerCount();
LUDORK_STANDARD_API void parallelFiles(
    std::size_t count, const std::function<void(std::size_t)>& work);

}  // namespace ludork::standard
