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
#include <Graphics/Materials/MaterialResource.hpp>
#include <Runtime/AssetInputStream.hpp>
#include <Runtime/RuntimeDataReader.hpp>
#include <Utf8Path.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ludork::preview_host {
namespace {

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

RuntimeData MaterialPreviewSession::Impl::render(
    const RuntimeData::Map& request, FrameFiles& frameFiles) {
    const int width = map_preview_data::integer(request, "width", 640);
    const int height = map_preview_data::integer(request, "height", 300);
    const int limit = static_cast<int>(sf::Texture::getMaximumSize());
    if (width <= 0 || height <= 0 || width > limit || height > limit) {
        throw std::invalid_argument(
            "Material preview dimensions exceed the supported texture size");
    }
    const sf::Vector2u size(static_cast<unsigned int>(width),
                            static_cast<unsigned int>(height));
    const RuntimeData* asset = map_preview_data::optional(request, "asset");
    std::string vertex;
    std::string fragment;
    RuntimeData::Array diagnostics;
    MapRenderer::Statistics statistics;
    sf::Image image(size, sf::Color(96, 24, 72));
    try {
        const std::string path =
            map_preview_data::text(request, "materialPath");
        const auto resource =
            asset == nullptr
                ? ludork::engine::materials::MaterialResource::load(path)
                : ludork::engine::materials::MaterialResource::fromData(*asset,
                                                                        path);
        const RuntimeData* parameterValues =
            map_preview_data::optional(request, "parameters");
        const RuntimeData::Map parameters =
            parameterValues == nullptr
                ? RuntimeData::Map{}
                : map_preview_data::map(*parameterValues);
        resource->validateParameters(parameters);
        const auto sources =
            ludork::engine::materials::MaterialCompiler::compile(*resource);
        vertex = sources.vertex;
        fragment = sources.fragment;
        MapRenderer::Scene scene;
        scene.cellSize = {
            static_cast<unsigned int>((width + EngineState::CellSize - 1) /
                                      EngineState::CellSize),
            static_cast<unsigned int>((height + EngineState::CellSize - 1) /
                                      EngineState::CellSize)};
        scene.layerOrder = {"preview"};
        MapRenderer::Region region;
        region.id = "preview";
        region.tilemap = tilemap(scene.cellSize);
        scene.regions.push_back(std::move(region));
        scene.ambientLight =
            map_preview_data::colour(request, "ambient", sf::Color(90, 90, 90));
        if (!background) {
            background = std::make_shared<sf::Texture>();
        }
        if (!background->loadFromImage(sf::Image(
                {1, 1}, map_preview_data::colour(request, "background",
                                                 sf::Color(24, 28, 36))))) {
            throw std::runtime_error(
                "Cannot create material preview background");
        }
        scene.panorama = background;
        MapRenderer::Sprite sprite;
        sprite.id = 1;
        sprite.layer = "preview";
        sprite.regionId = "preview";
        sprite.texture =
            texture(map_preview_data::text(request, "texturePath"));
        sprite.textureRect = frameRectangle(request, sprite.texture->getSize());
        sprite.material = path;
        sprite.materialParameters = parameters;
        sprite.materialResource = resource;
        const sf::Vector2f offset = resource->maximumVertexOffset();
        const sf::Vector2f frameSize(sprite.textureRect.size);
        const float scale =
            std::min(width * 0.75f / (frameSize.x + offset.x * 2),
                     height * 0.75f / (frameSize.y + offset.y * 2));
        sf::Transformable transform;
        transform.setPosition(sf::Vector2f(size) * 0.5f);
        transform.setOrigin(frameSize * 0.5f);
        transform.setScale({scale, scale});
        sprite.transform = transform.getTransform();
        scene.sprites.push_back(std::move(sprite));
        if (map_preview_data::boolean(request, "light", true)) {
            MapRenderer::Light light;
            light.position = {width * .35f, height * .3f};
            light.radius = static_cast<float>(std::max(width, height));
            light.intensity = 1.f;
            light.emitterId = 1;
            scene.lights.push_back(light);
        }
        MapRenderer::View view;
        view.worldRect = {{0, 0}, sf::Vector2f(size)};
        view.pixelSize = size;
        MapRenderer::Options options;
        options.lighting = MapRenderer::LightingMode::Lit;
        options.time = map_preview_data::number(request, "time", 0);
        options.showFog = false;
        image =
            renderer.render(scene, view, options).getTexture().copyToImage();
        statistics = renderer.statistics();
    } catch (const ludork::engine::materials::MaterialCompileError& exception) {
        diagnostics.emplace_back(
            object({{"nodeId", RuntimeData(exception.nodeId())},
                    {"portId", RuntimeData(exception.portId())},
                    {"message", RuntimeData(exception.what())}}));
    } catch (const std::exception& exception) {
        diagnostics.emplace_back(
            object({{"nodeId", RuntimeData("")},
                    {"portId", RuntimeData("")},
                    {"message", RuntimeData(exception.what())}}));
    }
    const auto& file = frameFiles.write(bgraFromPremultipliedRgba(image, size));
    const RuntimeData* generation =
        map_preview_data::optional(request, "generation");
    return RuntimeData(object(
        {{"type", RuntimeData("materialFrame")},
         {"generation",
          generation == nullptr ? RuntimeData(std::int64_t{0}) : *generation},
         {"width", RuntimeData(static_cast<std::int64_t>(width))},
         {"height", RuntimeData(static_cast<std::int64_t>(height))},
         {"stride", RuntimeData(static_cast<std::int64_t>(width) * 4)},
         {"pixelFormat", RuntimeData("Bgra8888Premultiplied")},
         {"sharedMemory",
          RuntimeData(object(
              {{"filePath", RuntimeData(ludork::standard::pathToUtf8(file))},
               {"offset", RuntimeData(std::int64_t{0})}}))},
         {"diagnostics", RuntimeData(std::move(diagnostics))},
         {"statistics", mapRenderStatistics(statistics)},
         {"generatedVertex", RuntimeData(vertex)},
         {"generatedFragment", RuntimeData(fragment)}}));
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
    return RuntimeData(
        object({{"type", RuntimeData("materialDescription")},
                {"registry",
                 ludork::engine::materials::MaterialResource::nodeRegistry()},
                {"defaultMaterial",
                 ludork::engine::materials::MaterialResource::defaultMaterial()
                     ->data()}}));
}

}  // namespace ludork::preview_host
