target_sources(PreviewHostExecutable PRIVATE
    Actor/ActorBatchRenderer.cpp
    Map/MapPreviewData.cpp
    Map/MapPreviewSession.cpp
    Particle/ParticlePreviewSession.cpp
    PreviewBuildInfo.cpp
    PreviewHostSession.cpp
    Protocol/FrameFiles.cpp
    Protocol/PreviewProtocol.cpp
    Rendering/PixelConversion.cpp
    Rendering/PreviewResources.cpp
    Subtitle/SubtitlePreviewSession.cpp
    UI/UiPreviewDrawing.cpp
    UI/UiPreviewInstantiation.cpp
    UI/UiPreviewReparent.cpp
    UI/UiPreviewSession.cpp
    PreviewHost.cpp)

target_include_directories(PreviewHostExecutable PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")
