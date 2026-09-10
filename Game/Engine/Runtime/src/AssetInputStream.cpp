#include <Runtime/AssetStore.hpp>
#include <Runtime/AssetInputStream.hpp>

#include "AssetInputStreamImpl.hpp"
#include <Utf8Path.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace ludork::runtime {
namespace {

bool addOverflows(std::uint64_t left, std::uint64_t right) {
    return right > std::numeric_limits<std::uint64_t>::max() - left;
}

}  // namespace

AssetInputStream::AssetInputStream(const std::filesystem::path& source,
                                   std::uint64_t offset, std::uint64_t size)
    : impl_(std::make_unique<Impl>()) {
    if (offset > static_cast<std::uint64_t>(
                     std::numeric_limits<std::streamoff>::max()) ||
        size > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("Asset stream is too large for this platform");
    }
    impl_->stream.open(source, std::ios::binary);
    if (!impl_->stream) {
        throw std::runtime_error("Failed to open asset source: " +
                                 ludork::standard::pathToUtf8(source));
    }
    impl_->stream.seekg(static_cast<std::streamoff>(offset));
    if (!impl_->stream) {
        throw std::runtime_error("Failed to seek asset source");
    }
    impl_->offset = offset;
    impl_->size = size;
}

AssetInputStream::~AssetInputStream() = default;
AssetInputStream::AssetInputStream(AssetInputStream&&) noexcept = default;
AssetInputStream& AssetInputStream::operator=(AssetInputStream&&) noexcept =
    default;

std::optional<std::size_t> AssetInputStream::read(void* data,
                                                  std::size_t size) {
    if (impl_ == nullptr || (data == nullptr && size != 0)) {
        return std::nullopt;
    }
    const std::uint64_t remaining = impl_->size - impl_->position;
    const std::size_t requested =
        static_cast<std::size_t>(std::min<std::uint64_t>(remaining, size));
    if (requested == 0) {
        return 0;
    }
    if (requested >
        static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
        return std::nullopt;
    }
    impl_->stream.read(static_cast<char*>(data),
                       static_cast<std::streamsize>(requested));
    const std::streamsize count = impl_->stream.gcount();
    if (count != static_cast<std::streamsize>(requested)) {
        return std::nullopt;
    }
    impl_->position += static_cast<std::uint64_t>(count);
    return static_cast<std::size_t>(count);
}

std::optional<std::size_t> AssetInputStream::seek(std::size_t position) {
    if (impl_ == nullptr || position > impl_->size ||
        addOverflows(impl_->offset, position) ||
        impl_->offset + position >
            static_cast<std::uint64_t>(
                std::numeric_limits<std::streamoff>::max())) {
        return std::nullopt;
    }
    impl_->stream.clear();
    impl_->stream.seekg(static_cast<std::streamoff>(impl_->offset + position));
    if (!impl_->stream) {
        return std::nullopt;
    }
    impl_->position = position;
    return position;
}

std::optional<std::size_t> AssetInputStream::tell() {
    return impl_ == nullptr ? std::nullopt
                            : std::optional<std::size_t>(
                                  static_cast<std::size_t>(impl_->position));
}

std::optional<std::size_t> AssetInputStream::getSize() {
    return impl_ == nullptr ? std::nullopt
                            : std::optional<std::size_t>(
                                  static_cast<std::size_t>(impl_->size));
}

}  // namespace ludork::runtime
