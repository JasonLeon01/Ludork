target_sources(UiPreviewHostExecutable PRIVATE
    Actor/ActorBatchRenderer.cpp
    Particle/ParticlePreviewSession.cpp
    PreviewBuildInfo.cpp
    PreviewHostSession.cpp
    Protocol/FrameFiles.cpp
    Protocol/PreviewProtocol.cpp
    Rendering/PixelConversion.cpp
    UI/UiPreviewDrawing.cpp
    UI/UiPreviewInstantiation.cpp
    UI/UiPreviewReparent.cpp
    UI/UiPreviewSession.cpp
    UiPreviewHost.cpp)

target_include_directories(UiPreviewHostExecutable PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")
