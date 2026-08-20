target_sources(UiPreviewHost PRIVATE
    Actor/ActorBatchRenderer.cpp
    PreviewHostSession.cpp
    Protocol/FrameFiles.cpp
    Protocol/PreviewProtocol.cpp
    Rendering/PixelConversion.cpp
    UI/UiPreviewDrawing.cpp
    UI/UiPreviewInstantiation.cpp
    UI/UiPreviewSession.cpp
    UiPreviewCurveResolver.cpp
    UiPreviewHost.cpp)

target_include_directories(UiPreviewHost PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")
