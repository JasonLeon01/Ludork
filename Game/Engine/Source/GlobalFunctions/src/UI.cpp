#include <GlobalFunctions/UI.hpp>
#include <algorithm>
#include <cctype>
#include <stdexcept>

sf::Color hexColor(const std::string& value, int alpha) {
    std::string text = value;
    if (!text.empty() && (text.front() == '#' || text.front() == '$')) {
        text.erase(text.begin());
    } else if (text.size() >= 2 && text.substr(0, 2) == "0x") {
        text.erase(0, 2);
    }
    if (text.size() != 6 && text.size() != 8) {
        throw std::runtime_error("Invalid hex color string");
    }
    if (!std::all_of(text.begin(), text.end(), [](unsigned char character) {
            return std::isxdigit(character) != 0;
        })) {
        throw std::runtime_error("Invalid hex color string");
    }
    const auto red =
        static_cast<std::uint8_t>(std::stoul(text.substr(0, 2), nullptr, 16));
    const auto green =
        static_cast<std::uint8_t>(std::stoul(text.substr(2, 2), nullptr, 16));
    const auto blue =
        static_cast<std::uint8_t>(std::stoul(text.substr(4, 2), nullptr, 16));
    if (text.size() == 6) {
        return sf::Color(red, green, blue, static_cast<std::uint8_t>(alpha));
    }
    const auto embeddedAlpha =
        static_cast<std::uint8_t>(std::stoul(text.substr(6, 2), nullptr, 16));
    return sf::Color(red, green, blue, embeddedAlpha);
}

sf::Color hexColor(int value, int alpha) {
    int r = (value >> 16) & 0xFF;
    int g = (value >> 8) & 0xFF;
    int b = value & 0xFF;
    int a = value >> 24 ? (value >> 24) & 0xFF : alpha;
    return sf::Color(r, g, b, a);
}

sf::Color getRosyBrown() {
    return hexColor(0xA96362);
}

sf::Color getCopper() {
    return hexColor(0xA86538);
}

sf::Color getSage() {
    return hexColor(0xADB57D);
}

sf::Color getTeal() {
    return hexColor(0x4B8082);
}

sf::Color getMutedPurple() {
    return hexColor(0x6F6496);
}

sf::Color getTaupe() {
    return hexColor(0x72695C);
}

sf::Color getTerraCotta() {
    return hexColor(0x99574D);
}

sf::Color getOchre() {
    return hexColor(0x927140);
}

sf::Color getFernGreen() {
    return hexColor(0x4B7455);
}

sf::Color getSteelBlue() {
    return hexColor(0x566C8F);
}

sf::Color getDimGrey() {
    return hexColor(0x6A6A6A);
}

sf::Color getCharcoal() {
    return hexColor(0x1F1F1F);
}
