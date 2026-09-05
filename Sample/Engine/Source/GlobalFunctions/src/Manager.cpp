#include <GlobalFunctions/Manager.hpp>

#include <Manager/AudioManager.hpp>
#include <Manager/AssetPath.hpp>
#include <Manager/FontManager.hpp>
#include <Manager/ShaderManager.hpp>
#include <Manager/TextureManager.hpp>

std::shared_ptr<sf::SoundBuffer> getSoundBuffer(const std::string& filePath) {
    return AudioManager::loadSound(filePath);
}

std::shared_ptr<sf::Sound> playSE(const std::string& filename,
                                  const SoundFilter* filter) {
    return AudioManager::playSound(
        ludork::global::manager::assetFile("Sounds", filename), filter);
}

std::shared_ptr<sf::Sound> playVoice(
    const std::string& filename, const SoundFilter* filter,
    const std::shared_ptr<sf::Transformable>& refActor, float minDistance) {
    return AudioManager::playVoice(
        ludork::global::manager::assetFile("Voices", filename), filter,
        refActor, minDistance);
}

std::shared_ptr<sf::Music> playMusic(const std::string& musicType,
                                     const std::string& filename,
                                     const MusicFilter* filter) {
    return AudioManager::playMusic(
        musicType, ludork::global::manager::assetFile("Musics", filename),
        filter);
}

void stopSound() {
    AudioManager::stopSound();
}
void stopVoice() {
    AudioManager::stopVoice();
}
void stopMusic(const std::string& musicType) {
    AudioManager::stopMusic(musicType);
}

std::shared_ptr<sf::Font> loadFont(const std::string& filename) {
    return FontManager::load(
        ludork::global::manager::assetFile("Fonts", filename));
}
std::shared_ptr<sf::Font> getFont(const std::string& fontName) {
    return FontManager::getFont(fontName);
}
std::string getFontFilename(const std::string& fontName) {
    return FontManager::getFontFilename(fontName);
}
std::vector<std::string> getFontList() {
    return FontManager::getFontList();
}
std::vector<std::string> getFontFilenameList() {
    return FontManager::getFontFilenameList();
}
bool hasFont(const std::string& fontName) {
    return FontManager::hasFont(fontName);
}

std::shared_ptr<sf::Texture> loadTexture(const std::string& subFolder,
                                         std::string filename, bool sRGB,
                                         std::optional<sf::IntRect> area,
                                         bool smooth) {
    return TextureManager::load(
        ludork::global::manager::textureAssetFile(subFolder, filename), sRGB,
        area, smooth);
}

std::shared_ptr<sf::Texture> loadBlock(const std::string& filename, bool sRGB,
                                       std::optional<sf::IntRect> area,
                                       bool smooth) {
    return loadTexture("Blocks", filename, sRGB, area, smooth);
}
std::shared_ptr<sf::Texture> loadCharacter(const std::string& filename,
                                           bool sRGB,
                                           std::optional<sf::IntRect> area,
                                           bool smooth) {
    return loadTexture("Characters", filename, sRGB, area, smooth);
}
std::shared_ptr<sf::Texture> loadSystem(const std::string& filename, bool sRGB,
                                        std::optional<sf::IntRect> area,
                                        bool smooth) {
    return loadTexture("System", filename, sRGB, area, smooth);
}
std::shared_ptr<sf::Texture> loadTileset(const std::string& filename, bool sRGB,
                                         std::optional<sf::IntRect> area,
                                         bool smooth) {
    return loadTexture("Tilesets", filename, sRGB, area, smooth);
}
std::shared_ptr<sf::Texture> loadAutotile(const std::string& filename,
                                          bool sRGB,
                                          std::optional<sf::IntRect> area,
                                          bool smooth) {
    return loadTexture("Autotiles", filename, sRGB, area, smooth);
}
std::shared_ptr<sf::Texture> loadFog(const std::string& filename, bool sRGB,
                                     std::optional<sf::IntRect> area,
                                     bool smooth) {
    return loadTexture("Fogs", filename, sRGB, area, smooth);
}
std::shared_ptr<sf::Texture> loadTransition(const std::string& filename,
                                            bool sRGB,
                                            std::optional<sf::IntRect> area,
                                            bool smooth) {
    return loadTexture("Transitions", filename, sRGB, area, smooth);
}

std::shared_ptr<sf::Shader> loadShader(
    const std::string& shaderPath, std::optional<sf::Shader::Type> shaderType) {
    return ShaderManager::load(
        ludork::global::manager::assetFile("Shaders", shaderPath), shaderType);
}
std::shared_ptr<sf::Shader> loadFullShaderWithGeo(const std::string& vertPath,
                                                  const std::string& geoPath,
                                                  const std::string& fragPath) {
    return ShaderManager::loadFullShaderWithGeo(
        ludork::global::manager::assetFile("Shaders", vertPath),
        ludork::global::manager::assetFile("Shaders", geoPath),
        ludork::global::manager::assetFile("Shaders", fragPath));
}
std::shared_ptr<sf::Shader> loadGeoShader(const std::string& vertPath,
                                          const std::string& geoPath,
                                          const std::string& fragPath) {
    return loadFullShaderWithGeo(vertPath, geoPath, fragPath);
}
