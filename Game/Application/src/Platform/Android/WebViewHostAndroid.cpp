#include "WebViewHostAndroid.hpp"
#include "WebViewHostAndroidImpl.hpp"

#include <SFML/System/NativeActivity.hpp>
#include <SFML/System/String.hpp>

#include <memory>

namespace ludork::application {

WebViewHostAndroidImpl::JniEnvironment::JniEnvironment(JavaVM* vm) : vm_(vm) {
    if (vm_ == nullptr) {
        return;
    }
    const jint result =
        vm_->GetEnv(reinterpret_cast<void**>(&environment_), JNI_VERSION_1_6);
    if (result == JNI_EDETACHED) {
        attached_ = vm_->AttachCurrentThread(&environment_, nullptr) == JNI_OK;
        if (!attached_) {
            environment_ = nullptr;
        }
    } else if (result != JNI_OK) {
        environment_ = nullptr;
    }
}

WebViewHostAndroidImpl::JniEnvironment::~JniEnvironment() {
    if (attached_) {
        vm_->DetachCurrentThread();
    }
}

JNIEnv* WebViewHostAndroidImpl::JniEnvironment::get() const {
    return environment_;
}

WebViewHostAndroidImpl::WebViewHostAndroidImpl(ANativeActivity* activity) {
    if (activity == nullptr) {
        return;
    }
    vm_ = activity->vm;
    JniEnvironment attached(vm_);
    if (JNIEnv* environment = attached.get()) {
        activity_ = environment->NewGlobalRef(activity->clazz);
        if (environment->ExceptionCheck()) {
            environment->ExceptionClear();
        }
    }
}

WebViewHostAndroidImpl::~WebViewHostAndroidImpl() {
    close(session_);
    JniEnvironment attached(vm_);
    if (JNIEnv* environment = attached.get(); environment && activity_) {
        environment->DeleteGlobalRef(activity_);
    }
}

bool WebViewHostAndroidImpl::open(std::uint64_t id, const std::string& url) {
    const std::lock_guard<std::mutex> lock(mutex_);
    JniEnvironment attached(vm_);
    JNIEnv* environment = attached.get();
    if (environment == nullptr || activity_ == nullptr) {
        return false;
    }
    jclass activityClass = environment->GetObjectClass(activity_);
    jmethodID method =
        activityClass == nullptr
            ? nullptr
            : environment->GetMethodID(activityClass, "showWebView",
                                       "(JLjava/lang/String;)V");
    const std::u16string value =
        sf::String::fromUtf8(url.begin(), url.end()).toUtf16();
    jstring address = method == nullptr
                          ? nullptr
                          : environment->NewString(
                                reinterpret_cast<const jchar*>(value.data()),
                                static_cast<jsize>(value.size()));
    if (address != nullptr) {
        environment->CallVoidMethod(activity_, method, static_cast<jlong>(id),
                                    address);
    }
    const bool success = address != nullptr && !environment->ExceptionCheck();
    if (environment->ExceptionCheck()) {
        environment->ExceptionClear();
    }
    if (address != nullptr) {
        environment->DeleteLocalRef(address);
    }
    if (activityClass != nullptr) {
        environment->DeleteLocalRef(activityClass);
    }
    if (success) {
        session_ = id;
    }
    return success;
}

void WebViewHostAndroidImpl::close(std::uint64_t id) {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (id == 0 || session_ != id) {
        return;
    }
    session_ = 0;
    JniEnvironment attached(vm_);
    JNIEnv* environment = attached.get();
    if (environment == nullptr || activity_ == nullptr) {
        return;
    }
    jclass activityClass = environment->GetObjectClass(activity_);
    jmethodID method =
        activityClass == nullptr
            ? nullptr
            : environment->GetMethodID(activityClass, "dismissWebView", "(J)V");
    if (method != nullptr) {
        environment->CallVoidMethod(activity_, method, static_cast<jlong>(id));
    }
    if (environment->ExceptionCheck()) {
        environment->ExceptionClear();
    }
    if (activityClass != nullptr) {
        environment->DeleteLocalRef(activityClass);
    }
}

void configureAndroidWebViewHost() {
    ludork::runtime::webview::setHostFactory([](sf::WindowHandle) {
        return std::make_shared<WebViewHostAndroidImpl>(
            sf::getNativeActivity());
    });
}

}  // namespace ludork::application

extern "C" JNIEXPORT void JNICALL
Java_com_ludork_android_LudorkActivity_completeWebView(JNIEnv*, jclass,
                                                       jlong id) {
    ludork::runtime::webview::notifyClosed(static_cast<std::uint64_t>(id));
}
