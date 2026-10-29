#pragma once

#include <unknwn.h>
#include <wrl/client.h>

#include <atomic>
#include <utility>

namespace ludork::runtime::webview {

template <typename Interface, typename Callback,
          typename Method = decltype(&Interface::Invoke)>
class WindowsWebViewCallbackImpl;

template <typename Interface, typename Callback, typename... Arguments>
class WindowsWebViewCallbackImpl<
    Interface, Callback, HRESULT (STDMETHODCALLTYPE Interface::*)(Arguments...)>
    final : public Interface {
public:
    WindowsWebViewCallbackImpl(const IID& interfaceId, Callback callback)
        : interfaceId_(interfaceId), callback_(std::move(callback)) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID interfaceId,
                                             void** object) override {
        if (object == nullptr) {
            return E_POINTER;
        }
        *object = nullptr;
        if (!IsEqualIID(interfaceId, __uuidof(IUnknown)) &&
            !IsEqualIID(interfaceId, interfaceId_)) {
            return E_NOINTERFACE;
        }
        *object = static_cast<Interface*>(this);
        AddRef();
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return ++referenceCount_;
    }

    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG remaining = --referenceCount_;
        if (remaining == 0) {
            delete this;
        }
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE Invoke(Arguments... arguments) override {
        return callback_(std::forward<Arguments>(arguments)...);
    }

private:
    std::atomic<ULONG> referenceCount_{1};
    IID interfaceId_;
    Callback callback_;
};

template <typename Interface, typename Callback>
Microsoft::WRL::ComPtr<Interface> makeWindowsWebViewCallback(
    const IID& interfaceId, Callback callback) {
    Microsoft::WRL::ComPtr<Interface> result;
    result.Attach(new WindowsWebViewCallbackImpl<Interface, Callback>(
        interfaceId, std::move(callback)));
    return result;
}

}  // namespace ludork::runtime::webview
