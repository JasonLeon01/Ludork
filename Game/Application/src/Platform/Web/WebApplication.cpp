#include "WebApplication.hpp"
#include "WebConfiguration.hpp"

#include <Application.hpp>
#include <PersistentStorage.hpp>
#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundBuffer.hpp>
#include <SFML/Audio/SoundChannel.hpp>
#include <emscripten.h>

#include <filesystem>
#include <array>
#include <cstdint>
#include <exception>

// clang-format off
EM_JS(void, ludorkWebStarted, (), { Module['ludorkStarted'](); });
EM_JS(void, ludorkWebError, (const char* error), { Module['ludorkError'](UTF8ToString(error)); });
EM_JS(void, ludorkWebTitle, (const char* title), { document.title = UTF8ToString(title); });
EM_JS(void, ludorkWebLocale, (char* buffer, int capacity), {
    stringToUTF8(navigator.language || 'en-GB', buffer, capacity);
});
// clang-format on

namespace ludork::application::web {

int run(int argc, char** argv) {
    try {
        ludorkWebTitle(Title);
        std::array<char, 128> locale{};
        ludorkWebLocale(locale.data(), static_cast<int>(locale.size()));
        configureSystemLocale(locale.data());
        configureRuntimePaths("/Game", "/UserData");
        ludork::standard::restoreStorage(std::filesystem::path("/UserData") /
                                         AppName);
        const std::array<std::int16_t, 1> silence{};
        const sf::SoundBuffer silentBuffer(silence.data(), silence.size(), 1,
                                           44100, {sf::SoundChannel::Mono});
        const sf::Sound audioLifetime(silentBuffer);
        ludorkWebStarted();
        const int result = ludork::application::run(argc, argv);
        ludork::standard::persistStorage(std::filesystem::path("/UserData") /
                                         AppName);
        return result;
    } catch (const std::exception& error) {
        ludorkWebError(error.what());
        return 1;
    }
}

}  // namespace ludork::application::web
