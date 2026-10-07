#include <Runtime/Async/AsyncOperation.hpp>
#if LUDORK_WITH_LUA
#include <Runtime/RuntimeSession.hpp>
#endif

#include <algorithm>
#include <unordered_map>

namespace {

std::unordered_map<lua_State*, std::vector<std::shared_ptr<AsyncOperation>>>&
operations() {
    static std::unordered_map<lua_State*,
                              std::vector<std::shared_ptr<AsyncOperation>>>
        value;
    return value;
}

}  // namespace

AsyncOperation::AsyncOperation(lua_State* state,
                               std::function<void(AsyncOperation&)> poll)
    : state_(state), poll_(std::move(poll)) {}

std::shared_ptr<AsyncOperation> AsyncOperation::create(
    std::function<void(AsyncOperation&)> poll) {
#if LUDORK_WITH_LUA
    ludork::runtime::RuntimeScope scope;
    lua_State* state = scope.state();
#else
    lua_State* state = nullptr;
#endif
    std::shared_ptr<AsyncOperation> operation(
        new AsyncOperation(state, std::move(poll)));
    operations()[state].push_back(operation);
    return operation;
}

void AsyncOperation::emit(const RuntimeValue& value) {
    if (isPending()) {
        events_.push_back(value);
    }
}

void AsyncOperation::complete(const RuntimeValue& result) {
    if (!isPending()) {
        return;
    }
    result_ = result;
    events_.push_back(result);
    status_ = "completed";
    cancellationCallbacks_.clear();
    releasePolling();
}

void AsyncOperation::cancel() {
    if (!isPending()) {
        return;
    }
    status_ = "cancelled";
    events_.clear();
    const std::vector<std::function<void()>> callbacks =
        std::move(cancellationCallbacks_);
    releasePolling();
    for (const auto& callback : callbacks) {
        callback();
    }
}

void AsyncOperation::onCancelled(std::function<void()> callback) {
    if (status_ == "cancelled") {
        callback();
    } else if (isPending()) {
        cancellationCallbacks_.push_back(std::move(callback));
    }
}

void AsyncOperation::releasePolling() {
    poll_ = {};
    const auto found = operations().find(state_);
    if (found != operations().end()) {
        std::erase_if(found->second, [this](const auto& operation) {
            return operation.get() == this;
        });
    }
}

std::string AsyncOperation::getStatus() const {
    return status_;
}

RuntimeValue AsyncOperation::getResult() const {
    if (status_ != "completed") {
        throw std::logic_error(
            "AsyncOperation result requires completed status");
    }
    return result_;
}

bool AsyncOperation::isPending() const noexcept {
    return status_ == "pending";
}

std::size_t AsyncOperation::eventCount() const noexcept {
    return events_.size();
}

RuntimeValue AsyncOperation::eventAt(std::size_t index) const {
    return events_.at(index);
}

void AsyncOperation::updateAll() {
#if LUDORK_WITH_LUA
    ludork::runtime::RuntimeScope scope;
    lua_State* state = scope.state();
#else
    lua_State* state = nullptr;
#endif
    const auto found = operations().find(state);
    if (found == operations().end()) {
        return;
    }
    const std::vector<std::shared_ptr<AsyncOperation>> snapshot = found->second;
    for (const std::shared_ptr<AsyncOperation>& operation : snapshot) {
        if (operation->isPending() && operation->poll_) {
            const std::function<void(AsyncOperation&)> poll = operation->poll_;
            poll(*operation);
        }
    }
}

void AsyncOperation::clearAll(lua_State* state) noexcept {
    const auto found = operations().find(state);
    if (found == operations().end()) {
        return;
    }
    const std::vector<std::shared_ptr<AsyncOperation>> snapshot =
        std::move(found->second);
    operations().erase(found);
    for (const std::shared_ptr<AsyncOperation>& operation : snapshot) {
        operation->cancel();
    }
}
