#pragma once

#include <Runtime/Detail/RuntimeServices.hpp>

namespace ludork::runtime::detail {

void clearRuntimeCommonCaches(sol::state_view lua);

}  // namespace ludork::runtime::detail
