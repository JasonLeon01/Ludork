#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>

#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

class SoundFilter;
class MusicFilter;

BIND_FUNCTION_GROUP(name = "Manager")

BIND_FUNCTION()
std::shared_ptr<sf::SoundBuffer> getSoundBuffer(const std::string& filePath);

BIND_FUNCTION(defaults = {nil})
std::shared_ptr<sf::Sound> playSE(const std::string& filename,
                                  const SoundFilter* filter = nullptr);

BIND_FUNCTION(defaults = {nil, nil, 64.0})
std::shared_ptr<sf::Sound> playVoice(
    const std::string& filename, const SoundFilter* filter = nullptr,
    const std::shared_ptr<sf::Transformable>& refActor = nullptr,
    float minDistance = 64.0f);

BIND_FUNCTION(defaults = {nil})
std::shared_ptr<sf::Music> playMusic(const std::string& musicType,
                                     const std::string& filename,
                                     const MusicFilter* filter = nullptr);

BIND_FUNCTION()
void stopSound();

BIND_FUNCTION()
void stopVoice();

BIND_FUNCTION()
void stopMusic(const std::string& musicType);

BIND_FUNCTION()
std::shared_ptr<sf::Font> loadFont(const std::string& filename);

BIND_FUNCTION()
std::shared_ptr<sf::Font> getFont(const std::string& fontName);

BIND_FUNCTION()
std::string getFontFilename(const std::string& fontName);

BIND_FUNCTION()
std::vector<std::string> getFontList();

BIND_FUNCTION()
std::vector<std::string> getFontFilenameList();

BIND_FUNCTION()
bool hasFont(const std::string& fontName);

BIND_FUNCTION(defaults = {false, nil, false})
std::shared_ptr<sf::Texture> loadTexture(
    const std::string& subFolder, std::string filename, bool sRGB = false,
    std::optional<sf::IntRect> area = std::nullopt, bool smooth = false);

BIND_FUNCTION(defaults = {false, nil, false})
std::shared_ptr<sf::Texture> loadBlock(
    const std::string& filename, bool sRGB = false,
    std::optional<sf::IntRect> area = std::nullopt, bool smooth = false);

BIND_FUNCTION(defaults = {false, nil, false})
std::shared_ptr<sf::Texture> loadCharacter(
    const std::string& filename, bool sRGB = false,
    std::optional<sf::IntRect> area = std::nullopt, bool smooth = false);

BIND_FUNCTION(defaults = {false, nil, false})
std::shared_ptr<sf::Texture> loadSystem(
    const std::string& filename, bool sRGB = false,
    std::optional<sf::IntRect> area = std::nullopt, bool smooth = false);

BIND_FUNCTION(defaults = {false, nil, false})
std::shared_ptr<sf::Texture> loadTileset(
    const std::string& filename, bool sRGB = false,
    std::optional<sf::IntRect> area = std::nullopt, bool smooth = false);

BIND_FUNCTION(defaults = {false, nil, false})
std::shared_ptr<sf::Texture> loadAutotile(
    const std::string& filename, bool sRGB = false,
    std::optional<sf::IntRect> area = std::nullopt, bool smooth = false);

BIND_FUNCTION(defaults = {false, nil, false})
std::shared_ptr<sf::Texture> loadFog(
    const std::string& filename, bool sRGB = false,
    std::optional<sf::IntRect> area = std::nullopt, bool smooth = false);

BIND_FUNCTION(defaults = {false, nil, false})
std::shared_ptr<sf::Texture> loadTransition(
    const std::string& filename, bool sRGB = false,
    std::optional<sf::IntRect> area = std::nullopt, bool smooth = false);

BIND_FUNCTION(defaults = {nil})
std::shared_ptr<sf::Shader> loadShader(
    const std::string& shaderPath,
    std::optional<sf::Shader::Type> shaderType = std::nullopt);

BIND_FUNCTION()
std::shared_ptr<sf::Shader> loadFullShaderWithGeo(const std::string& vertPath,
                                                  const std::string& geoPath,
                                                  const std::string& fragPath);

BIND_FUNCTION()
std::shared_ptr<sf::Shader> loadGeoShader(const std::string& vertPath,
                                          const std::string& geoPath,
                                          const std::string& fragPath);
