#include "PreviewHostSession.hpp"
#include "Protocol/PreviewProtocol.hpp"

#include <Runtime/RuntimeValue.hpp>
#include <UI/UiControlAdapterRegistry.hpp>
#include <Utils/File.hpp>

#include <iostream>
#include <optional>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2 || std::string(argv[1]) != "--stdio") {
        std::cerr << "Usage: UiPreviewHost --stdio\n";
        return 2;
    }
    try {
        ludork::preview_host::configureProtocolStreams();
        ludork::preview_host::PreviewHostSession host(
            uiControlAdapterFingerprint());
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
