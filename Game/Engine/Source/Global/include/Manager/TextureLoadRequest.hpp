#pragma once

#include <CoreMinimal.hpp>
#include <GlobalRuntimeApi.hpp>

namespace ludork::global::texture_manager_impl {
class TextureManagerImpl;
struct RequestState;
}  // namespace ludork::global::texture_manager_impl

BIND_CLASS(metadata = false)
class LUDORK_GLOBAL_API TextureLoadRequest {
public:
    ~TextureLoadRequest();
    TextureLoadRequest(const TextureLoadRequest&) = delete;
    TextureLoadRequest& operator=(const TextureLoadRequest&) = delete;
    TextureLoadRequest(TextureLoadRequest&&) = delete;
    TextureLoadRequest& operator=(TextureLoadRequest&&) = delete;

    BIND_CLASS_PROPERTY(readonly = true, metadata = false, type = "'pending'")
    static const std::string PENDING;

    BIND_CLASS_PROPERTY(readonly = true, metadata = false, type = "'ready'")
    static const std::string READY;

    BIND_CLASS_PROPERTY(readonly = true, metadata = false, type = "'failed'")
    static const std::string FAILED;

    BIND_CLASS_PROPERTY(readonly = true, metadata = false, type = "'cancelled'")
    static const std::string CANCELLED;

    BIND_METHOD(metadata = false)
    std::string getStatus() const;

    /// Return the ready texture without waiting. Pending and cancelled requests
    /// raise an error; failed requests rethrow the loading error.
    BIND_METHOD(metadata = false, nonnull_return = true)
    std::shared_ptr<sf::Texture> getTexture() const;

    /// Change the priority of this pending subscription. Larger values run
    /// first.
    BIND_METHOD(metadata = false)
    void setPriority(int priority);

    /// Release this subscription without cancelling other users of the texture.
    BIND_METHOD(metadata = false)
    void cancel();

private:
    friend class ludork::global::texture_manager_impl::TextureManagerImpl;

    TextureLoadRequest(
        std::shared_ptr<
            ludork::global::texture_manager_impl::TextureManagerImpl>
            impl,
        std::shared_ptr<ludork::global::texture_manager_impl::RequestState>
            state);

    std::shared_ptr<ludork::global::texture_manager_impl::TextureManagerImpl>
        impl_;
    std::shared_ptr<ludork::global::texture_manager_impl::RequestState> state_;
};
