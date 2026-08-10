#include "UiPreviewCurveResolver.hpp"

#include <Runtime/EngineState.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <UI/Canvas.hpp>
#include <UI/UiAssetRuntime.hpp>
#include <UI/UiControlAdapterRegistry.hpp>
#include <UI/UiResources.hpp>
#include <UI/UIState.hpp>
#include <Utf8Path.hpp>
#include <Utils/File.hpp>
#include <Utils/ShaderLoader.hpp>

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/System/Vector2.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace {

constexpr std::int64_t protocolVersion = 4;
constexpr std::uint32_t maximumMessageSize = 64u * 1024u * 1024u;
constexpr unsigned int maximumAtlasSize = 2048;
constexpr unsigned int atlasGutter = 1;
constexpr float neutralHueEpsilon = 0.0001f;
constexpr float minimumRenderScale = 0.01f;

void configureProtocolStreams() {
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
        throw std::runtime_error(
            "Failed to set preview protocol stdin to binary mode");
    }
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
        throw std::runtime_error(
            "Failed to set preview protocol stdout to binary mode");
    }
#endif
}

const RuntimeValue* findValue(const RuntimeValue::Map& values,
                              const std::string& name) {
    const auto iterator = values.find(name);
    return iterator == values.end() ? nullptr : &iterator->second;
}

const RuntimeValue::Map& requireMap(const RuntimeValue& value,
                                    const std::string& source) {
    const RuntimeValue::Map* map = value.getIf<RuntimeValue::Map>();
    if (map == nullptr) {
        throw std::invalid_argument(source + " must be an object");
    }
    return *map;
}

const RuntimeValue::Array& requireArray(const RuntimeValue& value,
                                        const std::string& source) {
    const RuntimeValue::Array* array = value.getIf<RuntimeValue::Array>();
    if (array == nullptr) {
        throw std::invalid_argument(source + " must be an array");
    }
    return *array;
}

const RuntimeValue& requireValue(const RuntimeValue::Map& values,
                                 const std::string& name,
                                 const std::string& source) {
    const RuntimeValue* value = findValue(values, name);
    if (value == nullptr) {
        throw std::invalid_argument(source + " is missing " + name);
    }
    return *value;
}

const std::string& requireString(const RuntimeValue& value,
                                 const std::string& source) {
    const std::string* text = value.getIf<std::string>();
    if (text == nullptr) {
        throw std::invalid_argument(source + " must be a string");
    }
    return *text;
}

std::int64_t requireInteger(const RuntimeValue& value,
                            const std::string& source) {
    const std::int64_t* integer = value.getIf<std::int64_t>();
    if (integer == nullptr) {
        throw std::invalid_argument(source + " must be an integer");
    }
    return *integer;
}

double requireNumber(const RuntimeValue& value, const std::string& source) {
    if (const std::int64_t* integer = value.getIf<std::int64_t>()) {
        return static_cast<double>(*integer);
    }
    const double* number = value.getIf<double>();
    if (number == nullptr || !std::isfinite(*number)) {
        throw std::invalid_argument(source + " must be a finite number");
    }
    return *number;
}

int requireInt32(const RuntimeValue& value, const std::string& source) {
    const std::int64_t integer = requireInteger(value, source);
    if (integer < std::numeric_limits<int>::min() ||
        integer > std::numeric_limits<int>::max()) {
        throw std::invalid_argument(source + " is outside the integer range");
    }
    return static_cast<int>(integer);
}

std::uint64_t checkedPixelByteCount(unsigned int width,
                                    unsigned int height,
                                    const std::string& source) {
    const std::uint64_t byteCount = static_cast<std::uint64_t>(width) *
                                    static_cast<std::uint64_t>(height) * 4u;
    if (byteCount >
        static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::overflow_error(source + " is too large");
    }
    return byteCount;
}

RuntimeValue::Map object(
    std::initializer_list<std::pair<const std::string, RuntimeValue>> values) {
    RuntimeValue::Map result;
    for (const auto& [name, value] : values) {
        result.emplace(name, value);
    }
    return result;
}

RuntimeValue number(float value) {
    return RuntimeValue(static_cast<double>(value));
}

std::string pathText(const std::filesystem::path& path) {
    return ludork::standard::pathToUtf8(path);
}

std::filesystem::path safeProjectPath(const std::string& value,
                                      const std::string& source) {
    const std::filesystem::path relative =
        ludork::standard::pathFromUtf8(value).lexically_normal();
    if (relative.empty() || relative.is_absolute()) {
        throw std::invalid_argument(source +
                                    " must be a relative project path");
    }
    for (const std::filesystem::path& part : relative) {
        if (part == "..") {
            throw std::invalid_argument(source +
                                        " cannot traverse parent folders");
        }
    }
    return std::filesystem::current_path() / relative;
}

std::string settingString(const RuntimeValue::Map& config,
                          const std::string& name) {
    const RuntimeValue::Map& setting =
        requireMap(requireValue(config, name, "System config"),
                   "System config." + name);
    return requireString(
        requireValue(setting, "value", "System config." + name),
        "System config." + name + ".value");
}

std::int64_t settingInteger(const RuntimeValue::Map& config,
                            const std::string& name) {
    const RuntimeValue::Map& setting =
        requireMap(requireValue(config, name, "System config"),
                   "System config." + name);
    return requireInteger(
        requireValue(setting, "value", "System config." + name),
        "System config." + name + ".value");
}

void configureUiResources() {
    const RuntimeValue configValue =
        getJSONData(std::filesystem::path(".") / "Data" / "Configs" /
                    "System.json");
    const RuntimeValue::Map& config =
        requireMap(configValue, "Data/Configs/System.json");
    const RuntimeValue::Map& fonts =
        requireMap(requireValue(config, "fonts", "System config"),
                   "System config.fonts");
    const RuntimeValue::Array& fontNames =
        requireArray(requireValue(fonts, "value", "System config.fonts"),
                     "System config.fonts.value");
    if (fontNames.empty()) {
        throw std::invalid_argument(
            "System config must declare at least one font");
    }
    const std::string& fontName =
        requireString(fontNames.front(), "System config.fonts.value[0]");
    const std::string base =
        findValue(fonts, "base") == nullptr
            ? "Fonts"
            : requireString(*findValue(fonts, "base"),
                            "System config.fonts.base");
    const std::filesystem::path fontPath =
        safeProjectPath("Assets/" + base + "/" + fontName,
                        "System config font");
    std::shared_ptr<sf::Font> font = std::make_shared<sf::Font>();
    if (!font->openFromFile(fontPath)) {
        throw std::runtime_error("Failed to load preview font: " +
                                 pathText(fontPath));
    }
    const std::int64_t size = settingInteger(config, "fontSize");
    if (size <= 0 || size > std::numeric_limits<int>::max()) {
        throw std::invalid_argument(
            "System config fontSize must be a positive integer");
    }
    defaultFont = std::move(font);
    defaultFontSize = static_cast<int>(size);
    defaultWindowskinName = settingString(config, "windowskinName");
}

std::array<std::uint8_t, 4> littleEndian(std::uint32_t value) {
    return {
        static_cast<std::uint8_t>(value & 0xffu),
        static_cast<std::uint8_t>((value >> 8u) & 0xffu),
        static_cast<std::uint8_t>((value >> 16u) & 0xffu),
        static_cast<std::uint8_t>((value >> 24u) & 0xffu),
    };
}

std::optional<std::string> readMessage() {
    std::array<std::uint8_t, 4> lengthBytes{};
    std::cin.read(reinterpret_cast<char*>(lengthBytes.data()),
                  static_cast<std::streamsize>(lengthBytes.size()));
    if (std::cin.gcount() == 0 && std::cin.eof()) {
        return std::nullopt;
    }
    if (std::cin.gcount() !=
        static_cast<std::streamsize>(lengthBytes.size())) {
        throw std::runtime_error("Truncated preview protocol length");
    }
    const std::uint32_t length =
        static_cast<std::uint32_t>(lengthBytes[0]) |
        (static_cast<std::uint32_t>(lengthBytes[1]) << 8u) |
        (static_cast<std::uint32_t>(lengthBytes[2]) << 16u) |
        (static_cast<std::uint32_t>(lengthBytes[3]) << 24u);
    if (length == 0 || length > maximumMessageSize) {
        throw std::runtime_error("Invalid preview protocol message length");
    }
    std::string message(length, '\0');
    std::cin.read(message.data(), static_cast<std::streamsize>(length));
    if (std::cin.gcount() != static_cast<std::streamsize>(length)) {
        throw std::runtime_error("Truncated preview protocol message");
    }
    return message;
}

void writeMessage(const RuntimeValue& value) {
    const std::string message = stringifyJSON(value);
    if (message.empty() || message.size() > maximumMessageSize) {
        throw std::runtime_error(
            "Preview protocol response has an invalid size");
    }
    const auto length =
        littleEndian(static_cast<std::uint32_t>(message.size()));
    std::cout.write(reinterpret_cast<const char*>(length.data()),
                    static_cast<std::streamsize>(length.size()));
    std::cout.write(message.data(),
                    static_cast<std::streamsize>(message.size()));
    std::cout.flush();
    if (!std::cout) {
        throw std::runtime_error("Failed to write preview protocol response");
    }
}

sf::Vector2u designSize(const RuntimeValue::Map& asset) {
    const RuntimeValue::Map& size =
        requireMap(requireValue(asset, "designSize", "UI asset"),
                   "UI asset.designSize");
    const double width =
        requireNumber(requireValue(size, "width", "UI asset.designSize"),
                      "UI asset.designSize.width");
    const double height =
        requireNumber(requireValue(size, "height", "UI asset.designSize"),
                      "UI asset.designSize.height");
    if (width <= 0.0 || height <= 0.0 ||
        width > static_cast<double>(std::numeric_limits<unsigned int>::max()) ||
        height >
            static_cast<double>(std::numeric_limits<unsigned int>::max())) {
        throw std::invalid_argument(
            "UI asset designSize is outside the preview range");
    }
    const unsigned int roundedWidth =
        static_cast<unsigned int>(std::lround(width));
    const unsigned int roundedHeight =
        static_cast<unsigned int>(std::lround(height));
    if (roundedWidth == 0 || roundedHeight == 0) {
        throw std::invalid_argument(
            "UI asset designSize must produce positive pixels");
    }
    return {roundedWidth, roundedHeight};
}

struct RenderTargetSpec {
    float renderScale;
    sf::Vector2u size;
};

sf::Vector2u scaledRenderSize(const sf::Vector2u& design,
                              float renderScale) {
    const float width = static_cast<float>(design.x) * renderScale;
    const float height = static_cast<float>(design.y) * renderScale;
    if (!std::isfinite(width) || !std::isfinite(height) || width < 1.0f ||
        height < 1.0f) {
        return {};
    }
    return {
        static_cast<unsigned int>(width),
        static_cast<unsigned int>(height),
    };
}

RenderTargetSpec renderTargetSpec(const sf::Vector2u& design,
                                  double requestedScale) {
    if (requestedScale < static_cast<double>(minimumRenderScale)) {
        throw std::invalid_argument(
            "Render request.renderScale is below the engine minimum");
    }
    const unsigned int maximumTextureSize = sf::Texture::getMaximumSize();
    if (maximumTextureSize == 0) {
        throw std::runtime_error(
            "OpenGL does not report a usable maximum texture size");
    }
    const double maximumScale = std::min(
        static_cast<double>(maximumTextureSize) /
            static_cast<double>(design.x),
        static_cast<double>(maximumTextureSize) /
            static_cast<double>(design.y));
    if (maximumScale < static_cast<double>(minimumRenderScale)) {
        throw std::invalid_argument(
            "UI asset designSize cannot fit the maximum texture size");
    }
    float renderScale = static_cast<float>(
        std::min(requestedScale, maximumScale));
    sf::Vector2u size = scaledRenderSize(design, renderScale);
    while ((size.x > maximumTextureSize ||
            size.y > maximumTextureSize) &&
           renderScale > minimumRenderScale) {
        renderScale = std::nextafter(renderScale, 0.0f);
        size = scaledRenderSize(design, renderScale);
    }
    if (renderScale < minimumRenderScale || size.x == 0 || size.y == 0 ||
        size.x > maximumTextureSize || size.y > maximumTextureSize) {
        throw std::invalid_argument(
            "Render request.renderScale cannot produce a valid preview");
    }
    return {renderScale, size};
}

void renderNestedCanvases(const std::shared_ptr<ControlBase>& control) {
    for (const std::shared_ptr<ControlBase>& child :
         control->getChildren()) {
        if (child != nullptr && child->getVisible()) {
            renderNestedCanvases(child);
        }
    }
    const std::shared_ptr<Canvas> canvas =
        std::dynamic_pointer_cast<Canvas>(control);
    if (canvas != nullptr) {
        canvas->render();
    }
}

std::vector<std::uint8_t> bgraFromPremultipliedRgba(
    const sf::Image& image, const sf::Vector2u& size) {
    const std::uint64_t byteCount =
        checkedPixelByteCount(size.x, size.y, "Preview frame");
    const std::uint8_t* source = image.getPixelsPtr();
    if (source == nullptr) {
        throw std::runtime_error("Preview renderer returned no pixels");
    }
    std::vector<std::uint8_t> result(static_cast<std::size_t>(byteCount));
    for (std::size_t index = 0; index < result.size(); index += 4) {
        result[index] = source[index + 2];
        result[index + 1] = source[index + 1];
        result[index + 2] = source[index];
        result[index + 3] = source[index + 3];
    }
    return result;
}

std::vector<std::uint8_t> premultipliedBgraFromStraightRgba(
    const sf::Image& image, const sf::Vector2u& size) {
    const std::uint64_t byteCount =
        checkedPixelByteCount(size.x, size.y, "Preview frame");
    const std::uint8_t* source = image.getPixelsPtr();
    if (source == nullptr) {
        throw std::runtime_error("Preview renderer returned no pixels");
    }
    std::vector<std::uint8_t> result(static_cast<std::size_t>(byteCount));
    for (std::size_t index = 0; index < result.size(); index += 4) {
        const std::uint32_t alpha = source[index + 3];
        result[index] = static_cast<std::uint8_t>(
            (static_cast<std::uint32_t>(source[index + 2]) * alpha + 127u) /
            255u);
        result[index + 1] = static_cast<std::uint8_t>(
            (static_cast<std::uint32_t>(source[index + 1]) * alpha + 127u) /
            255u);
        result[index + 2] = static_cast<std::uint8_t>(
            (static_cast<std::uint32_t>(source[index]) * alpha + 127u) /
            255u);
        result[index + 3] = static_cast<std::uint8_t>(alpha);
    }
    return result;
}

std::vector<std::uint8_t> renderFrame(
    const std::shared_ptr<UiAssetInstance>& instance,
    const sf::Vector2u& size) {
    const std::shared_ptr<ControlBase> root = instance->getRoot();
    sf::RenderTexture target(size);
    target.clear(sf::Color::Transparent);
    if (root->getVisible()) {
        renderNestedCanvases(root);
        target.draw(*root);
    }
    target.display();
    const sf::Image image = target.getTexture().copyToImage();
    return bgraFromPremultipliedRgba(image, size);
}

bool effectiveVisible(const std::shared_ptr<ControlBase>& control) {
    std::shared_ptr<ControlBase> current = control;
    while (current != nullptr) {
        if (!current->getVisible()) {
            return false;
        }
        current = current->getParent();
    }
    return true;
}

sf::FloatRect effectiveClip(const std::shared_ptr<ControlBase>& control,
                            const sf::FloatRect& rootClip) {
    sf::FloatRect clip = rootClip;
    std::shared_ptr<ControlBase> current = control;
    while (current != nullptr) {
        if (std::dynamic_pointer_cast<Canvas>(current) != nullptr) {
            const std::optional<sf::FloatRect> intersection =
                clip.findIntersection(current->getAbsoluteBounds());
            if (!intersection.has_value()) {
                return {{0.0f, 0.0f}, {0.0f, 0.0f}};
            }
            clip = *intersection;
        }
        current = current->getParent();
    }
    return clip;
}

bool insideEffectiveClip(const std::shared_ptr<ControlBase>& control,
                         const sf::Vector2f& point,
                         const sf::FloatRect& rootClip,
                         float renderScale) {
    if (!rootClip.contains(point)) {
        return false;
    }
    std::shared_ptr<ControlBase> current = control;
    while (current != nullptr) {
        if (std::dynamic_pointer_cast<Canvas>(current) != nullptr) {
            const sf::Vector2f local =
                current->screenRenderTransform()
                    .getInverse()
                    .transformPoint(point) /
                renderScale;
            if (!current->getLocalBounds().contains(local)) {
                return false;
            }
        }
        current = current->getParent();
    }
    return true;
}

RuntimeValue::Array nodeGeometry(
    const std::shared_ptr<UiAssetInstance>& instance,
    const sf::Vector2u& size, float renderScale) {
    const sf::FloatRect rootClip(
        {0.0f, 0.0f},
        {static_cast<float>(size.x), static_cast<float>(size.y)});
    RuntimeValue::Array result;
    for (const UiAssetNodeView& node : instance->getNodeViews()) {
        const sf::FloatRect clip = effectiveClip(node.control, rootClip);
        result.emplace_back(object({
            {"nodeId", RuntimeValue(node.nodeId)},
            {"x", number(node.bounds.position.x / renderScale)},
            {"y", number(node.bounds.position.y / renderScale)},
            {"width", number(node.bounds.size.x / renderScale)},
            {"height", number(node.bounds.size.y / renderScale)},
            {"clipX", number(clip.position.x / renderScale)},
            {"clipY", number(clip.position.y / renderScale)},
            {"clipWidth", number(clip.size.x / renderScale)},
            {"clipHeight", number(clip.size.y / renderScale)},
            {"drawOrder",
             RuntimeValue(static_cast<std::int64_t>(node.drawOrder))},
            {"visible", RuntimeValue(effectiveVisible(node.control))},
        }));
    }
    return result;
}

class FrameFiles {
public:
    FrameFiles() {
        const auto stamp =
            std::chrono::steady_clock::now().time_since_epoch().count();
        const std::filesystem::path temporary =
            std::filesystem::temp_directory_path();
        for (std::size_t index = 0; index < paths_.size(); ++index) {
            paths_[index] =
                temporary /
                ("LudorkUiPreview-" + std::to_string(stamp) + "-" +
                 std::to_string(index) + ".bin");
        }
    }

    ~FrameFiles() {
        for (const std::filesystem::path& path : paths_) {
            std::error_code error;
            std::filesystem::remove(path, error);
        }
    }

    const std::filesystem::path& write(
        const std::vector<std::uint8_t>& pixels) {
        current_ = (current_ + 1) % paths_.size();
        const std::filesystem::path& path = paths_[current_];
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output) {
            throw std::runtime_error(
                "Failed to open preview frame buffer: " + pathText(path));
        }
        output.write(reinterpret_cast<const char*>(pixels.data()),
                     static_cast<std::streamsize>(pixels.size()));
        if (!output) {
            throw std::runtime_error(
                "Failed to write preview frame buffer: " + pathText(path));
        }
        return path;
    }

private:
    std::array<std::filesystem::path, 2> paths_;
    std::size_t current_ = 0;
};

struct FileStamp {
    bool exists = false;
    std::uintmax_t size = 0;
    std::filesystem::file_time_type modified{};

    bool operator==(const FileStamp&) const = default;
};

FileStamp fileStamp(const std::filesystem::path& path) {
    std::error_code error;
    const bool exists = std::filesystem::is_regular_file(path, error);
    if (!exists || error) {
        return {};
    }
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error) {
        return {};
    }
    const std::filesystem::file_time_type modified =
        std::filesystem::last_write_time(path, error);
    if (error) {
        return {};
    }
    return {true, size, modified};
}

bool pathInside(const std::filesystem::path& root,
                const std::filesystem::path& path) {
    const std::filesystem::path relative = path.lexically_relative(root);
    if (relative.empty() || relative.is_absolute()) {
        return false;
    }
    for (const std::filesystem::path& part : relative) {
        if (part == "..") {
            return false;
        }
    }
    return true;
}

std::filesystem::path projectAssetPath(
    const std::filesystem::path& projectPath, const std::string& value,
    const std::filesystem::path& defaultFolder, const std::string& source) {
    if (value.empty()) {
        throw std::invalid_argument(source + " cannot be empty");
    }
    const std::filesystem::path requested =
        ludork::standard::pathFromUtf8(value).lexically_normal();
    std::filesystem::path candidate;
    if (requested.is_absolute()) {
        candidate = requested;
    } else {
        const std::string generic =
            ludork::standard::pathToGenericUtf8(requested);
        candidate = generic.starts_with("Assets/")
                        ? projectPath / requested
                        : projectPath / defaultFolder / requested;
    }
    candidate = std::filesystem::weakly_canonical(candidate);
    if (!pathInside(projectPath, candidate)) {
        throw std::invalid_argument(source + " is outside the project");
    }
    return candidate;
}

std::filesystem::path actorTexturePath(
    const std::filesystem::path& projectPath, const std::string& value) {
    return projectAssetPath(projectPath, value, "Assets/Characters",
                            "Actor texturePath");
}

std::filesystem::path actorShaderPath(
    const std::filesystem::path& projectPath, const std::string& value) {
    std::filesystem::path path = projectAssetPath(
        projectPath, value, "Assets/Shaders", "Actor shaderPath");
    if (fileStamp(path).exists) {
        return path;
    }
    const std::string extension =
        ludork::standard::pathToUtf8(path.extension());
    if (extension == ".frag" || extension == ".vert" ||
        extension == ".geom") {
        std::filesystem::path encrypted = path;
        encrypted.replace_extension(extension + "c");
        if (fileStamp(encrypted).exists) {
            return encrypted;
        }
    }
    return path;
}

struct ActorVisualRequest {
    std::string id;
    std::filesystem::path texturePath;
    sf::IntRect textureRect;
    std::filesystem::path shaderPath;
    float hue = 0.0f;
};

struct PackedActorVisual {
    ActorVisualRequest visual;
    std::size_t page = 0;
    sf::Vector2u position;
    bool shaderError = false;
    std::string error;
};

struct AtlasLayout {
    sf::Vector2u size;
    std::vector<std::size_t> visualIndices;
};

struct TextureCacheEntry {
    FileStamp stamp;
    std::shared_ptr<sf::Texture> texture;
    std::string error;
};

struct ShaderCacheEntry {
    FileStamp stamp;
    std::shared_ptr<sf::Shader> shader;
    std::string error;
};

class ActorBatchRenderer {
public:
    void reset(const std::filesystem::path& projectPath) {
        projectPath_ = projectPath;
        textures_.clear();
        shaders_.clear();
        pageBuffers_.clear();
        effectBuffers_.clear();
        activeEffectBufferKeys_.clear();
        errorShader_.reset();
    }

    RuntimeValue render(const RuntimeValue::Map& request,
                        FrameFiles& frameFiles) {
        const std::int64_t generation = requireInteger(
            requireValue(request, "generation", "Actor render request"),
            "Actor render request.generation");
        const float time = static_cast<float>(requireNumber(
            requireValue(request, "time", "Actor render request"),
            "Actor render request.time"));
        const RuntimeValue::Array& itemValues = requireArray(
            requireValue(request, "items", "Actor render request"),
            "Actor render request.items");
        std::vector<PackedActorVisual> visuals;
        visuals.reserve(itemValues.size());
        for (std::size_t index = 0; index < itemValues.size(); ++index) {
            visuals.push_back(parseVisual(itemValues[index], index));
        }
        std::stable_sort(
            visuals.begin(), visuals.end(),
            [](const PackedActorVisual& left,
               const PackedActorVisual& right) {
                return left.visual.id < right.visual.id;
            });
        const unsigned int limit = std::min(
            maximumAtlasSize, sf::Texture::getMaximumSize());
        if (limit <= atlasGutter * 2) {
            throw std::runtime_error(
                "OpenGL maximum texture size cannot hold an actor atlas");
        }
        std::vector<AtlasLayout> layouts = pack(visuals, limit);
        pageBuffers_.resize(layouts.size());
        std::unordered_set<std::uint64_t> requestedEffectBufferKeys;
        for (const PackedActorVisual& visual : visuals) {
            const bool hasHue =
                visual.visual.hue > neutralHueEpsilon &&
                std::abs(visual.visual.hue - 360.0f) > neutralHueEpsilon;
            if (!visual.visual.shaderPath.empty() && hasHue) {
                requestedEffectBufferKeys.insert(
                    effectBufferKey(visual.visual.textureRect.size));
            }
        }
        pruneEffectBuffers(requestedEffectBufferKeys);
        activeEffectBufferKeys_.clear();
        std::vector<std::vector<std::uint8_t>> pagePixels;
        pagePixels.reserve(layouts.size());
        for (std::size_t pageIndex = 0; pageIndex < layouts.size();
            ++pageIndex) {
            pagePixels.push_back(
                renderPage(layouts[pageIndex], visuals, pageIndex, time));
        }
        pruneEffectBuffers(activeEffectBufferKeys_);
        std::vector<std::uint8_t> sharedPixels;
        RuntimeValue::Array pages;
        for (std::size_t pageIndex = 0; pageIndex < layouts.size();
             ++pageIndex) {
            const AtlasLayout& layout = layouts[pageIndex];
            const std::int64_t offset =
                static_cast<std::int64_t>(sharedPixels.size());
            const std::vector<std::uint8_t>& pixels = pagePixels[pageIndex];
            sharedPixels.insert(sharedPixels.end(), pixels.begin(),
                                pixels.end());
            pages.emplace_back(object({
                {"width", RuntimeValue(static_cast<std::int64_t>(layout.size.x))},
                {"height", RuntimeValue(static_cast<std::int64_t>(layout.size.y))},
                {"stride", RuntimeValue(static_cast<std::int64_t>(layout.size.x) * 4)},
                {"sharedMemory", RuntimeValue(object({
                    {"filePath", RuntimeValue(std::string())},
                    {"offset", RuntimeValue(offset)},
                }))},
            }));
        }
        const std::filesystem::path& framePath =
            frameFiles.write(sharedPixels);
        for (RuntimeValue& pageValue : pages) {
            RuntimeValue::Map& page = *pageValue.getIf<RuntimeValue::Map>();
            RuntimeValue::Map& sharedMemory =
                *page["sharedMemory"].getIf<RuntimeValue::Map>();
            sharedMemory["filePath"] = RuntimeValue(pathText(framePath));
        }
        RuntimeValue::Array items;
        items.reserve(visuals.size());
        for (const PackedActorVisual& visual : visuals) {
            items.emplace_back(object({
                {"id", RuntimeValue(visual.visual.id)},
                {"page", RuntimeValue(static_cast<std::int64_t>(visual.page))},
                {"x", RuntimeValue(static_cast<std::int64_t>(visual.position.x))},
                {"y", RuntimeValue(static_cast<std::int64_t>(visual.position.y))},
                {"width", RuntimeValue(static_cast<std::int64_t>(visual.visual.textureRect.size.x))},
                {"height", RuntimeValue(static_cast<std::int64_t>(visual.visual.textureRect.size.y))},
                {"shaderError", RuntimeValue(visual.shaderError)},
                {"error", RuntimeValue(visual.error)},
            }));
        }
        return RuntimeValue(object({
            {"type", RuntimeValue("actorFrame")},
            {"generation", RuntimeValue(generation)},
            {"pixelFormat", RuntimeValue("Bgra8888Premultiplied")},
            {"pages", RuntimeValue(std::move(pages))},
            {"items", RuntimeValue(std::move(items))},
        }));
    }

private:
    PackedActorVisual parseVisual(const RuntimeValue& value,
                                  std::size_t index) const {
        const std::string source =
            "Actor render request.items[" + std::to_string(index) + "]";
        const RuntimeValue::Map& item = requireMap(value, source);
        const RuntimeValue::Map& rect = requireMap(
            requireValue(item, "textureRect", source),
            source + ".textureRect");
        const int width = requireInt32(
            requireValue(rect, "width", source + ".textureRect"),
            source + ".textureRect.width");
        const int height = requireInt32(
            requireValue(rect, "height", source + ".textureRect"),
            source + ".textureRect.height");
        if (width <= 0 || height <= 0) {
            throw std::invalid_argument(source +
                                        ".textureRect must have positive size");
        }
        const std::string& shaderText = requireString(
            requireValue(item, "shaderPath", source),
            source + ".shaderPath");
        float hue = static_cast<float>(requireNumber(
            requireValue(item, "hue", source), source + ".hue"));
        hue = std::fmod(hue, 360.0f);
        if (hue < 0.0f) {
            hue += 360.0f;
        }
        ActorVisualRequest visual{
            requireString(requireValue(item, "id", source), source + ".id"),
            actorTexturePath(
                projectPath_,
                requireString(requireValue(item, "texturePath", source),
                              source + ".texturePath")),
            sf::IntRect(
                {requireInt32(requireValue(rect, "x", source + ".textureRect"),
                              source + ".textureRect.x"),
                 requireInt32(requireValue(rect, "y", source + ".textureRect"),
                              source + ".textureRect.y")},
                {width, height}),
            shaderText.empty()
                ? std::filesystem::path()
                : actorShaderPath(projectPath_, shaderText),
            hue};
        return {std::move(visual)};
    }

    std::vector<AtlasLayout> pack(std::vector<PackedActorVisual>& visuals,
                                  unsigned int limit) const {
        std::vector<AtlasLayout> pages;
        if (visuals.empty()) {
            return pages;
        }
        pages.emplace_back();
        unsigned int x = atlasGutter;
        unsigned int y = atlasGutter;
        unsigned int rowHeight = 0;
        for (std::size_t index = 0; index < visuals.size(); ++index) {
            PackedActorVisual& visual = visuals[index];
            const unsigned int width =
                static_cast<unsigned int>(visual.visual.textureRect.size.x);
            const unsigned int height =
                static_cast<unsigned int>(visual.visual.textureRect.size.y);
            if (width + atlasGutter * 2 > limit ||
                height + atlasGutter * 2 > limit) {
                throw std::invalid_argument(
                    "Actor frame is larger than the maximum atlas page");
            }
            if (x + width + atlasGutter > limit) {
                x = atlasGutter;
                y += rowHeight + atlasGutter;
                rowHeight = 0;
            }
            if (y + height + atlasGutter > limit) {
                pages.emplace_back();
                x = atlasGutter;
                y = atlasGutter;
                rowHeight = 0;
            }
            AtlasLayout& page = pages.back();
            visual.page = pages.size() - 1;
            visual.position = {x, y};
            page.visualIndices.push_back(index);
            page.size.x = std::max(page.size.x, x + width + atlasGutter);
            page.size.y = std::max(page.size.y, y + height + atlasGutter);
            x += width + atlasGutter;
            rowHeight = std::max(rowHeight, height);
        }
        return pages;
    }

    std::vector<std::uint8_t> renderPage(
        const AtlasLayout& layout, std::vector<PackedActorVisual>& visuals,
        std::size_t pageIndex, float time) {
        sf::RenderTexture& target = pageBuffer(layout.size, pageIndex);
        target.setSmooth(false);
        target.clear(sf::Color::Transparent);
        for (const std::size_t index : layout.visualIndices) {
            renderVisual(target, visuals[index], time);
        }
        target.display();
        const sf::Image image = target.getTexture().copyToImage();
        return premultipliedBgraFromStraightRgba(image, layout.size);
    }

    sf::RenderTexture& pageBuffer(const sf::Vector2u& size,
                                  std::size_t pageIndex) {
        std::unique_ptr<sf::RenderTexture>& buffer = pageBuffers_[pageIndex];
        if (buffer == nullptr || buffer->getSize() != size) {
            buffer = std::make_unique<sf::RenderTexture>(size);
            buffer->setSmooth(false);
        }
        return *buffer;
    }

    void renderVisual(sf::RenderTexture& target, PackedActorVisual& packed,
                      float time) {
        const TextureCacheEntry& textureEntry =
            loadTexture(packed.visual.texturePath);
        if (textureEntry.texture == nullptr) {
            packed.error = textureEntry.error;
            return;
        }
        const sf::Vector2u textureSize = textureEntry.texture->getSize();
        const sf::IntRect& rect = packed.visual.textureRect;
        if (rect.position.x < 0 || rect.position.y < 0 ||
            static_cast<unsigned int>(rect.position.x + rect.size.x) >
                textureSize.x ||
            static_cast<unsigned int>(rect.position.y + rect.size.y) >
                textureSize.y) {
            packed.error = "Actor textureRect is outside the texture";
            return;
        }
        std::shared_ptr<sf::Shader> actorShader;
        if (!packed.visual.shaderPath.empty()) {
            const ShaderCacheEntry& shaderEntry =
                loadShader(packed.visual.shaderPath);
            actorShader = shaderEntry.shader;
            if (actorShader == nullptr) {
                packed.shaderError = true;
                packed.error = shaderEntry.error;
            }
        }
        const bool hasHue = packed.visual.hue > neutralHueEpsilon &&
                            std::abs(packed.visual.hue - 360.0f) >
                                neutralHueEpsilon;
        if (packed.shaderError) {
            drawError(target, *textureEntry.texture, rect, packed.position);
            return;
        }
        if (actorShader != nullptr && hasHue) {
            sf::RenderTexture& effect = effectBuffer(rect.size);
            effect.clear(sf::Color::Transparent);
            sf::Sprite source(*textureEntry.texture, rect);
            bindActorShader(*actorShader, *textureEntry.texture, rect, time);
            sf::RenderStates actorStates;
            actorStates.shader = actorShader.get();
            effect.draw(source, actorStates);
            effect.display();
            sf::Sprite composed(effect.getTexture());
            composed.setPosition(sf::Vector2f(packed.position));
            sf::Shader& hueShader = requireHueShader();
            bindHueShader(hueShader, packed.visual.hue);
            sf::RenderStates hueStates;
            hueStates.shader = &hueShader;
            target.draw(composed, hueStates);
            return;
        }
        sf::Sprite source(*textureEntry.texture, rect);
        source.setPosition(sf::Vector2f(packed.position));
        sf::RenderStates states(sf::BlendNone);
        if (actorShader != nullptr) {
            bindActorShader(*actorShader, *textureEntry.texture, rect, time);
            states.shader = actorShader.get();
        } else if (hasHue) {
            sf::Shader& hueShader = requireHueShader();
            bindHueShader(hueShader, packed.visual.hue);
            states.shader = &hueShader;
        }
        target.draw(source, states);
    }

    const TextureCacheEntry& loadTexture(const std::filesystem::path& path) {
        const std::string key = pathText(path);
        const FileStamp stamp = fileStamp(path);
        const auto iterator = textures_.find(key);
        if (iterator != textures_.end() && iterator->second.stamp == stamp) {
            return iterator->second;
        }
        TextureCacheEntry entry;
        entry.stamp = stamp;
        if (!stamp.exists) {
            entry.error = "Texture file not found: " + key;
        } else {
            entry.texture = std::make_shared<sf::Texture>();
            if (!entry.texture->loadFromFile(path, false)) {
                entry.texture.reset();
                entry.error = "Texture load failed: " + key;
            } else {
                entry.texture->setSmooth(false);
            }
        }
        return textures_.insert_or_assign(key, std::move(entry)).first->second;
    }

    const ShaderCacheEntry& loadShader(const std::filesystem::path& path) {
        const std::string key = pathText(path);
        const FileStamp stamp = fileStamp(path);
        const auto iterator = shaders_.find(key);
        if (iterator != shaders_.end() && iterator->second.stamp == stamp) {
            return iterator->second;
        }
        ShaderCacheEntry entry;
        entry.stamp = stamp;
        const ShaderLoadResult result =
            ShaderLoader::load(key, sf::Shader::Type::Fragment);
        entry.shader = result.shader;
        entry.error = result.error;
        return shaders_.insert_or_assign(key, std::move(entry)).first->second;
    }

    sf::RenderTexture& effectBuffer(const sf::Vector2i& size) {
        const std::uint64_t key = effectBufferKey(size);
        activeEffectBufferKeys_.insert(key);
        const auto iterator = effectBuffers_.find(key);
        if (iterator != effectBuffers_.end()) {
            return *iterator->second;
        }
        std::unique_ptr<sf::RenderTexture> buffer =
            std::make_unique<sf::RenderTexture>(
                sf::Vector2u(static_cast<unsigned int>(size.x),
                             static_cast<unsigned int>(size.y)));
        buffer->setSmooth(false);
        return *effectBuffers_.emplace(key, std::move(buffer)).first->second;
    }

    static std::uint64_t effectBufferKey(const sf::Vector2i& size) {
        return static_cast<std::uint64_t>(
                   static_cast<unsigned int>(size.x))
                   << 32u |
               static_cast<unsigned int>(size.y);
    }

    void pruneEffectBuffers(
        const std::unordered_set<std::uint64_t>& retainedKeys) {
        for (auto iterator = effectBuffers_.begin();
             iterator != effectBuffers_.end();) {
            if (!retainedKeys.contains(iterator->first)) {
                iterator = effectBuffers_.erase(iterator);
            } else {
                ++iterator;
            }
        }
    }

    sf::Shader& requireHueShader() {
        const std::filesystem::path path =
            actorShaderPath(projectPath_, "Global/Hue.frag");
        const ShaderCacheEntry& entry = loadShader(path);
        if (entry.shader == nullptr) {
            throw std::runtime_error(entry.error);
        }
        return *entry.shader;
    }

    sf::Shader& requireErrorShader() {
        if (errorShader_ == nullptr) {
            constexpr const char* source = R"(
#ifdef GL_ES
precision highp float;
varying mediump vec4 sf_FrontColor;
varying mediump vec4 sf_TexCoord0;
#else
varying vec4 sf_FrontColor;
varying vec4 sf_TexCoord0;
#endif
uniform sampler2D texture;
void main()
{
    vec4 source = texture2D(texture, sf_TexCoord0.xy);
    gl_FragColor = vec4(source.r, 0.0, source.b, source.a) * sf_FrontColor;
}
)";
            errorShader_ = std::make_shared<sf::Shader>();
            if (!errorShader_->loadFromMemory(
                    source, sf::Shader::Type::Fragment)) {
                errorShader_.reset();
                throw std::runtime_error(
                    "Failed to compile actor shader error renderer");
            }
        }
        return *errorShader_;
    }

    void drawError(sf::RenderTexture& target, const sf::Texture& texture,
                   const sf::IntRect& rect,
                   const sf::Vector2u& position) {
        sf::Sprite source(texture, rect);
        source.setPosition(sf::Vector2f(position));
        sf::Shader& shader = requireErrorShader();
        shader.setUniform("texture", sf::Shader::CurrentTexture);
        sf::RenderStates states(sf::BlendNone);
        states.shader = &shader;
        target.draw(source, states);
    }

    static void bindActorShader(sf::Shader& shader,
                                const sf::Texture& texture,
                                const sf::IntRect& rect, float time) {
        const sf::Vector2u size = texture.getSize();
        shader.setUniform("texture", sf::Shader::CurrentTexture);
        shader.setUniform("time", time);
        shader.setUniform(
            "textureSize",
            sf::Vector2f(static_cast<float>(size.x),
                         static_cast<float>(size.y)));
        shader.setUniform(
            "textureRect",
            sf::Glsl::Vec4(static_cast<float>(rect.position.x),
                           static_cast<float>(rect.position.y),
                           static_cast<float>(rect.size.x),
                           static_cast<float>(rect.size.y)));
    }

    static void bindHueShader(sf::Shader& shader, float hue) {
        shader.setUniform("screenTex", sf::Shader::CurrentTexture);
        shader.setUniform("hue", hue);
    }

    std::filesystem::path projectPath_;
    std::unordered_map<std::string, TextureCacheEntry> textures_;
    std::unordered_map<std::string, ShaderCacheEntry> shaders_;
    std::vector<std::unique_ptr<sf::RenderTexture>> pageBuffers_;
    std::unordered_map<std::uint64_t, std::unique_ptr<sf::RenderTexture>>
        effectBuffers_;
    std::unordered_set<std::uint64_t> activeEffectBufferKeys_;
    std::shared_ptr<sf::Shader> errorShader_;
};

class Host {
public:
    ~Host() noexcept {
        instance_.reset();
        curveResolver_.clear();
        clearUiControlAdapterResourceCache();
        uiResources().reset();
    }

    RuntimeValue handle(const RuntimeValue& requestValue) {
        const RuntimeValue::Map& request =
            requireMap(requestValue, "Preview request");
        const std::string& type =
            requireString(requireValue(request, "type", "Preview request"),
                          "Preview request.type");
        if (type == "handshake") {
            return handshake(request);
        }
        if (!accepted_) {
            throw std::runtime_error(
                "Preview protocol handshake has not completed");
        }
        if (type == "render") {
            return render(request);
        }
        if (type == "hitTest") {
            return hitTest(request);
        }
        if (type == "renderActorBatch") {
            engineState().setScale(1.0f);
            return actorRenderer_.render(request, frameFiles_);
        }
        throw std::invalid_argument("Unknown preview request type: " + type);
    }

private:
    RuntimeValue handshake(const RuntimeValue::Map& request) {
        curveResolver_.clear();
        accepted_ = false;
        instance_.reset();
        generation_ = 0;
        designSize_ = {};
        renderSize_ = {};
        renderScale_ = 1.0f;
        const std::int64_t requestedProtocol = requireInteger(
            requireValue(request, "protocolVersion", "Handshake"),
            "Handshake.protocolVersion");
        const std::string& requestedFingerprint = requireString(
            requireValue(request, "adapterFingerprint", "Handshake"),
            "Handshake.adapterFingerprint");
        bool accepted =
            requestedProtocol == protocolVersion &&
            requestedFingerprint == uiControlAdapterFingerprint();
        std::string message;
        if (!accepted) {
            message =
                "UiPreviewHost protocol or adapter fingerprint "
                "is incompatible.";
        } else {
            const std::string& projectPath = requireString(
                requireValue(request, "projectPath", "Handshake"),
                "Handshake.projectPath");
            const std::filesystem::path project =
                std::filesystem::weakly_canonical(
                    ludork::standard::pathFromUtf8(projectPath));
            if (!std::filesystem::is_directory(project)) {
                throw std::invalid_argument(
                    "Handshake projectPath is not a directory");
            }
            std::filesystem::current_path(project);
            engineState().setScale(1.0f);
            curveResolver_.install(project);
            actorRenderer_.reset(project);
            accepted_ = true;
        }
        RuntimeValue::Array capabilities;
        capabilities.emplace_back(RuntimeValue("ui"));
        capabilities.emplace_back(RuntimeValue("actor"));
        return RuntimeValue(object({
            {"type", RuntimeValue("handshake")},
            {"accepted", RuntimeValue(accepted)},
            {"protocolVersion", RuntimeValue(protocolVersion)},
            {"adapterFingerprint",
             RuntimeValue(std::string(uiControlAdapterFingerprint()))},
            {"capabilities", RuntimeValue(std::move(capabilities))},
            {"message", RuntimeValue(std::move(message))},
        }));
    }

    RuntimeValue render(const RuntimeValue::Map& request) {
        const std::int64_t generation = requireInteger(
            requireValue(request, "generation", "Render request"),
            "Render request.generation");
        const std::string& assetKey = requireString(
            requireValue(request, "assetKey", "Render request"),
            "Render request.assetKey");
        const RuntimeValue& asset =
            requireValue(request, "asset", "Render request");
        const RuntimeValue::Map& assetMap =
            requireMap(asset, "Render request.asset");
        const RuntimeValue::Map& dependencies =
            requireMap(
                requireValue(request, "dependencies", "Render request"),
                "Render request.dependencies");
        const sf::Vector2u design = designSize(assetMap);
        const double requestedScale = requireNumber(
            requireValue(request, "renderScale", "Render request"),
            "Render request.renderScale");
        const RenderTargetSpec targetSpec =
            renderTargetSpec(design, requestedScale);
        engineState().setScale(targetSpec.renderScale);
        configureUiResources();
        std::shared_ptr<UiAssetInstance> instance =
            UiAssetRuntime::instance().instantiateSnapshot(
                assetKey, asset, dependencies, design, true);
        const std::vector<std::uint8_t> pixels =
            renderFrame(instance, targetSpec.size);
        const std::filesystem::path& framePath = frameFiles_.write(pixels);
        instance_ = std::move(instance);
        generation_ = generation;
        designSize_ = design;
        renderSize_ = targetSpec.size;
        renderScale_ = targetSpec.renderScale;
        return RuntimeValue(object({
            {"type", RuntimeValue("frame")},
            {"generation", RuntimeValue(generation)},
            {"designWidth",
             RuntimeValue(static_cast<std::int64_t>(design.x))},
            {"designHeight",
             RuntimeValue(static_cast<std::int64_t>(design.y))},
            {"width",
             RuntimeValue(static_cast<std::int64_t>(renderSize_.x))},
            {"height",
             RuntimeValue(static_cast<std::int64_t>(renderSize_.y))},
            {"stride",
             RuntimeValue(static_cast<std::int64_t>(renderSize_.x) * 4)},
            {"renderScale", number(renderScale_)},
            {"sharedMemory",
             RuntimeValue(object({
                 {"filePath", RuntimeValue(pathText(framePath))},
                 {"offset", RuntimeValue(std::int64_t{0})},
             }))},
            {"nodes",
             RuntimeValue(nodeGeometry(
                 instance_, renderSize_, renderScale_))},
        }));
    }

    RuntimeValue hitTest(const RuntimeValue::Map& request) const {
        const std::int64_t generation = requireInteger(
            requireValue(request, "generation", "Hit test request"),
            "Hit test request.generation");
        const sf::Vector2f logicalPoint{
            static_cast<float>(requireNumber(
                requireValue(request, "x", "Hit test request"),
                "Hit test request.x")),
            static_cast<float>(requireNumber(
                requireValue(request, "y", "Hit test request"),
                "Hit test request.y"))};
        RuntimeValue nodeId;
        if (instance_ != nullptr && generation == generation_) {
            engineState().setScale(renderScale_);
            const sf::Vector2f point = logicalPoint * renderScale_;
            const sf::FloatRect rootClip(
                {0.0f, 0.0f},
                {static_cast<float>(renderSize_.x),
                 static_cast<float>(renderSize_.y)});
            std::vector<UiAssetNodeView> nodes =
                instance_->getNodeViews();
            std::stable_sort(
                nodes.begin(), nodes.end(),
                [](const UiAssetNodeView& left,
                   const UiAssetNodeView& right) {
                    return left.drawOrder > right.drawOrder;
                });
            for (const UiAssetNodeView& node : nodes) {
                if (!effectiveVisible(node.control)) {
                    continue;
                }
                if (!node.bounds.contains(point)) {
                    continue;
                }
                if (!insideEffectiveClip(
                        node.control, point, rootClip, renderScale_)) {
                    continue;
                }
                const sf::Vector2f local =
                    node.control->screenRenderTransform()
                        .getInverse()
                        .transformPoint(point) /
                    renderScale_;
                if (node.control->getLocalBounds().contains(local)) {
                    nodeId = RuntimeValue(node.nodeId);
                    break;
                }
            }
        }
        return RuntimeValue(object({
            {"type", RuntimeValue("hitTest")},
            {"generation", RuntimeValue(generation)},
            {"nodeId", std::move(nodeId)},
        }));
    }

    UiPreviewCurveResolver curveResolver_;
    FrameFiles frameFiles_;
    ActorBatchRenderer actorRenderer_;
    std::shared_ptr<UiAssetInstance> instance_;
    std::int64_t generation_ = 0;
    sf::Vector2u designSize_;
    sf::Vector2u renderSize_;
    float renderScale_ = 1.0f;
    bool accepted_ = false;
};

RuntimeValue errorResponse(const std::string& message) {
    return RuntimeValue(object({
        {"type", RuntimeValue("error")},
        {"message", RuntimeValue(message)},
    }));
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2 || std::string(argv[1]) != "--stdio") {
        std::cerr << "Usage: UiPreviewHost --stdio\n";
        return 2;
    }
    try {
        configureProtocolStreams();
        Host host;
        while (true) {
            const std::optional<std::string> message = readMessage();
            if (!message.has_value()) {
                return 0;
            }
            try {
                writeMessage(host.handle(parseJSONText(*message)));
            } catch (const std::exception& exception) {
                writeMessage(errorResponse(exception.what()));
            }
        }
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
