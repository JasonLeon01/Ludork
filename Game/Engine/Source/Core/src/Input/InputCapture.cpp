#include <Input/InputCapture.hpp>

#include "InputService/InputImpl.hpp"

InputCapture::~InputCapture() {
    release();
}

void InputCapture::setConfirmEnabled(bool enabled) {
    ludork::engine::input_impl::inputImpl().setCaptureConfirmEnabled(*this,
                                                                     enabled);
}

bool InputCapture::consumeConfirm() {
    return ludork::engine::input_impl::inputImpl().consumeCaptureConfirm(*this);
}

void InputCapture::release() {
    active_ = false;
    confirmEnabled_ = false;
    confirmed_ = false;
}
