#include <Manager/TextureLoadRequest.hpp>

#include "TextureManager/TextureManagerImpl.hpp"

#include <utility>

const std::string TextureLoadRequest::PENDING = "pending";
const std::string TextureLoadRequest::READY = "ready";
const std::string TextureLoadRequest::FAILED = "failed";
const std::string TextureLoadRequest::CANCELLED = "cancelled";

TextureLoadRequest::TextureLoadRequest(
    std::shared_ptr<ludork::global::texture_manager_impl::TextureManagerImpl>
        impl,
    std::shared_ptr<ludork::global::texture_manager_impl::RequestState> state)
    : impl_(std::move(impl)), state_(std::move(state)) {}

TextureLoadRequest::~TextureLoadRequest() {
    impl_->cancel(*state_);
}

std::string TextureLoadRequest::getStatus() const {
    return impl_->status(*state_);
}

std::shared_ptr<sf::Texture> TextureLoadRequest::getTexture() const {
    return impl_->texture(*state_);
}

void TextureLoadRequest::setPriority(int priority) {
    impl_->setPriority(*state_, priority);
}

void TextureLoadRequest::cancel() {
    impl_->cancel(*state_);
}
