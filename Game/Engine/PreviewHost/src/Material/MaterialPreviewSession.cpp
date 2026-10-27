#include "MaterialPreviewSessionImpl.hpp"

#include "Map/MapPreviewData.hpp"
#include "Protocol/FrameFiles.hpp"
#include "Protocol/PreviewProtocol.hpp"
#include "Rendering/PixelConversion.hpp"
#include "Rendering/MapRenderStatistics.hpp"

#include <EngineState.hpp>
#include <Gameplay/TileLayer.hpp>
#include <Gameplay/Tilemap/Tilemap.hpp>
#include <Graphics/Materials/MaterialCompiler.hpp>
#include <Graphics/Materials/MaterialCompileError.hpp>
#include <Graphics/Materials/MaterialGenerated.hpp>
#include <Graphics/Materials/MaterialResource.hpp>
#include <Runtime/AssetInputStream.hpp>
#include <Runtime/RuntimeDataReader.hpp>
#include <Utf8Path.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <type_traits>

namespace ludork::preview_host {
namespace {

bool sameData(const RuntimeData& left, const RuntimeData& right) {
    return left.visit([&right](const auto& value) -> bool {
        using Value = std::remove_cvref_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, std::monostate>) {
            return right.isNil();
        } else {
            const Value* other = right.getIf<Value>();
            if (other == nullptr) {
                return false;
            }
            if constexpr (std::is_same_v<Value, RuntimeData::Array>) {
                return value.size() == other->size() &&
                       std::equal(value.begin(), value.end(), other->begin(),
                                  sameData);
            } else if constexpr (std::is_same_v<Value, RuntimeData::Map>) {
                if (value.size() != other->size()) {
                    return false;
                }
                for (const auto& [key, item] : value) {
                    const auto found = other->find(key);
                    if (found == other->end() ||
                        !sameData(item, found->second)) {
                        return false;
                    }
                }
                return true;
            } else {
                return value == *other;
            }
        }
    });
}

RuntimeData diagnostic(const std::exception& exception) {
    return RuntimeData(object({{"nodeId", RuntimeData("")},
                               {"portId", RuntimeData("")},
                               {"message", RuntimeData(exception.what())}}));
}

RuntimeData diagnostic(
    const ludork::engine::materials::MaterialCompileError& exception) {
    return RuntimeData(object({{"nodeId", RuntimeData(exception.nodeId())},
                               {"portId", RuntimeData(exception.portId())},
                               {"message", RuntimeData(exception.what())}}));
}

sf::IntRect frameRectangle(const RuntimeData::Map& request, sf::Vector2u size) {
    const RuntimeData* value = map_preview_data::optional(request, "frameRect");
    if (value == nullptr) {
        return {{0, 0}, sf::Vector2i(size)};
    }
    const RuntimeData::Array& values = map_preview_data::array(*value);
    if (values.size() != 4) {
        throw std::invalid_argument(
            "Material frameRect must contain x, y, width, height");
    }
    int fields[4];
    for (std::size_t index = 0; index < 4; ++index) {
        fields[index] = ludork::runtime::value_reader::requireInt(
            values[index], "Material frameRect");
    }
    if (fields[0] < 0 || fields[1] < 0 || fields[2] <= 0 || fields[3] <= 0 ||
        static_cast<std::uint64_t>(fields[0]) + fields[2] > size.x ||
        static_cast<std::uint64_t>(fields[1]) + fields[3] > size.y) {
        throw std::invalid_argument(
            "Material frameRect is outside the preview texture");
    }
    return {{fields[0], fields[1]}, {fields[2], fields[3]}};
}

}  // namespace

std::shared_ptr<sf::Texture> MaterialPreviewSession::Impl::texture(
    const std::string& path) {
    const auto stamp =
        path.empty() ? std::optional<ludork::runtime::AssetStore::AssetStat>{}
                     : ludork::runtime::assetStore().stat(path);
    const bool unchanged =
        !stamp && !sourceStamp ||
        stamp && sourceStamp && stamp->size == sourceStamp->size &&
            stamp->modificationTime == sourceStamp->modificationTime;
    if (source && sourcePath == path && unchanged) {
        return source;
    }
    auto loaded = std::make_shared<sf::Texture>();
    if (path.empty()) {
        sf::Image image({32, 32}, sf::Color::Transparent);
        for (unsigned int y = 0; y < 32; ++y) {
            for (unsigned int x = 0; x < 32; ++x) {
                const float dx = static_cast<float>(x) - 15.5f;
                const float dy = static_cast<float>(y) - 15.5f;
                if (dx * dx + dy * dy > 196.0f) {
                    continue;
                }
                image.setPixel({x, y},
                               sf::Color(static_cast<std::uint8_t>(64 + x * 5),
                                         static_cast<std::uint8_t>(64 + y * 5),
                                         (x / 4 + y / 4) % 2 == 0 ? 220 : 120));
            }
        }
        if (!loaded->loadFromImage(image)) {
            throw std::runtime_error("Cannot create material preview sample");
        }
    } else {
        auto stream = ludork::runtime::assetStore().open(path);
        if (!loaded->loadFromStream(*stream, false)) {
            throw std::runtime_error("Cannot load material preview texture: " +
                                     path);
        }
    }
    loaded->setSmooth(false);
    source = std::move(loaded);
    sourcePath = path;
    sourceStamp = stamp;
    return source;
}

std::shared_ptr<Tilemap> MaterialPreviewSession::Impl::tilemap(
    sf::Vector2u size) {
    if (map && mapSize == size) {
        return map;
    }
    if (!empty) {
        empty = std::make_shared<sf::Texture>();
        if (!empty->loadFromImage(
                sf::Image({32, 32}, sf::Color::Transparent))) {
            throw std::runtime_error("Cannot create material preview canvas");
        }
    }
    const TileLayerData data("preview", Tileset{}, static_cast<int>(size.x),
                             static_cast<int>(size.y));
    map = std::make_shared<Tilemap>(std::vector<std::shared_ptr<TileLayer>>{
        std::make_shared<TileLayer>(data, empty)});
    mapSize = size;
    return map;
}

void MaterialPreviewSession::Impl::prepare(const RuntimeData& asset,
                                           std::string path,
                                           std::int64_t revision) {
    materialRevision = revision;
    materialPath = std::move(path);
    material.reset();
    generatedVertex.clear();
    generatedFragment.clear();
    compileDiagnostics.clear();
    scene = {};
    view = {};
    sceneMaterialRevision = -1;
    try {
        const auto resource =
            ludork::engine::materials::MaterialResource::fromData(asset,
                                                                  materialPath);
        const auto sources =
            ludork::engine::materials::MaterialGenerated::sources(
                *resource, nullptr,
                ludork::engine::materials::MaterialCompiler::Pass::Color);
        generatedVertex = sources.vertex;
        generatedFragment = sources.fragment;
        material = resource;
    } catch (const ludork::engine::materials::MaterialCompileError& exception) {
        compileDiagnostics.push_back(diagnostic(exception));
    } catch (const std::exception& exception) {
        compileDiagnostics.push_back(diagnostic(exception));
    }
}

void MaterialPreviewSession::Impl::updateScene(const RuntimeData::Map& request,
                                               sf::Vector2u size) {
    const RuntimeData* parameterValues =
        map_preview_data::optional(request, "parameters");
    const RuntimeData parameters = parameterValues == nullptr
                                       ? RuntimeData(RuntimeData::Map{})
                                       : *parameterValues;
    const RuntimeData::Map& values = map_preview_data::map(parameters);
    const std::shared_ptr<sf::Texture> nextTexture =
        texture(map_preview_data::text(request, "texturePath"));
    const sf::IntRect rectangle =
        frameRectangle(request, nextTexture->getSize());
    const sf::Color ambient =
        map_preview_data::colour(request, "ambient", sf::Color(90, 90, 90));
    const sf::Color colour =
        map_preview_data::colour(request, "background", sf::Color(24, 28, 36));
    const bool light = map_preview_data::boolean(request, "light", true);
    if (sceneMaterialRevision == materialRevision && view.pixelSize == size &&
        !scene.sprites.empty() &&
        scene.sprites.front().texture == nextTexture &&
        scene.sprites.front().textureRect == rectangle &&
        sameData(sceneParameters, parameters) &&
        scene.ambientLight == ambient && backgroundColour == colour &&
        (!scene.lights.empty()) == light) {
        return;
    }
    material->validateParameters(values);
    if (!background) {
        background = std::make_shared<sf::Texture>();
    }
    if (backgroundColour != colour) {
        if (!background->loadFromImage(sf::Image({1, 1}, colour))) {
            throw std::runtime_error(
                "Cannot create material preview background");
        }
        backgroundColour = colour;
    }
    MapRenderer::Scene next;
    next.cellSize = {
        (size.x + EngineState::CellSize - 1) / EngineState::CellSize,
        (size.y + EngineState::CellSize - 1) / EngineState::CellSize};
    next.layerOrder = {"preview"};
    MapRenderer::Region region;
    region.id = "preview";
    region.tilemap = tilemap(next.cellSize);
    next.regions.push_back(std::move(region));
    next.ambientLight = ambient;
    next.panorama = background;
    MapRenderer::Sprite sprite;
    sprite.id = 1;
    sprite.layer = "preview";
    sprite.regionId = "preview";
    sprite.texture = nextTexture;
    sprite.textureRect = rectangle;
    sprite.material = materialPath;
    sprite.materialParameters = values;
    sprite.materialResource = material;
    const sf::Vector2f offset = material->maximumVertexOffset();
    const sf::Vector2f frameSize(rectangle.size);
    const float scale = std::min(size.x * 0.75f / (frameSize.x + offset.x * 2),
                                 size.y * 0.75f / (frameSize.y + offset.y * 2));
    sf::Transformable transform;
    transform.setPosition(sf::Vector2f(size) * 0.5f);
    transform.setOrigin(frameSize * 0.5f);
    transform.setScale({scale, scale});
    sprite.transform = transform.getTransform();
    next.sprites.push_back(std::move(sprite));
    if (light) {
        MapRenderer::Light point;
        point.position = {size.x * .35f, size.y * .3f};
        point.radius = static_cast<float>(std::max(size.x, size.y));
        point.intensity = 1.f;
        point.emitterId = 1;
        next.lights.push_back(point);
    }
    scene = std::move(next);
    sceneParameters = parameters;
    view.worldRect = {{0, 0}, sf::Vector2f(size)};
    view.pixelSize = size;
    sceneMaterialRevision = materialRevision;
    renderer.invalidateScene();
}

RuntimeData MaterialPreviewSession::Impl::render(
    const RuntimeData::Map& request, FrameFiles& frameFiles) {
    const std::int64_t nextRevision =
        ludork::runtime::value_reader::requireInteger(
            map_preview_data::required(request, "materialRevision"),
            "Material request.materialRevision");
    if (nextRevision < 0) {
        throw std::invalid_argument(
            "Material preview revision must be nonnegative");
    }
    const RuntimeData* asset = map_preview_data::optional(request, "asset");
    if (asset == nullptr && materialRevision != nextRevision) {
        throw std::invalid_argument(
            "Material preview revision requires a complete asset snapshot");
    }
    if (asset != nullptr) {
        const std::string& path = ludork::runtime::value_reader::requireString(
            map_preview_data::required(request, "materialPath"),
            "Material request.materialPath");
        if (materialRevision != nextRevision || materialPath != path) {
            prepare(*asset, path, nextRevision);
        }
    }
    const int width = map_preview_data::integer(request, "width", 640);
    const int height = map_preview_data::integer(request, "height", 300);
    const int limit = static_cast<int>(sf::Texture::getMaximumSize());
    if (width <= 0 || height <= 0 || width > limit || height > limit) {
        throw std::invalid_argument(
            "Material preview dimensions exceed the supported texture size");
    }
    const sf::Vector2u size(static_cast<unsigned int>(width),
                            static_cast<unsigned int>(height));
    RuntimeData::Array nextDiagnostics = compileDiagnostics;
    MapRenderer::Statistics statistics;
    sf::Image image;
    if (material) {
        try {
            updateScene(request, size);
            MapRenderer::Options options;
            options.lighting = MapRenderer::LightingMode::Lit;
            options.time = map_preview_data::number(request, "time", 0);
            options.showFog = false;
            image = renderer.render(scene, view, options)
                        .getTexture()
                        .copyToImage();
            statistics = renderer.statistics();
        } catch (
            const ludork::engine::materials::MaterialCompileError& exception) {
            nextDiagnostics.push_back(diagnostic(exception));
        } catch (const std::exception& exception) {
            nextDiagnostics.push_back(diagnostic(exception));
        }
    }
    if (!nextDiagnostics.empty()) {
        image = sf::Image(size, sf::Color(96, 24, 72));
    }
    const RuntimeData nextDiagnosticData(std::move(nextDiagnostics));
    const bool diagnosticsChanged = !sameData(diagnostics, nextDiagnosticData);
    if (diagnosticsChanged) {
        diagnostics = nextDiagnosticData;
        ++diagnosticsRevision;
    }
    const auto& file = frameFiles.write(bgraFromPremultipliedRgba(image, size));
    const RuntimeData* generation =
        map_preview_data::optional(request, "generation");
    RuntimeData::Map response = object(
        {{"type", RuntimeData(FrameResponse)},
         {"generation",
          generation == nullptr ? RuntimeData(std::int64_t{0}) : *generation},
         {"materialRevision", RuntimeData(materialRevision)},
         {"diagnosticsRevision", RuntimeData(diagnosticsRevision)},
         {"width", RuntimeData(static_cast<std::int64_t>(width))},
         {"height", RuntimeData(static_cast<std::int64_t>(height))},
         {"stride", RuntimeData(static_cast<std::int64_t>(width) *
                                preview_pixels::BytesPerPixel)},
         {"pixelFormat", RuntimeData(preview_pixels::Format)},
         {"sharedMemory",
          RuntimeData(object(
              {{"filePath", RuntimeData(ludork::standard::pathToUtf8(file))},
               {"offset", RuntimeData(std::int64_t{0})}}))},
         {"statistics", mapRenderStatistics(statistics)}});
    if (asset != nullptr || diagnosticsChanged) {
        response.emplace("diagnostics", diagnostics);
    }
    if (asset != nullptr) {
        response.emplace("generatedVertex", RuntimeData(generatedVertex));
        response.emplace("generatedFragment", RuntimeData(generatedFragment));
    }
    return RuntimeData(std::move(response));
}

MaterialPreviewSession::MaterialPreviewSession()
    : impl_(std::make_unique<Impl>()) {}
MaterialPreviewSession::~MaterialPreviewSession() = default;
RuntimeData MaterialPreviewSession::render(const RuntimeData::Map& request,
                                           FrameFiles& frameFiles) {
    return impl_->render(request, frameFiles);
}
void MaterialPreviewSession::reset() {
    impl_ = std::make_unique<Impl>();
}
RuntimeData MaterialPreviewSession::describe() {
    return RuntimeData(object(
        {{"type", RuntimeData(DescriptionResponse)},
         {"registry",
          ludork::engine::materials::MaterialResource::nodeRegistry()},
         {"defaultMaterial",
          ludork::engine::materials::MaterialResource::defaultDocument()}}));
}

}  // namespace ludork::preview_host
