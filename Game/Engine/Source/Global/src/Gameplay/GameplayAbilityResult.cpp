#include <Gameplay/GameplayAbilityResult.hpp>
#include "GameplayValueUtils.hpp"

#include <stdexcept>
#include <utility>

namespace {

bool resultCodeTruthy(const GameplayAbilityResult::Code& code) {
    if (const std::string* value = std::get_if<std::string>(&code)) {
        return !value->empty();
    }
    return std::get<std::int64_t>(code) != 0;
}

}  // namespace

GameplayAbilityResult::GameplayAbilityResult(bool succeeded,
                                             std::optional<Code> resultCode,
                                             RuntimeIdentityPtr resultData)
    : ok(succeeded),
      code(resultCode.value_or(Code(std::string{}))),
      data(std::move(resultData)) {
    if (data == nullptr) {
        data = ludork::global::gameplay_detail::runtimeMap();
    }
}

std::shared_ptr<GameplayAbilityResult> GameplayAbilityResult::Success(
    std::optional<Code> code, RuntimeIdentityPtr data) {
    return std::make_shared<GameplayAbilityResult>(
        true, code.value_or(Code(std::string("Success"))), std::move(data));
}

std::shared_ptr<GameplayAbilityResult> GameplayAbilityResult::Failure(
    Code code, RuntimeIdentityPtr data) {
    if (!resultCodeTruthy(code)) {
        throw std::invalid_argument(
            "Gameplay Ability failure code must not be empty");
    }
    return std::make_shared<GameplayAbilityResult>(false, std::move(code),
                                                   std::move(data));
}
