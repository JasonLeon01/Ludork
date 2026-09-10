#include "PreviewBuildInfo.hpp"
#include "PreviewHostSession.hpp"
#include "Protocol/PreviewProtocol.hpp"

#include <Runtime/RuntimeData.hpp>
#include <UI/UiControlAdapterRegistry.hpp>
#include <Runtime/Json.hpp>

#include <iostream>
#include <optional>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2 || (std::string(argv[1]) != "--stdio" &&
                      std::string(argv[1]) != "--describe" &&
                      std::string(argv[1]) != "--build-info")) {
        std::cerr << "Usage: UiPreviewHost --stdio|--describe|--build-info\n";
        return 2;
    }
    try {
        ludork::preview_host::configureProtocolStreams();
        if (std::string(argv[1]) != "--stdio") {
            const std::string_view description =
                std::string(argv[1]) == "--describe"
                    ? uiControlRegistryDescription()
                    : ludork::preview_host::previewBuildInfo();
            std::cout.write(description.data(),
                            static_cast<std::streamsize>(description.size()));
            std::cout.flush();
            return std::cout ? 0 : 1;
        }
        ludork::preview_host::PreviewHostSession host(
            uiControlAdapterFingerprint(), uiControlRegistryHash());
        while (true) {
            const std::optional<std::string> message =
                ludork::preview_host::readMessage();
            if (!message.has_value()) {
                return 0;
            }
            try {
                ludork::preview_host::writeMessage(
                    host.handle(parseJSONText(*message)));
            } catch (const std::exception& exception) {
                ludork::preview_host::writeMessage(
                    ludork::preview_host::errorResponse(exception.what()));
            }
        }
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
