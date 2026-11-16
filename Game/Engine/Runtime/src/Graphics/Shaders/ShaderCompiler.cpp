#include <Runtime/Graphics/ShaderCompiler.hpp>

#include <memory>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3dcompiler.h>
#endif

namespace ludork::runtime::graphics {
namespace {

#ifdef _WIN32
void releaseBlob(ID3DBlob* blob) {
    if (blob) {
        blob->Release();
    }
}

std::string compilerVersion() {
    HMODULE module = GetModuleHandleW(D3DCOMPILER_DLL_W);
    if (!module) {
        throw std::runtime_error("The HLSL compiler module is not loaded");
    }
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(module, path.data(),
                                            static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) {
        throw std::runtime_error("Cannot locate the HLSL compiler module");
    }
    path.resize(length);
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &ignored);
    std::vector<std::uint8_t> bytes(size);
    if (!size || !GetFileVersionInfoW(path.c_str(), 0, size, bytes.data())) {
        throw std::runtime_error("Cannot read the HLSL compiler version");
    }
    VS_FIXEDFILEINFO* version = nullptr;
    UINT versionSize = 0;
    if (!VerQueryValueW(bytes.data(), L"\\", reinterpret_cast<void**>(&version),
                        &versionSize) ||
        versionSize < sizeof(*version)) {
        throw std::runtime_error("The HLSL compiler version is invalid");
    }
    return std::to_string(version->dwFileVersionMS) + "." +
           std::to_string(version->dwFileVersionLS);
}
#endif

void validateProfile(std::string_view profile) {
    if (profile != "4_0" && profile != "4_1" && profile != "5_0") {
        throw std::invalid_argument("Unsupported DirectX shader profile: " +
                                    std::string(profile));
    }
}

}  // namespace

std::vector<std::uint8_t> ShaderCompiler::compileDxbc(
    std::string_view source, ShaderTranslator::Stage stage,
    std::string_view entryPoint, std::string_view name,
    std::string_view profile) {
    validateProfile(profile);
#ifdef _WIN32
    const std::string target =
        std::string(stage == ShaderTranslator::Stage::Vertex     ? "vs_"
                    : stage == ShaderTranslator::Stage::Geometry ? "gs_"
                                                                 : "ps_") +
        std::string(profile);
    const std::string entry(entryPoint);
    const std::string sourceName(name);
    ID3DBlob* rawCode = nullptr;
    ID3DBlob* rawErrors = nullptr;
    const HRESULT result = D3DCompile(
        source.data(), source.size(), sourceName.c_str(), nullptr, nullptr,
        entry.c_str(), target.c_str(),
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
        &rawCode, &rawErrors);
    const std::unique_ptr<ID3DBlob, decltype(&releaseBlob)> code(rawCode,
                                                                 releaseBlob);
    const std::unique_ptr<ID3DBlob, decltype(&releaseBlob)> errors(rawErrors,
                                                                   releaseBlob);
    if (FAILED(result) || !code) {
        throw std::runtime_error(
            "HLSL compilation failed: " + sourceName + "\n" +
            (errors ? std::string(
                          static_cast<const char*>(errors->GetBufferPointer()),
                          errors->GetBufferSize())
                    : std::to_string(static_cast<unsigned long>(result))));
    }
    const auto* begin =
        static_cast<const std::uint8_t*>(code->GetBufferPointer());
    return {begin, begin + code->GetBufferSize()};
#else
    static_cast<void>(source);
    static_cast<void>(stage);
    static_cast<void>(entryPoint);
    static_cast<void>(name);
    throw std::runtime_error("DXBC compilation requires Windows");
#endif
}

std::string ShaderCompiler::fingerprint(std::string_view profile) {
    validateProfile(profile);
#ifdef _WIN32
    return std::string("D3DCompile/") + LUDORK_SHADER_COMPILER_FINGERPRINT +
           "/" + compilerVersion() + "/" + std::string(profile) + "/strict/O3";
#else
    return std::string("D3DCompile/unavailable/") +
           LUDORK_SHADER_COMPILER_FINGERPRINT + "/" + std::string(profile);
#endif
}

}  // namespace ludork::runtime::graphics
