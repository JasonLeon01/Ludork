#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace ludork::standard {

enum class FileBatchState {
    Scanning,
    Running,
    Completed,
    Cancelling,
    Cancelled,
    Failed,
};

struct FileBatchSpec {
    std::string category;
    std::filesystem::path root;
    std::string suffix;
    std::string excludeSuffix;
    bool recursive = false;
    bool required = true;
};

struct FileBatchError {
    std::string operation;
    std::string category;
    std::string path;
    int code = 0;
    std::string message;
};

struct FileBatchItem {
    std::size_t index = 0;
    std::string category;
    std::string relativePath;
    std::string content;
    bool encryptedData = false;
};

struct FileBatchSnapshot {
    FileBatchState state = FileBatchState::Failed;
    std::size_t total = 0;
    std::size_t completed = 0;
    std::size_t delivered = 0;
    bool drained = false;
    std::vector<FileBatchItem> items;
    std::optional<FileBatchError> error;
};

class FileBatchJob;

class FileBatchRuntime {
public:
    FileBatchRuntime();
    ~FileBatchRuntime();

    FileBatchRuntime(const FileBatchRuntime&) = delete;
    FileBatchRuntime& operator=(const FileBatchRuntime&) = delete;

    std::shared_ptr<FileBatchJob> start(std::vector<FileBatchSpec> specs);
    FileBatchSnapshot poll(const std::shared_ptr<FileBatchJob>& job,
                           std::size_t maximum);
    bool cancel(const std::shared_ptr<FileBatchJob>& job);
    void release(const std::shared_ptr<FileBatchJob>& job) noexcept;
    void shutdown() noexcept;

private:
    class Implementation;
    std::unique_ptr<Implementation> implementation_;
};

const char* fileBatchStateName(FileBatchState state);

}  // namespace ludork::standard
