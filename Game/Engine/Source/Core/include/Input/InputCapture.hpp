#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>

namespace ludork::engine::input_impl {
class ModalInputImpl;
}

BIND_CLASS()
class LUDORK_ENGINE_API InputCapture {
public:
    ~InputCapture();

    InputCapture(const InputCapture&) = delete;
    InputCapture& operator=(const InputCapture&) = delete;

    BIND_METHOD()
    void setConfirmEnabled(bool enabled);

    BIND_METHOD()
    bool consumeConfirm();

    BIND_METHOD()
    void release();

private:
    friend class ludork::engine::input_impl::ModalInputImpl;

    InputCapture() = default;

    bool active_ = true;
    bool confirmEnabled_ = false;
    bool confirmed_ = false;
};
