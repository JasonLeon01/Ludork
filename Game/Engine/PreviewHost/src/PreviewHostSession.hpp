#pragma once

#include "Actor/ActorBatchRenderer.hpp"
#include "Map/MapPreviewSession.hpp"
#include "Material/MaterialPreviewSession.hpp"
#include "Particle/ParticlePreviewSession.hpp"
#include "Protocol/FrameFiles.hpp"
#include "UI/UiPreviewSession.hpp"
#include "Subtitle/SubtitlePreviewSession.hpp"

#include <Runtime/RuntimeData.hpp>

#include <string>
#include <string_view>

namespace ludork::preview_host {

class PreviewHostSession {
public:
    PreviewHostSession(std::string_view adapterFingerprint,
                       std::string_view registryHash);
    ~PreviewHostSession() noexcept;

    PreviewHostSession(const PreviewHostSession&) = delete;
    PreviewHostSession& operator=(const PreviewHostSession&) = delete;
    PreviewHostSession(PreviewHostSession&&) = delete;
    PreviewHostSession& operator=(PreviewHostSession&&) = delete;

    RuntimeData handle(const RuntimeData& requestValue);

private:
    static constexpr char MapClosedResponse[] = "mapClosed";
    static constexpr char HandshakeResponse[] = "handshake";
    static constexpr char HandshakeRequest[] = "handshake";
    static constexpr char CloseMapRequest[] = "closeMap";
    static constexpr char CompileMaterialRequest[] = "compileMaterial";
    static constexpr char RenderMapRequest[] = "renderMap";
    static constexpr char DescribeMaterialsRequest[] = "describeMaterials";
    static constexpr char RenderMaterialRequest[] = "renderMaterial";
    static constexpr char RenderUiRequest[] = "render";
    static constexpr char HitTestRequest[] = "hitTest";
    static constexpr char ResolveReparentRequest[] = "resolveReparent";
    static constexpr char RenderActorBatchRequest[] = "renderActorBatch";
    static constexpr char RenderParticleRequest[] = "renderParticle";
    static constexpr char RenderSubtitleRequest[] = "renderSubtitle";

    RuntimeData handshake(const RuntimeData::Map& request);

    std::string adapterFingerprint_;
    std::string registryHash_;
    FrameFiles frameFiles_;
    ActorBatchRenderer actorRenderer_;
    MapPreviewSession mapSession_;
    MaterialPreviewSession materialSession_;
    UiPreviewSession uiSession_;
    ParticlePreviewSession particleSession_;
    SubtitlePreviewSession subtitleSession_;
    bool accepted_ = false;
};

}  // namespace ludork::preview_host
