#pragma once

#include <filesystem>
#include <string>

namespace ludork::application {

void configureRuntimePaths(const std::filesystem::path& runtimeRoot,
                           const std::filesystem::path& userDataRoot);
void configureSystemLocale(const std::string& systemLocale);

int run(int argc, char** argv);

}
