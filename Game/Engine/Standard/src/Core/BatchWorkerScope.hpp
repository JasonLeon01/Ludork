#pragma once

#include <functional>

namespace ludork::standard {

void runWithBatchWorkerScope(const std::function<void()>& work);

}  // namespace ludork::standard
