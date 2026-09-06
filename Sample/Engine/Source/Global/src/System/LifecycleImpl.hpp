#pragma once

#include <ConfigParser.hpp>

#include <functional>
#include <memory>
#include <string>

namespace ludork::global::system_lifecycle_impl {

void init(const std::shared_ptr<ludork::standard::ConfigParser>& data,
          const std::string& dataFilePath);
bool isDebugMode();
void setDebugMode(bool debugMode);
bool isActive();
bool shouldLoop();
void run();
void setStandardUpdate(std::function<void()> update);
void updateRuntime();
void initializeRuntimeSession() noexcept;
void shutdownRuntime() noexcept;
void onConfigChanged(const std::string& key);

}  // namespace ludork::global::system_lifecycle_impl
