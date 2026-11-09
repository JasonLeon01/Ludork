#include "MaterialCacheRequestImpl.hpp"

#include <Graphics/Materials/MaterialGenerated.hpp>
#include <Graphics/Materials/MaterialResource.hpp>
#include <DataFile.hpp>
#include <EncryptedPayload.hpp>
#include <LudorkGenerated/EncryptedPayloadConstants.hpp>
#include <Runtime/Data.hpp>
#include <Runtime/Json.hpp>
#include <Runtime/RuntimeDataReader.hpp>
#include <Sha256.hpp>
#include <StringUtils.hpp>
#include <Utf8Path.hpp>

#include <chrono>
#include <fstream>
#include <stdexcept>
#include <cerrno>
#include <iostream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace ludork::engine::materials {
namespace {

namespace reader = ludork::runtime::value_reader;

std::filesystem::path storedPath(const std::string& path, bool encryptShaders,
                                 bool encryptData) {
    auto result = ludork::standard::pathFromUtf8(path);
    if (encryptData && result.extension() == ".json") {
        result.replace_extension(".ldc");
    } else if (encryptShaders && (result.extension() == ".vert" ||
                                  result.extension() == ".frag")) {
        result += "c";
    }
    return result;
}

std::string physicalBytes(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Cannot read material cache: " +
                                 ludork::standard::pathToUtf8(path));
    }
    std::string bytes{std::istreambuf_iterator<char>(stream),
                      std::istreambuf_iterator<char>()};
    if (stream.bad()) {
        throw std::runtime_error("Cannot read complete material cache: " +
                                 ludork::standard::pathToUtf8(path));
    }
    return bytes;
}

bool requireOrdinaryEntry(const std::filesystem::path& path) {
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error == std::errc::no_such_file_or_directory ||
        status.type() == std::filesystem::file_type::not_found) {
        return false;
    }
    if (error) {
        throw std::filesystem::filesystem_error("Cannot inspect material cache",
                                                path, error);
    }
#if defined(_WIN32)
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        throw std::runtime_error("Material cache links are not supported: " +
                                 ludork::standard::pathToUtf8(path));
    }
#endif
    if (std::filesystem::is_symlink(status) ||
        (!std::filesystem::is_directory(status) &&
         !std::filesystem::is_regular_file(status))) {
        throw std::runtime_error("Unsupported material cache entry: " +
                                 ludork::standard::pathToUtf8(path));
    }
    return std::filesystem::is_directory(status);
}

void requireOrdinaryTree(const std::filesystem::path& path) {
    if (requireOrdinaryEntry(path)) {
        for (const auto& child : std::filesystem::directory_iterator(path)) {
            requireOrdinaryTree(child.path());
        }
    }
}

std::set<std::string> expectedFiles(
    const std::set<std::pair<std::string, std::string>>& programs,
    bool encryptShaders, bool encryptData) {
    std::set<std::string> files;
    std::map<std::string, std::pair<std::string, std::string>> claimed;
    const auto claim = [&](const std::string& path, const std::string& owner) {
        const std::string folded = ludork::standard::lowercase(path);
        const auto [position, inserted] =
            claimed.emplace(folded, std::pair(path, owner));
        if (!inserted && position->second != std::pair(path, owner)) {
            throw std::invalid_argument(
                "Material cache paths collide: " + path + " and " +
                position->second.first);
        }
    };
    const auto add = [&](const std::string& path) {
        const std::string stored = ludork::standard::pathToGenericUtf8(
            storedPath(path, encryptShaders, encryptData));
        claim(stored, "file");
        files.insert(stored);
    };
    for (const auto& [material, layer] : programs) {
        const std::string base = MaterialGenerated::resourceDirectory(material);
        const std::string program =
            base +
            (layer.empty()
                 ? "/single"
                 : "/layers/" +
                       MaterialGenerated::resourceDirectory(layer).substr(10));
        claim(base, "material");
        claim(program, "program");
        add(base + "/resource.json");
        for (const char* name :
             {"vertex.vert", "color.frag", "surface.frag", "transmission.frag",
              "source.frag", "program.json"}) {
            add(program + "/" + name);
        }
    }
    for (const auto& path : files) {
        auto parent = ludork::standard::pathFromUtf8(path).parent_path();
        while (!parent.empty()) {
            const auto found = claimed.find(ludork::standard::lowercase(
                ludork::standard::pathToGenericUtf8(parent)));
            if (found != claimed.end() && found->second.second == "file") {
                throw std::invalid_argument(
                    "Material cache file occupies a directory: " + path);
            }
            parent = parent.parent_path();
        }
    }
    return files;
}

}  // namespace

MaterialCacheRequest::Impl::DirectoryLock::DirectoryLock(
    const std::filesystem::path& path, const std::atomic<bool>& stop) {
    requireOrdinaryEntry(path);
    while (!stop.load()) {
#if defined(_WIN32)
        HANDLE file = CreateFileW(
            path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (file != INVALID_HANDLE_VALUE) {
            BY_HANDLE_FILE_INFORMATION info{};
            if (!GetFileInformationByHandle(file, &info) ||
                (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
                CloseHandle(file);
                throw std::runtime_error("Invalid material cache lock file");
            }
            handle = reinterpret_cast<std::intptr_t>(file);
            return;
        }
        const DWORD error = GetLastError();
        if (error != ERROR_SHARING_VIOLATION && error != ERROR_LOCK_VIOLATION) {
            throw std::system_error(static_cast<int>(error),
                                    std::system_category(),
                                    "Cannot lock material cache");
        }
#else
        const int file =
            ::open(path.c_str(), O_CREAT | O_RDWR | O_NOFOLLOW, 0600);
        if (file == -1) {
            throw std::system_error(errno, std::generic_category(),
                                    "Cannot open material cache lock");
        }
        if (::flock(file, LOCK_EX | LOCK_NB) == 0) {
            handle = file;
            return;
        }
        const int error = errno;
        ::close(file);
        if (error != EWOULDBLOCK && error != EAGAIN && error != EINTR) {
            throw std::system_error(error, std::generic_category(),
                                    "Cannot lock material cache");
        }
#endif
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    throw std::runtime_error("Material cache preparation was cancelled");
}

MaterialCacheRequest::Impl::DirectoryLock::~DirectoryLock() {
    if (handle != -1) {
#if defined(_WIN32)
        CloseHandle(reinterpret_cast<HANDLE>(handle));
#else
        ::flock(static_cast<int>(handle), LOCK_UN);
        ::close(static_cast<int>(handle));
#endif
    }
}

void MaterialCacheRequest::Impl::prepare(const std::atomic<bool>& stop) {
    requireOrdinaryEntry(userDataRoot);
    requireOrdinaryEntry(userDataRoot / "Assets");
    requireOrdinaryEntry(userDataRoot / "Assets" / "Shaders");
    std::filesystem::create_directories(cacheRoot.parent_path());
    DirectoryLock lock(cacheRoot.parent_path() / ".MaterialGenerated.lock",
                       stop);
    const Inputs inputs = captureInputs(stop);
    progress = 0.1;
    checkCancelled(stop);
    if (!reusable(inputs, stop)) {
        generate(inputs, stop);
    }
}

bool MaterialCacheRequest::Impl::reusable(const Inputs& inputs,
                                          const std::atomic<bool>& stop) const {
    requireOrdinaryEntry(userDataRoot);
    requireOrdinaryEntry(userDataRoot / "Assets");
    requireOrdinaryEntry(userDataRoot / "Assets" / "Shaders");
    requireOrdinaryTree(cacheRoot);
    const std::filesystem::path recordPath =
        cacheRoot / (inputs.encryptData ? "cache.ldc" : "cache.json");
    if (!std::filesystem::is_regular_file(recordPath)) {
        return false;
    }
    const auto expected = expectedFiles(inputs.programs, inputs.encryptShaders,
                                        inputs.encryptData);
    try {
        const RuntimeData record = decodeData(physicalBytes(recordPath));
        const auto& fields =
            reader::requireMap(record, "material cache record");
        if (stringifyJSON(
                reader::requireValue(fields, "identity", "material cache")) !=
            stringifyJSON(inputs.identity())) {
            return false;
        }
        const auto& outputs = reader::requireMap(
            reader::requireValue(fields, "outputs", "material cache"),
            "material cache outputs");
        if (outputs.size() != expected.size()) {
            return false;
        }
        for (const auto& path : expected) {
            checkCancelled(stop);
            const auto found = outputs.find(path);
            if (found == outputs.end() ||
                !std::filesystem::is_regular_file(
                    cacheRoot / ludork::standard::pathFromUtf8(path)) ||
                reader::requireString(found->second, path) !=
                    ludork::standard::sha256(physicalBytes(
                        cacheRoot / ludork::standard::pathFromUtf8(path)))) {
                return false;
            }
        }
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(cacheRoot)) {
            checkCancelled(stop);
            if (entry.is_regular_file() && entry.path() != recordPath &&
                !expected.contains(ludork::standard::pathToGenericUtf8(
                    entry.path().lexically_relative(cacheRoot)))) {
                return false;
            }
        }
        return true;
    } catch (const std::exception&) {
        checkCancelled(stop);
        return false;
    }
}

void MaterialCacheRequest::Impl::generate(const Inputs& inputs,
                                          const std::atomic<bool>& stop) {
    const auto expected = expectedFiles(inputs.programs, inputs.encryptShaders,
                                        inputs.encryptData);
    std::map<std::string, std::shared_ptr<const MaterialResource>> materials;
    materials.emplace(
        "", MaterialResource::fromData(MaterialResource::defaultDocument()));
    for (const auto& [path, document] : inputs.materials) {
        checkCancelled(stop);
        materials.emplace(path, MaterialResource::fromData(document, path));
    }

    std::filesystem::create_directories(cacheRoot.parent_path());
    static std::atomic<unsigned long long> nextId = 0;
    const std::string suffix =
        std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) +
        "-" + std::to_string(nextId.fetch_add(1));
    const auto staging =
        cacheRoot.parent_path() / (".MaterialGenerated-" + suffix);
    const auto backup =
        cacheRoot.parent_path() / (".MaterialGenerated-old-" + suffix);
    std::filesystem::create_directory(staging);
    bool backedUp = false;
    try {
        RuntimeData::Map outputs;
        std::size_t completed = 0;
        for (const auto& [material, layer] : inputs.programs) {
            checkCancelled(stop);
            const RuntimeData compiled = MaterialGenerated::compile(
                *materials.at(material),
                layer.empty() ? nullptr : materials.at(layer).get());
            const auto& files = reader::requireMap(
                reader::requireValue(
                    reader::requireMap(compiled, "material compiler"), "files",
                    "material compiler"),
                "material files");
            for (const auto& [path, value] : files) {
                checkCancelled(stop);
                const std::string& text = reader::requireString(value, path);
                const auto relative =
                    storedPath(path, inputs.encryptShaders, inputs.encryptData);
                const std::string key =
                    ludork::standard::pathToGenericUtf8(relative);
                if (!expected.contains(key)) {
                    throw std::runtime_error(
                        "Unexpected material compiler output: " + key);
                }
                const auto destination = staging / relative;
                if (outputs.contains(key)) {
                    continue;
                }
                std::filesystem::create_directories(destination.parent_path());
                if (relative.extension() == ".ldc") {
                    saveData(destination, parseJSONText(text));
                } else if (relative.extension() == ".vertc" ||
                           relative.extension() == ".fragc") {
                    const ludork::standard::EncryptedPayloadFormat format{
                        ludork::generated::encrypted::ShaderMagic,
                        ludork::generated::encrypted::MaximumShaderSize,
                        "shader"};
                    const std::uint64_t nonce =
                        std::stoull(ludork::standard::sha256(key + '\0' + text)
                                        .substr(0, 16),
                                    nullptr, 16);
                    const auto bytes = ludork::standard::encodeEncryptedPayload(
                        text, nonce, format, key, "material shader");
                    ludork::standard::writeFileBytes(
                        destination,
                        std::string_view(
                            reinterpret_cast<const char*>(bytes.data()),
                            bytes.size()));
                } else {
                    ludork::standard::writeFileBytes(destination, text);
                }
                outputs.emplace(key, RuntimeData(ludork::standard::sha256(
                                         physicalBytes(destination))));
            }
            progress = 0.1 + 0.8 * static_cast<double>(++completed) /
                                 static_cast<double>(inputs.programs.size());
        }
        if (outputs.size() != expected.size()) {
            throw std::runtime_error(
                "Material compiler outputs are incomplete");
        }
        if (stringifyJSON(captureInputs(stop).identity()) !=
            stringifyJSON(inputs.identity())) {
            throw std::runtime_error(
                "Material inputs changed during preparation; restart "
                "initialization");
        }
        saveData(staging / (inputs.encryptData ? "cache.ldc" : "cache.json"),
                 RuntimeData(RuntimeData::Map{
                     {"identity", inputs.identity()},
                     {"outputs", RuntimeData(std::move(outputs))}}));
        checkCancelled(stop);
        if (std::filesystem::exists(cacheRoot)) {
            std::filesystem::rename(cacheRoot, backup);
            backedUp = true;
        }
        std::filesystem::rename(staging, cacheRoot);
        if (backedUp) {
            backedUp = false;
            std::error_code cleanupError;
            std::filesystem::remove_all(backup, cleanupError);
            if (cleanupError) {
                std::cerr << "Material cache is ready, but its previous "
                             "generation could not be removed: "
                          << ludork::standard::pathToUtf8(backup) << " ("
                          << cleanupError.message() << ")\n";
            }
        }
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove_all(staging, ignored);
        if (backedUp && !std::filesystem::exists(cacheRoot)) {
            std::filesystem::rename(backup, cacheRoot, ignored);
        }
        throw;
    }
}

}  // namespace ludork::engine::materials
