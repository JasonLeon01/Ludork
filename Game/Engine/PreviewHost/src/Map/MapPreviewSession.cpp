#include "MapPreviewSessionImpl.hpp"

#include "MapPreviewData.hpp"
#include "Protocol/FrameFiles.hpp"
#include "Protocol/PreviewProtocol.hpp"
#include "Rendering/PixelConversion.hpp"
#include "Rendering/MapRenderStatistics.hpp"

#include <EngineState.hpp>
#include <Gameplay/Tilemap/Tilemap.hpp>
#include <Runtime/AssetInputStream.hpp>
#include <Runtime/AssetStore.hpp>
#include <Runtime/Json.hpp>
#include <Runtime/RuntimeDataReader.hpp>
#include <Utf8Path.hpp>
#include <Graphics/Materials/MaterialResource.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <set>
#include <span>
#include <stdexcept>

namespace ludork::preview_host {
namespace {

std::int64_t revision(const RuntimeData::Map& request,
                      const std::string& name) {
    return ludork::runtime::value_reader::requireInteger(
        map_preview_data::required(request, name), "Map request." + name);
}

sf::IntRect textureRect(const RuntimeData::Map& value) {
    const RuntimeData::Array& fields = map_preview_data::array(
        map_preview_data::required(value, "textureRect"));
    if (fields.size() != 4) {
        throw std::invalid_argument(
            "Map sprite textureRect must have four elements");
    }
    int parts[4];
    for (std::size_t index = 0; index < 4; ++index) {
        parts[index] = ludork::runtime::value_reader::requireInt(
            fields[index], "Map textureRect");
    }
    return {{parts[0], parts[1]}, {parts[2], parts[3]}};
}

double milliseconds(std::chrono::steady_clock::time_point begin,
                    std::chrono::steady_clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

RuntimeData::Array changedRecords(const RuntimeData::Array& before,
                                  const RuntimeData::Map& changes,
                                  const std::string& upsertKey,
                                  const std::string& removedKey,
                                  const std::string& orderKey) {
    std::unordered_map<std::string, const RuntimeData*> records;
    for (const RuntimeData& value : before) {
        records.emplace(
            map_preview_data::text(map_preview_data::map(value), "id"), &value);
    }
    for (const RuntimeData& value : map_preview_data::array(
             map_preview_data::required(changes, removedKey))) {
        records.erase(ludork::runtime::value_reader::requireString(
            value, "Removed scene record"));
    }
    for (const RuntimeData& value : map_preview_data::array(
             map_preview_data::required(changes, upsertKey))) {
        records.insert_or_assign(
            map_preview_data::text(map_preview_data::map(value), "id"), &value);
    }
    RuntimeData::Array result;
    for (const RuntimeData& value : map_preview_data::array(
             map_preview_data::required(changes, orderKey))) {
        const std::string& id = ludork::runtime::value_reader::requireString(
            value, "Scene record order");
        const auto found = records.find(id);
        if (found == records.end()) {
            throw std::invalid_argument(
                "Scene record order contains missing or duplicate records");
        }
        result.push_back(*found->second);
        records.erase(found);
    }
    if (!records.empty()) {
        throw std::invalid_argument("Scene record order is incomplete");
    }
    return result;
}

}  // namespace

MapPreviewSession::Impl::~Impl() noexcept {
    try {
        reset();
    } catch (const std::exception& exception) {
        std::cerr << "Map preview shutdown: " << exception.what() << '\n';
    }
}

void MapPreviewSession::Impl::activate() {
    if (context == nullptr) {
        context = std::make_unique<sf::Context>();
    }
    if (!context->setActive(true)) {
        throw std::runtime_error("Failed to activate map preview context");
    }
}

std::shared_ptr<sf::Texture> MapPreviewSession::Impl::texture(
    const std::string& path) {
    const auto found = textures.find(path);
    if (found != textures.end()) {
        return found->second;
    }
    std::shared_ptr<sf::Texture> result = std::make_shared<sf::Texture>();
    if (path.empty()) {
        if (!result->loadFromImage(sf::Image({1, 1}, sf::Color::Transparent))) {
            throw std::runtime_error("Cannot create empty map texture");
        }
    } else {
        std::unique_ptr<ludork::runtime::AssetInputStream> stream =
            ludork::runtime::assetStore().open(path);
        if (!result->loadFromStream(*stream, false)) {
            throw std::runtime_error("Cannot load map texture: " + path);
        }
    }
    result->setSmooth(false);
    textures.emplace(path, result);
    return result;
}

std::uint64_t MapPreviewSession::Impl::spriteId(const std::string& name) {
    const auto [entry, inserted] = spriteIds.try_emplace(name, nextSpriteId);
    if (inserted) {
        ++nextSpriteId;
    }
    return entry->second;
}

std::shared_ptr<TileLayer> MapPreviewSession::Impl::updateLayer(
    const std::string& cacheKey, const std::string& layerName,
    const RuntimeData::Map& layer, int width, int height,
    const RuntimeData::Map& tilesets, const RuntimeData::Map& autoTiles) {
    const std::string tilesetKey =
        map_preview_data::text(layer, "layerTileset");
    Tileset tileSet;
    const RuntimeData* tilesetValue =
        map_preview_data::optional(tilesets, tilesetKey);
    if (tilesetValue != nullptr) {
        tileSet = map_preview_data::tileset(*tilesetValue);
    } else if (!tilesetKey.empty()) {
        throw std::invalid_argument("Map tileset was not supplied: " +
                                    tilesetKey);
    }
    TileGrid tileGrid = map_preview_data::tiles(layer, width, height);
    AutoTileGrid autoGrid = map_preview_data::autoTiles(layer, width, height);
    std::set<std::string> usedKeys;
    for (const auto& row : autoGrid) {
        for (const AutoTileCell& cell : row) {
            if (cell.has_value()) {
                const std::string& key = std::get<std::string>(*cell);
                if (!key.empty()) {
                    usedKeys.insert(key);
                }
            }
        }
    }
    std::vector<std::string> keys(usedKeys.begin(), usedKeys.end());
    std::vector<AutoTile> pool;
    std::vector<std::shared_ptr<sf::Texture>> autoTextures;
    std::vector<int> frameCounts;
    RuntimeData::Map autoSignature;
    for (const std::string& key : keys) {
        const RuntimeData& entry = map_preview_data::required(autoTiles, key);
        AutoTile definition = map_preview_data::autoTile(entry);
        autoTextures.push_back(texture(definition.fileName));
        frameCounts.push_back(
            std::max(1, static_cast<int>(autoTextures.back()->getSize().x) /
                            (AnimatedTileFrameCount * EngineState::CellSize)));
        pool.push_back(std::move(definition));
        autoSignature.emplace(key, entry);
    }
    for (auto& row : autoGrid) {
        for (AutoTileCell& cell : row) {
            if (!cell.has_value()) {
                continue;
            }
            const std::string& key = std::get<std::string>(*cell);
            if (key.empty()) {
                cell.reset();
            } else {
                cell = static_cast<int>(
                    std::lower_bound(keys.begin(), keys.end(), key) -
                    keys.begin());
            }
        }
    }
    const std::string material = map_preview_data::text(layer, "material");
    const std::string signature = stringifyJSON(RuntimeData(object({
        {"width", RuntimeData(static_cast<std::int64_t>(width))},
        {"height", RuntimeData(static_cast<std::int64_t>(height))},
        {"tilesetKey", RuntimeData(tilesetKey)},
        {"tileset", tilesetValue == nullptr ? RuntimeData() : *tilesetValue},
        {"autoTiles", RuntimeData(std::move(autoSignature))},
        {"material", RuntimeData(material)},
        {"materialRevision", RuntimeData(materialRevision)},
    })));
    Layer& cached = layers[cacheKey];
    if (!cached.graphics || cached.signature != signature) {
        TileLayerData data(layerName, tileSet, tileGrid, autoGrid, pool, keys,
                           material);
        data.layerTilesetKey = tilesetKey;
        cached.graphics = std::make_shared<TileLayer>(
            data, texture(tileSet.fileName), autoTextures, frameCounts,
            map_preview_data::boolean(layer, "visible", true));
        cached.signature = signature;
    } else {
        int left = width;
        int top = height;
        int right = 0;
        int bottom = 0;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (cached.tiles[y][x] != tileGrid[y][x] ||
                    cached.autoTiles[y][x] != autoGrid[y][x]) {
                    left = std::min(left, x);
                    top = std::min(top, y);
                    right = std::max(right, x + 1);
                    bottom = std::max(bottom, y + 1);
                }
            }
        }
        if (right > left && bottom > top) {
            TileGrid tileBlock;
            AutoTileGrid autoBlock;
            for (int y = top; y < bottom; ++y) {
                tileBlock.emplace_back(tileGrid[y].begin() + left,
                                       tileGrid[y].begin() + right);
                autoBlock.emplace_back(autoGrid[y].begin() + left,
                                       autoGrid[y].begin() + right);
            }
            cached.graphics->updateBlock(left, top, tileBlock, autoBlock);
        }
        cached.graphics->setVisible(
            map_preview_data::boolean(layer, "visible", true));
    }
    cached.tiles = std::move(tileGrid);
    cached.autoTiles = std::move(autoGrid);
    return cached.graphics;
}

void MapPreviewSession::Impl::updateSprite(std::size_t index,
                                           const RuntimeData::Map& fields) {
    MapRenderer::Sprite sprite;
    sprite.id = spriteId(map_preview_data::text(fields, "id"));
    sprite.layer = map_preview_data::text(fields, "layer");
    sprite.regionId = map_preview_data::text(fields, "regionId");
    const std::string path = map_preview_data::text(fields, "texturePath");
    if (!path.empty()) {
        sprite.texture = texture(path);
    }
    sprite.textureRect = textureRect(fields);
    sf::Transformable transform;
    transform.setPosition(map_preview_data::vector(fields, "position") +
                          map_preview_data::vector(fields, "translation"));
    transform.setScale(map_preview_data::vector(fields, "scale", {1, 1}));
    transform.setOrigin(map_preview_data::vector(fields, "origin"));
    transform.setRotation(
        sf::degrees(map_preview_data::number(fields, "rotation", 0)));
    sprite.transform = transform.getTransform();
    sprite.colour = map_preview_data::colour(fields, "colour");
    sprite.presentationOpacity = map_preview_data::number(fields, "opacity", 1);
    sprite.affectsLighting = map_preview_data::boolean(fields, "visible", true);
    sprite.visible = true;
    sprite.material = map_preview_data::text(fields, "material");
    if (const RuntimeData* parameters =
            map_preview_data::optional(fields, "materialParameters")) {
        sprite.materialParameters = map_preview_data::map(*parameters);
    }
    sprite.materialResource =
        ludork::engine::materials::MaterialResource::load(sprite.material);
    sprite.materialResource->validateParameters(sprite.materialParameters);
    animations.erase(sprite.id);
    if (sprite.texture &&
        map_preview_data::boolean(fields, "animated", false)) {
        const float interval = map_preview_data::number(
            fields, "switchInterval", DefaultAnimationInterval);
        const int frames = map_preview_data::integer(fields, "frameCount", 1);
        if (interval <= 0 || frames <= 0) {
            throw std::invalid_argument(
                "Map sprite animation interval and frame count must be "
                "positive");
        }
        animations.insert_or_assign(
            sprite.id, Animation{index, sprite.textureRect, interval, frames});
    }
    scene.sprites[index] = std::move(sprite);
}

void MapPreviewSession::Impl::updateSprites(const RuntimeData::Array& values) {
    animations.clear();
    scene.sprites.resize(values.size());
    for (std::size_t index = 0; index < values.size(); ++index) {
        updateSprite(index, map_preview_data::map(values[index]));
    }
}

void MapPreviewSession::Impl::updateLight(std::size_t index,
                                          const RuntimeData::Map& fields) {
    MapRenderer::Light light;
    light.position = map_preview_data::vector(fields, "position");
    light.colour = map_preview_data::colour(fields, "colour");
    light.radius =
        map_preview_data::number(fields, "radius", DefaultLightRadius);
    light.intensity = map_preview_data::number(fields, "intensity", 1);
    const std::string owner = map_preview_data::text(fields, "ownerId");
    if (!owner.empty()) {
        light.emitterId = spriteId(owner);
    }
    scene.lights[index] = light;
}

void MapPreviewSession::Impl::updateVisuals(const RuntimeData::Map& changes,
                                            const std::string& name,
                                            bool updateGraphics) {
    const RuntimeData::Array& before =
        map_preview_data::array(map_preview_data::required(sceneData, name));
    const RuntimeData::Map& delta =
        map_preview_data::map(map_preview_data::required(changes, name));
    RuntimeData::Array after =
        changedRecords(before, delta, "upsert", "removed", "order");
    if (!updateGraphics) {
        sceneData.insert_or_assign(name, RuntimeData(std::move(after)));
        return;
    }
    std::unordered_map<std::string, std::size_t> oldIndices;
    for (std::size_t index = 0; index < before.size(); ++index) {
        oldIndices.emplace(
            map_preview_data::text(map_preview_data::map(before[index]), "id"),
            index);
    }
    std::unordered_set<std::string> changed;
    for (const RuntimeData& value :
         map_preview_data::array(map_preview_data::required(delta, "upsert"))) {
        changed.insert(
            map_preview_data::text(map_preview_data::map(value), "id"));
    }
    bool sameOrder = before.size() == after.size();
    for (std::size_t index = 0; sameOrder && index < after.size(); ++index) {
        sameOrder =
            map_preview_data::text(map_preview_data::map(before[index]),
                                   "id") ==
            map_preview_data::text(map_preview_data::map(after[index]), "id");
    }
    if (sameOrder) {
        for (const std::string& id : changed) {
            const std::size_t index = oldIndices.at(id);
            const RuntimeData::Map& fields =
                map_preview_data::map(after[index]);
            if (name == "sprites") {
                updateSprite(index, fields);
            } else {
                updateLight(index, fields);
            }
        }
        sceneData.insert_or_assign(name, RuntimeData(std::move(after)));
        return;
    }
    if (name == "sprites") {
        std::vector<MapRenderer::Sprite> previous = std::move(scene.sprites);
        scene.sprites.resize(after.size());
        std::unordered_set<std::uint64_t> active;
        for (std::size_t index = 0; index < after.size(); ++index) {
            const RuntimeData::Map& fields =
                map_preview_data::map(after[index]);
            const std::string id = map_preview_data::text(fields, "id");
            const auto found = oldIndices.find(id);
            if (found != oldIndices.end() && !changed.contains(id)) {
                scene.sprites[index] = std::move(previous[found->second]);
            } else {
                updateSprite(index, fields);
            }
            const std::uint64_t nativeId = scene.sprites[index].id;
            active.insert(nativeId);
            if (auto animation = animations.find(nativeId);
                animation != animations.end()) {
                animation->second.spriteIndex = index;
            }
        }
        std::erase_if(animations, [&active](const auto& item) {
            return !active.contains(item.first);
        });
    } else {
        std::vector<MapRenderer::Light> previous = std::move(scene.lights);
        scene.lights.resize(after.size());
        for (std::size_t index = 0; index < after.size(); ++index) {
            const RuntimeData::Map& fields =
                map_preview_data::map(after[index]);
            const std::string id = map_preview_data::text(fields, "id");
            const auto found = oldIndices.find(id);
            if (found != oldIndices.end() && !changed.contains(id)) {
                scene.lights[index] = std::move(previous[found->second]);
            } else {
                updateLight(index, fields);
            }
        }
    }
    sceneData.insert_or_assign(name, RuntimeData(std::move(after)));
}

void MapPreviewSession::Impl::updateScene(const RuntimeData::Map& data) {
    hasScene = false;
    const int width = map_preview_data::integer(data, "width", 0);
    const int height = map_preview_data::integer(data, "height", 0);
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Map scene dimensions must be positive");
    }
    scene.cellSize = {static_cast<unsigned int>(width),
                      static_cast<unsigned int>(height)};
    scene.layerOrder.clear();
    for (const RuntimeData& value : map_preview_data::array(
             map_preview_data::required(data, "layerOrder"))) {
        scene.layerOrder.push_back(ludork::runtime::value_reader::requireString(
            value, "Map layer order"));
    }
    const RuntimeData::Map& tilesetData =
        map_preview_data::map(map_preview_data::required(data, "tilesets"));
    const RuntimeData::Map& autoTileData =
        map_preview_data::map(map_preview_data::required(data, "autoTiles"));
    scene.regions.clear();
    std::unordered_set<std::string> activeLayers;
    const bool world = map_preview_data::boolean(data, "world", false);
    for (const RuntimeData& value :
         map_preview_data::array(map_preview_data::required(data, "regions"))) {
        const RuntimeData::Map& fields = map_preview_data::map(value);
        const RuntimeData::Map& mapData =
            map_preview_data::map(map_preview_data::required(fields, "map"));
        MapRenderer::Region region;
        region.id = map_preview_data::text(fields, "id");
        region.cellOrigin = {map_preview_data::integer(fields, "x", 0),
                             map_preview_data::integer(fields, "y", 0)};
        region.priority = map_preview_data::integer(fields, "priority", 0);
        const int mapWidth = map_preview_data::integer(mapData, "width", 0);
        const int mapHeight = map_preview_data::integer(mapData, "height", 0);
        if (mapWidth <= 0 || mapHeight <= 0) {
            throw std::invalid_argument(
                "Map region dimensions must be positive");
        }
        const RuntimeData::Map& mapLayers = map_preview_data::map(
            map_preview_data::required(mapData, "layers"));
        std::vector<std::shared_ptr<TileLayer>> graphics;
        for (const std::string& layerName : scene.layerOrder) {
            const RuntimeData* layer =
                map_preview_data::optional(mapLayers, layerName);
            if (layer == nullptr) {
                continue;
            }
            const std::string cacheKey = region.id + "\x1f" + layerName;
            activeLayers.insert(cacheKey);
            graphics.push_back(
                updateLayer(cacheKey, layerName, map_preview_data::map(*layer),
                            mapWidth, mapHeight, tilesetData, autoTileData));
            region.revision += graphics.back()->getContentRevision();
        }
        region.tilemap = std::make_shared<Tilemap>(graphics);
        if (world) {
            region.fog = map_preview_data::fog(mapData);
            region.ambientLight =
                map_preview_data::colour(mapData, "ambientLight");
        }
        scene.regions.push_back(std::move(region));
    }
    std::erase_if(layers, [&activeLayers](const auto& item) {
        return !activeLayers.contains(item.first);
    });
    updateSprites(
        map_preview_data::array(map_preview_data::required(data, "sprites")));
    const RuntimeData::Array& lights =
        map_preview_data::array(map_preview_data::required(data, "lights"));
    scene.lights.resize(lights.size());
    for (std::size_t index = 0; index < lights.size(); ++index) {
        updateLight(index, map_preview_data::map(lights[index]));
    }
    scene.ambientLight = map_preview_data::colour(data, "ambientLight");
    const std::string panorama = map_preview_data::text(data, "panorama");
    scene.panorama = panorama.empty() ? nullptr : texture(panorama);
    scene.fog = map_preview_data::fog(data);
    hasScene = true;
}

RuntimeData MapPreviewSession::Impl::render(const RuntimeData::Map& request,
                                            FrameFiles& frameFiles) {
    activate();
    const auto applyBegin = std::chrono::steady_clock::now();
    const float particleDelta = ludork::runtime::value_reader::requireFloat(
        map_preview_data::required(request, "particleDelta"),
        "Map request.particleDelta");
    const bool resetParticleMotion = ludork::runtime::value_reader::requireBool(
        map_preview_data::required(request, "resetParticleMotion"),
        "Map request.resetParticleMotion");
    if (particleDelta < 0 || particleDelta > MaximumParticleDeltaSeconds) {
        throw std::invalid_argument(
            "Map particle delta must be between 0 and 0.25 seconds");
    }
    const std::string sessionId = map_preview_data::text(request, "sessionId");
    const std::int64_t generation = revision(request, "generation");
    const std::int64_t nextResourceRevision =
        revision(request, "resourceRevision");
    const std::int64_t nextMaterialRevision =
        revision(request, "materialRevision");
    if (nextMaterialRevision < 0) {
        throw std::invalid_argument(
            "Map material revision must be nonnegative");
    }
    const std::string nextSceneKey =
        map_preview_data::text(request, "sceneKey");
    const RuntimeData* materialValues =
        map_preview_data::optional(request, "materials");
    const bool resourcesChanged = nextResourceRevision != resourceRevision;
    if (materialValues == nullptr &&
        (materials == nullptr || nextMaterialRevision != materialRevision ||
         resourcesChanged)) {
        throw std::invalid_argument(
            "Map material revision requires a complete material snapshot");
    }
    std::unique_ptr<ludork::engine::materials::MaterialResource::Overrides>
        nextMaterials;
    if (materialValues != nullptr) {
        nextMaterials = std::make_unique<
            ludork::engine::materials::MaterialResource::Overrides>(
            map_preview_data::map(*materialValues));
    }
    if (resourcesChanged) {
        reset();
        resourceRevision = nextResourceRevision;
    } else if (sceneKey != nextSceneKey) {
        clearScene();
    }
    sceneKey = nextSceneKey;
    bool rebuild = nextMaterials != nullptr;
    if (nextMaterials != nullptr) {
        materials = std::move(nextMaterials);
        materialRevision = nextMaterialRevision;
        layers.clear();
        if (!resourcesChanged) {
            renderer.reset();
        }
    }
    const ludork::engine::materials::MaterialResource::ScopedOverrides
        materialScope(*materials);
    bool sceneChanged = false;
    if (const RuntimeData* data =
            map_preview_data::optional(request, "scene")) {
        sceneData = map_preview_data::map(*data);
        rebuild = true;
    } else if (const RuntimeData* changes =
                   map_preview_data::optional(request, "sceneChanges")) {
        if (!hasScene) {
            throw std::invalid_argument(
                "Map scene changes require an existing scene");
        }
        const RuntimeData::Map& values = map_preview_data::map(*changes);
        const RuntimeData::Map& fields =
            map_preview_data::map(map_preview_data::required(values, "fields"));
        rebuild = rebuild || !fields.empty();
        for (const auto& [key, value] : fields) {
            if (key == "regions" || key == "sprites" || key == "lights") {
                throw std::invalid_argument(
                    "Map scene records must use record changes");
            }
            sceneData.insert_or_assign(key, value);
        }
        const RuntimeData::Array& regionOrder = map_preview_data::array(
            map_preview_data::required(values, "regionOrder"));
        rebuild = rebuild ||
                  !map_preview_data::array(
                       map_preview_data::required(values, "regions"))
                       .empty() ||
                  !map_preview_data::array(
                       map_preview_data::required(values, "removedRegions"))
                       .empty() ||
                  regionOrder.size() != scene.regions.size();
        for (std::size_t index = 0; !rebuild && index < regionOrder.size();
             ++index) {
            rebuild = ludork::runtime::value_reader::requireString(
                          regionOrder[index], "Map region order") !=
                      scene.regions[index].id;
        }
        if (rebuild) {
            sceneData.insert_or_assign(
                "regions",
                RuntimeData(changedRecords(
                    map_preview_data::array(
                        map_preview_data::required(sceneData, "regions")),
                    values, "regions", "removedRegions", "regionOrder")));
        }
        updateVisuals(values, "sprites", !rebuild);
        updateVisuals(values, "lights", !rebuild);
        sceneChanged = true;
    }
    if (rebuild) {
        updateScene(sceneData);
        sceneChanged = true;
    }
    if (!hasScene) {
        throw std::invalid_argument(
            "Map preview needs a complete scene after opening or restarting");
    }
    if (sceneChanged) {
        emitters.update(sceneData, scene);
    }
    const auto applyEnd = std::chrono::steady_clock::now();
    const int width = map_preview_data::integer(request, "width", 0);
    const int height = map_preview_data::integer(request, "height", 0);
    const int maximum = static_cast<int>(sf::Texture::getMaximumSize());
    if (width <= 0 || height <= 0 || width > maximum || height > maximum) {
        throw std::invalid_argument(
            "Map preview output size exceeds the supported texture dimensions");
    }
    const RuntimeData::Map& viewport =
        map_preview_data::map(map_preview_data::required(request, "view"));
    MapRenderer::View view;
    view.worldRect = {{map_preview_data::number(viewport, "x", 0),
                       map_preview_data::number(viewport, "y", 0)},
                      {map_preview_data::number(viewport, "width", 0),
                       map_preview_data::number(viewport, "height", 0)}};
    view.pixelSize = {static_cast<unsigned int>(width),
                      static_cast<unsigned int>(height)};
    view.rotation =
        sf::degrees(map_preview_data::number(viewport, "rotation", 0));
    MapRenderer::Options options;
    options.time = map_preview_data::number(request, "time", 0);
    if (options.time < 0 || view.worldRect.size.x <= 0 ||
        view.worldRect.size.y <= 0) {
        throw std::invalid_argument(
            "Map preview time and view dimensions are invalid");
    }
    options.lighting = map_preview_data::boolean(request, "lit", false)
                           ? MapRenderer::LightingMode::Lit
                           : MapRenderer::LightingMode::Unlit;
    options.showFog = map_preview_data::boolean(request, "showFog", true);
    options.showPanorama =
        map_preview_data::boolean(request, "showPanorama", true);
    options.activeLayer = map_preview_data::text(request, "selectedLayer");
    options.inactiveLayerOpacity =
        map_preview_data::number(request, "otherLayerOpacity", 1);
    for (const auto& [id, animation] : animations) {
        MapRenderer::Sprite& sprite = scene.sprites[animation.spriteIndex];
        const int frame = static_cast<int>(std::fmod(
            std::floor(options.time / animation.interval), animation.frames));
        sprite.textureRect = animation.baseRect;
        if (sprite.texture && sprite.texture->getSize().x > 0) {
            sprite.textureRect.position.x =
                (animation.baseRect.position.x +
                 frame * animation.baseRect.size.x) %
                static_cast<int>(sprite.texture->getSize().x);
        }
    }
    const auto renderBegin = std::chrono::steady_clock::now();
    emitters.advance(particleDelta, resetParticleMotion);
    const sf::RenderTexture& output =
        renderer.render(scene, view, options, emitters.drawHooks());
    const auto readbackBegin = std::chrono::steady_clock::now();
    const sf::Image image = output.getTexture().copyToImage();
    const auto transferBegin = std::chrono::steady_clock::now();
    const std::span<std::uint8_t> pixels = frameFiles.acquire(
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) *
        preview_pixels::BytesPerPixel);
    bgraFromPremultipliedRgba(image, output.getSize(), pixels);
    const std::filesystem::path& path = frameFiles.path();
    const auto end = std::chrono::steady_clock::now();
    return RuntimeData(object({
        {"type", RuntimeData(FrameResponse)},
        {"sessionId", RuntimeData(sessionId)},
        {"generation", RuntimeData(generation)},
        {"width", RuntimeData(static_cast<std::int64_t>(width))},
        {"height", RuntimeData(static_cast<std::int64_t>(height))},
        {"stride", RuntimeData(static_cast<std::int64_t>(width) *
                               preview_pixels::BytesPerPixel)},
        {"pixelFormat", RuntimeData(preview_pixels::Format)},
        {"previewSceneApplyMs",
         RuntimeData(milliseconds(applyBegin, applyEnd))},
        {"renderMilliseconds",
         RuntimeData(milliseconds(renderBegin, readbackBegin))},
        {"previewReadbackMs",
         RuntimeData(milliseconds(readbackBegin, transferBegin))},
        {"previewTransferMs", RuntimeData(milliseconds(transferBegin, end))},
        {"statistics", mapRenderStatistics(renderer.statistics())},
        {"sharedMemory",
         RuntimeData(object({
             {"filePath", RuntimeData(ludork::standard::pathToUtf8(path))},
             {"offset", RuntimeData(std::int64_t{0})},
         }))},
    }));
}

void MapPreviewSession::Impl::clearScene(bool releaseEmitterResources) {
    emitters.clear(releaseEmitterResources);
    renderer.invalidateScene();
    scene = {};
    sceneData.clear();
    layers.clear();
    spriteIds.clear();
    animations.clear();
    nextSpriteId = 1;
    sceneKey.clear();
    hasScene = false;
}

void MapPreviewSession::Impl::reset() {
    if (context != nullptr) {
        activate();
    }
    clearScene(true);
    renderer.reset();
    textures.clear();
    materials.reset();
    resourceRevision = -1;
    materialRevision = -1;
}

MapPreviewSession::MapPreviewSession() : impl_(std::make_unique<Impl>()) {}
MapPreviewSession::~MapPreviewSession() = default;
RuntimeData MapPreviewSession::render(const RuntimeData::Map& request,
                                      FrameFiles& frameFiles) {
    try {
        return impl_->render(request, frameFiles);
    } catch (...) {
        impl_->clearScene();
        impl_->materials.reset();
        impl_->resourceRevision = -1;
        impl_->materialRevision = -1;
        throw;
    }
}
void MapPreviewSession::reset() {
    impl_->reset();
}

void MapPreviewSession::close(const std::string& sceneKey) {
    if (impl_->sceneKey == sceneKey) {
        impl_->activate();
        impl_->clearScene();
    }
}

}  // namespace ludork::preview_host
