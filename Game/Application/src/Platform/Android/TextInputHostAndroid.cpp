#include "TextInputHostAndroidImpl.hpp"

#include <SFML/System/NativeActivity.hpp>
#include <SFML/System/String.hpp>

#include <array>
#include <utility>

namespace {

std::mutex hostMutex;
std::weak_ptr<ludork::application::TextInputHostAndroidImpl> currentHost;

jstring nativeString(JNIEnv* environment, const std::string& text) {
    const std::u16string value =
        sf::String::fromUtf8(text.begin(), text.end()).toUtf16();
    return environment->NewString(reinterpret_cast<const jchar*>(value.data()),
                                  static_cast<jsize>(value.size()));
}

}  // namespace

namespace ludork::application {

TextInputHostAndroidImpl::JniEnvironment::JniEnvironment(JavaVM* vm) : vm_(vm) {
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

TextInputHostAndroidImpl::JniEnvironment::~JniEnvironment() {
    if (attached_) {
        vm_->DetachCurrentThread();
    }
}

JNIEnv* TextInputHostAndroidImpl::JniEnvironment::get() const {
    return environment_;
}

TextInputHostAndroidImpl::TextInputHostAndroidImpl(ANativeActivity* activity)
    : activity_(activity) {}

TextInputHostAndroidImpl::~TextInputHostAndroidImpl() {
    end(session_);
}

bool TextInputHostAndroidImpl::isModal() const {
    return true;
}

bool TextInputHostAndroidImpl::handlesKeyboard() const {
    return true;
}

bool TextInputHostAndroidImpl::begin(
    ludork::engine::text_input::SessionId id,
    const ludork::engine::text_input::Request& request, Sink sink) {
    if (activity_ == nullptr) {
        return false;
    }
    JniEnvironment attached(activity_->vm);
    JNIEnv* environment = attached.get();
    if (environment == nullptr) {
        return false;
    }
    jclass activityClass = environment->GetObjectClass(activity_->clazz);
    jmethodID method = environment->GetMethodID(
        activityClass, "showTextInput",
        "(JLjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/"
        "String;Ljava/lang/String;Ljava/lang/String;)V");
    if (method == nullptr) {
        environment->ExceptionClear();
        environment->DeleteLocalRef(activityClass);
        return false;
    }
    const std::array<jstring, 6> strings{
        nativeString(environment, request.state.text),
        nativeString(environment, request.title),
        nativeString(environment, request.prompt),
        nativeString(environment, request.placeholder),
        nativeString(environment, request.confirmText),
        nativeString(environment, request.cancelText)};
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        session_ = id;
        sink_ = std::move(sink);
    }
    environment->CallVoidMethod(activity_->clazz, method,
                                static_cast<jlong>(id), strings[0], strings[1],
                                strings[2], strings[3], strings[4], strings[5]);
    const bool success = !environment->ExceptionCheck();
    if (!success) {
        environment->ExceptionClear();
        const std::lock_guard<std::mutex> lock(mutex_);
        session_ = 0;
        sink_ = {};
    }
    for (jstring value : strings) {
        environment->DeleteLocalRef(value);
    }
    environment->DeleteLocalRef(activityClass);
    return success;
}

void TextInputHostAndroidImpl::update(ludork::engine::text_input::SessionId,
                                      const ludork::engine::text_input::State&,
                                      const sf::FloatRect&) {}

void TextInputHostAndroidImpl::end(ludork::engine::text_input::SessionId id) {
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (session_ != id || session_ == 0) {
            return;
        }
        session_ = 0;
        sink_ = {};
    }
    if (activity_ == nullptr) {
        return;
    }
    JniEnvironment attached(activity_->vm);
    JNIEnv* environment = attached.get();
    if (environment == nullptr) {
        return;
    }
    jclass activityClass = environment->GetObjectClass(activity_->clazz);
    jmethodID method =
        environment->GetMethodID(activityClass, "dismissTextInput", "(J)V");
    if (method != nullptr) {
        environment->CallVoidMethod(activity_->clazz, method,
                                    static_cast<jlong>(id));
    }
    if (environment->ExceptionCheck()) {
        environment->ExceptionClear();
    }
    environment->DeleteLocalRef(activityClass);
}

void TextInputHostAndroidImpl::complete(
    ludork::engine::text_input::SessionId id, bool accepted,
    const std::string& text) {
    Sink callback;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (session_ != id || session_ == 0) {
            return;
        }
        session_ = 0;
        callback = std::exchange(sink_, {});
    }
    ludork::engine::text_input::Event event;
    event.kind = ludork::engine::text_input::Event::Kind::Complete;
    event.accepted = accepted;
    event.state.text = text;
    callback(id, std::move(event));
}

std::shared_ptr<ludork::engine::text_input::TextInputHost>
createAndroidTextInputHost() {
    const std::shared_ptr<TextInputHostAndroidImpl> host =
        std::make_shared<TextInputHostAndroidImpl>(sf::getNativeActivity());
    const std::lock_guard<std::mutex> lock(hostMutex);
    currentHost = host;
    return host;
}

}  // namespace ludork::application

extern "C" JNIEXPORT void JNICALL
Java_com_ludork_android_LudorkActivity_completeTextInput(JNIEnv* environment,
                                                         jclass, jlong id,
                                                         jboolean accepted,
                                                         jstring text) {
    std::shared_ptr<ludork::application::TextInputHostAndroidImpl> host;
    {
        const std::lock_guard<std::mutex> lock(hostMutex);
        host = currentHost.lock();
    }
    if (host == nullptr) {
        return;
    }
    std::string value;
    if (accepted && text != nullptr) {
        const jchar* characters = environment->GetStringChars(text, nullptr);
        if (characters == nullptr) {
            return;
        }
        const jsize length = environment->GetStringLength(text);
        const sf::U8String bytes =
            sf::String::fromUtf16(characters, characters + length).toUtf8();
        value.assign(bytes.begin(), bytes.end());
        environment->ReleaseStringChars(text, characters);
    }
    host->complete(static_cast<ludork::engine::text_input::SessionId>(id),
                   accepted == JNI_TRUE, value);
}
