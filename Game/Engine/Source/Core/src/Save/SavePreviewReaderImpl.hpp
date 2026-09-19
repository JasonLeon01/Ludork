#pragma once

#include <Save/SavePreviewReader.hpp>
#include "SavePreviewServiceImpl.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>

struct SavePreviewReader::Impl {
    struct Scan {
        int latestSlot = 0;
        std::string error;
    };

    std::atomic<std::uint64_t> scanSequence{0};
    std::atomic<std::uint64_t> previewSequence{0};
    std::mutex mutex;
    std::optional<Scan> pendingScan;
    std::optional<ludork::engine::SavePreviewServiceImpl::Preview>
        pendingPreview;
    Scan scan;
    ludork::engine::SavePreviewServiceImpl::Preview preview;
};
