#include <Gameplay/GameplayAbilityResult.hpp>
#include "GameplayValueUtils.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace {

bool resultCodeTruthy(const RuntimeValue& code) {
    if (const std::string* value = code.getIf<std::string>()) {
        return !value->empty();
    }
    if (const std::int64_t* value = code.getIf<std::int64_t>()) {
        return *value != 0;
    }
    if (const double* value = code.getIf<double>()) {
        return std::isfinite(*value) && *value != 0.0;
    }
    return false;
}

}  // namespace

GameplayAbilityResult::GameplayAbilityResult(bool succeeded,
                                             RuntimeValue resultCode,
                                             RuntimeIdentityPtr resultData)
    : ok(succeeded), code(std::move(resultCode)), data(std::move(resultData)) {
    if (code.isNil()) {
        code = RuntimeValue("");
    }
    if (data == nullptr) {
        data = ludork::global::gameplay_detail::runtimeMap();
    }
}

std::shared_ptr<GameplayAbilityResult> GameplayAbilityResult::Success(
    RuntimeValue code, RuntimeIdentityPtr data) {
    if (code.isNil()) {
        code = RuntimeValue("Success");
    }
    return std::make_shared<GameplayAbilityResult>(true, std::move(code),
                                                   std::move(data));
}

std::shared_ptr<GameplayAbilityResult> GameplayAbilityResult::Failure(
    RuntimeValue code, RuntimeIdentityPtr data) {
    if (!resultCodeTruthy(code)) {
        throw std::invalid_argument(
            "Gameplay Ability failure code must not be empty");
    }
    return std::make_shared<GameplayAbilityResult>(false, std::move(code),
                                                   std::move(data));
}
