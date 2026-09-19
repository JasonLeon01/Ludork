#include <Save/SavePreviewReader.hpp>

#include "SavePreviewReaderImpl.hpp"
#include <Utf8Path.hpp>

#include <filesystem>
#include <stdexcept>

SavePreviewReader::SavePreviewReader() : impl_(std::make_shared<Impl>()) {}

SavePreviewReader::~SavePreviewReader() {
    cancel();
}

void SavePreviewReader::requestScan(const std::vector<std::string>& paths) {
    if (paths.size() > 10000) {
        throw std::invalid_argument("Too many save slots");
    }
    const auto impl = impl_;
    const auto sequence = ++impl->scanSequence;
    {
        std::lock_guard lock(impl->mutex);
        impl->pendingScan.reset();
    }
    ludork::engine::savePreviewService().enqueue([impl, sequence, paths] {
        Impl::Scan result;
        std::optional<std::filesystem::file_time_type> latest;
        try {
            for (std::size_t index = 0; index < paths.size(); ++index) {
                if (impl->scanSequence != sequence) {
                    return;
                }
                const auto path = ludork::standard::pathFromUtf8(paths[index]);
                if (!std::filesystem::exists(path)) {
                    continue;
                }
                if (!std::filesystem::is_regular_file(path)) {
                    throw std::runtime_error("Save path is not a file: " +
                                             paths[index]);
                }
                const auto modified = std::filesystem::last_write_time(path);
                if (!latest || modified > *latest) {
                    latest = modified;
                    result.latestSlot = static_cast<int>(index + 1);
                }
            }
        } catch (const std::exception& error) {
            result.error = error.what();
        }
        std::lock_guard lock(impl->mutex);
        if (impl->scanSequence == sequence) {
            impl->pendingScan = std::move(result);
        }
    });
}

bool SavePreviewReader::pollScan() {
    std::lock_guard lock(impl_->mutex);
    if (!impl_->pendingScan) {
        return false;
    }
    impl_->scan = std::move(*impl_->pendingScan);
    impl_->pendingScan.reset();
    return true;
}

int SavePreviewReader::getLatestSlot() const {
    return impl_->scan.latestSlot;
}

std::string SavePreviewReader::getScanError() const {
    return impl_->scan.error;
}

void SavePreviewReader::requestPreview(const std::string& path,
                                       unsigned int width,
                                       unsigned int height) {
    if (width == 0 || height == 0 || width > 1024 || height > 1024) {
        throw std::invalid_argument(
            "Save preview dimensions must be between 1 and 1024");
    }
    const auto impl = impl_;
    const auto sequence = ++impl->previewSequence;
    {
        std::lock_guard lock(impl->mutex);
        impl->pendingPreview.reset();
    }
    impl->preview.state = "loading";
    auto* service = &ludork::engine::savePreviewService();
    service->enqueue([impl, sequence, path, width, height, service] {
        if (impl->previewSequence != sequence) {
            return;
        }
        auto result = service->read(path, width, height);
        std::lock_guard lock(impl->mutex);
        if (impl->previewSequence == sequence) {
            impl->pendingPreview = std::move(result);
        }
    });
}

bool SavePreviewReader::pollPreview() {
    std::lock_guard lock(impl_->mutex);
    if (!impl_->pendingPreview) {
        return false;
    }
    impl_->preview = std::move(*impl_->pendingPreview);
    impl_->pendingPreview.reset();
    return true;
}

std::string SavePreviewReader::getState() const {
    return impl_->preview.state;
}

double SavePreviewReader::getModificationTime() const {
    return impl_->preview.modificationTime;
}

std::shared_ptr<sf::Image> SavePreviewReader::getImage() const {
    return impl_->preview.image;
}

std::string SavePreviewReader::getError() const {
    return impl_->preview.error;
}

void SavePreviewReader::cancelPreview() {
    ++impl_->previewSequence;
    std::lock_guard lock(impl_->mutex);
    impl_->pendingPreview.reset();
    impl_->preview = {};
}

void SavePreviewReader::cancel() {
    cancelPreview();
    ++impl_->scanSequence;
    std::lock_guard lock(impl_->mutex);
    impl_->pendingScan.reset();
}

void SavePreviewReader::invalidate(const std::string& path) {
    ludork::engine::savePreviewService().invalidate(path);
}
