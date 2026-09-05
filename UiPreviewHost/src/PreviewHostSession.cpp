#include "PreviewHostSession.hpp"

#include "Protocol/PreviewProtocol.hpp"

#include <EngineState.hpp>
#include <Runtime/AssetStore.hpp>
#include <Runtime/RuntimeValueReader.hpp>
#include <UI/UiControlAdapterRegistry.hpp>
#include <UI/UiResources.hpp>
#include <UI/UiVector4CurveResource.hpp>
#include <Utf8Path.hpp>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>

namespace ludork::preview_host {

PreviewHostSession::PreviewHostSession(std::string_view adapterFingerprint)
    : adapterFingerprint_(adapterFingerprint) {}

PreviewHostSession::~PreviewHostSession() noexcept {
    uiSession_.reset();
    clearUiVector4CurveResourceCache();
    clearUiControlAdapterResourceCache();
    uiResources().reset();
}

RuntimeData PreviewHostSession::handle(const RuntimeData& requestValue) {
    const RuntimeData::Map& request = ludork::runtime::value_reader::requireMap(
        requestValue, "Preview request");
    const std::string& type = ludork::runtime::value_reader::requireString(
        ludork::runtime::value_reader::requireValue(request, "type",
                                                    "Preview request"),
        "Preview request.type");
    if (type == "handshake") {
        return handshake(request);
    }
    if (!accepted_) {
        throw std::runtime_error(
            "Preview protocol handshake has not completed");
    }
    if (type == "render") {
        return uiSession_.render(request, frameFiles_);
    }
    if (type == "hitTest") {
        return uiSession_.hitTest(request);
    }
    if (type == "renderActorBatch") {
        engineState().setScale(1.0f);
        return actorRenderer_.render(request, frameFiles_);
    }
    throw std::invalid_argument("Unknown preview request type: " + type);
}

RuntimeData PreviewHostSession::handshake(const RuntimeData::Map& request) {
    clearUiVector4CurveResourceCache();
    accepted_ = false;
    uiSession_.reset();
    const std::int64_t requestedProtocol =
        ludork::runtime::value_reader::requireInteger(
            ludork::runtime::value_reader::requireValue(
                request, "protocolVersion", "Handshake"),
            "Handshake.protocolVersion");
    const std::string& requestedFingerprint =
        ludork::runtime::value_reader::requireString(
            ludork::runtime::value_reader::requireValue(
                request, "adapterFingerprint", "Handshake"),
            "Handshake.adapterFingerprint");
    const bool accepted = requestedProtocol == protocolVersion &&
                          requestedFingerprint == adapterFingerprint_;
    std::string message;
    if (!accepted) {
        message =
            "UiPreviewHost protocol or adapter fingerprint is incompatible.";
    } else {
        const std::string& projectPath =
            ludork::runtime::value_reader::requireString(
                ludork::runtime::value_reader::requireValue(
                    request, "projectPath", "Handshake"),
                "Handshake.projectPath");
        const std::filesystem::path project = std::filesystem::weakly_canonical(
            ludork::standard::pathFromUtf8(projectPath));
        if (!std::filesystem::is_directory(project)) {
            throw std::invalid_argument(
                "Handshake projectPath is not a directory");
        }
        std::filesystem::current_path(project);
        ludork::runtime::assetStore().configure(project);
        engineState().setScale(1.0f);
        actorRenderer_.reset(project);
        accepted_ = true;
    }
    RuntimeData::Array capabilities;
    capabilities.emplace_back(RuntimeData("ui"));
    capabilities.emplace_back(RuntimeData("actor"));
    return RuntimeData(object({
        {"type", RuntimeData("handshake")},
        {"accepted", RuntimeData(accepted)},
        {"protocolVersion", RuntimeData(protocolVersion)},
        {"adapterFingerprint", RuntimeData(adapterFingerprint_)},
        {"capabilities", RuntimeData(std::move(capabilities))},
        {"message", RuntimeData(std::move(message))},
    }));
}

}  // namespace ludork::preview_host
