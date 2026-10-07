#pragma once

#include <Runtime/RuntimeValue.hpp>

struct lua_State;

BIND_CLASS()
class LUDORK_RUNTIME_API AsyncOperation : public RuntimeObject {
public:
    LUDORK_CAST_DERIVED(AsyncOperation, RuntimeObject)

    /// Create an operation; an optional poll runs once per logic frame until
    /// settlement.
    BIND_METHOD(name = "new", metadata = false, defaults = {nil},
                nonnull_return = true)
    static std::shared_ptr<AsyncOperation> create(
        std::function<void(AsyncOperation&)> poll = {});

    /// Publish an intermediate Blueprint execution value without completing the
    /// operation.
    BIND_METHOD(metadata = false)
    void emit(const RuntimeValue& value);

    /// Complete once, retaining the result for every waiter, including late
    /// waiters.
    BIND_METHOD(metadata = false, defaults = {nil})
    void complete(const RuntimeValue& result = RuntimeValue());

    /// Cancel pending waiters without completing their execution flow.
    BIND_METHOD(metadata = false)
    void cancel();

    /// Return pending, completed or cancelled.
    BIND_METHOD(metadata = false)
    std::string getStatus() const;

    /// Read the completed result; pending and cancelled operations raise an
    /// error.
    BIND_METHOD(metadata = false)
    RuntimeValue getResult() const;

    void onCancelled(std::function<void()> callback);
    bool isPending() const noexcept;
    std::size_t eventCount() const noexcept;
    RuntimeValue eventAt(std::size_t index) const;
    static void updateAll();
    static void clearAll(lua_State* state) noexcept;

private:
    explicit AsyncOperation(lua_State* state,
                            std::function<void(AsyncOperation&)> poll);
    void releasePolling();

    lua_State* state_;
    std::string status_ = "pending";
    std::function<void(AsyncOperation&)> poll_;
    RuntimeValue result_;
    std::vector<std::function<void()>> cancellationCallbacks_;
    std::vector<RuntimeValue> events_;
};
