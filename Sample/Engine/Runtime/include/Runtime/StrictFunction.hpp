#pragma once

#include <functional>
#include <memory>
#include <utility>

struct lua_State;

namespace ludork::runtime {

template <typename Signature>
class StrictFunction;

template <typename Return, typename... Arguments>
class StrictFunction<Return(Arguments...)> {
public:
    class Reference {
    public:
        virtual ~Reference() = default;
        virtual bool push(lua_State* state) const = 0;
    };

    StrictFunction() = default;
    StrictFunction(std::function<Return(Arguments...)> function,
                   std::shared_ptr<Reference> reference = {})
        : function_(std::move(function)), reference_(std::move(reference)) {}

    explicit operator bool() const noexcept {
        return static_cast<bool>(function_);
    }

    Return operator()(Arguments... arguments) const {
        return function_(std::forward<Arguments>(arguments)...);
    }

    const std::function<Return(Arguments...)>& function() const noexcept {
        return function_;
    }

    const std::shared_ptr<Reference>& reference() const noexcept {
        return reference_;
    }

private:
    std::function<Return(Arguments...)> function_;
    std::shared_ptr<Reference> reference_;
};

}  // namespace ludork::runtime
