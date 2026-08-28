---@meta Global.GameMap

---@alias Global.GameMap.TerrainTileID integer | string | nil
---@alias Global.GameMap.StaticTransmissionElement boolean | tuple<string | integer>
---@alias Global.GameMap.StaticTransmissionSignature tuple<Global.GameMap.StaticTransmissionElement>

---@brief Game map managing tile layers, actors, lights, collisions, and pathfinding.
---
--- Provides the core gameplay map with lighting, occlusion, actor management,
--- and pathfinding support. Integrates with Camera and SceneBase.
---@class GameMapLayerMaskTextureCacheEntry
---@field [1] sf.Image
---@field [2] sf.Image
---@field [3] sf.Image
---@field [4] sf.Texture
---@field [5] sf.Texture
---@field [6] sf.Texture

---@class GameMapCoverLayerState
---@field layer   Engine.TileLayer
---@field visible boolean

---@class Global.GameMap.SparseWorldConfig
---@field size                      sf.Vector2u
---@field layerOrder                string[]
---@field tilePassabilityQuery      fun(position: sf.Vector2i): boolean
---@field directionPassabilityQuery fun(fromPosition: sf.Vector2i, toPosition: sf.Vector2i, direction: integer): boolean
---@field topMaterialQuery          fun(position: sf.Vector2i): Engine.Material | nil

---@class Global.GameMap.WorldTileMaskConfig
---@field targetSize     sf.Vector2f
---@field viewSize       sf.Vector2f
---@field viewPosition   sf.Vector2f
---@field viewRotation   number
---@field regionSize     sf.Vector2f
---@field regionPosition sf.Vector2f

---@class Global.GameMap.ActiveLight
---@field light GlobalCore.Light
---@field owner Engine.Actor | nil

---@class Global.GameMap.LightCacheEntry
---@field [1] number
---@field [2] number
---@field [3] number
---@field [4] number
---@field [5] number
---@field [6] number
---@field [7] number

---@class (partial) GameMap: GlobalCore.GameMapBase
---@field DefaultCoverAlpha                  integer
---@field MapViewRect                        sf.IntRect                                                                                                                                                      Logical-screen rectangle occupied by the map canvas; defaults to `(192, 32, 416, 416)`.
---@field _tilemap                           Engine.Tilemap
---@field _camera                            GlobalCore.Camera | nil
---@field _mapViewRect                       sf.IntRect
---@field _components                        ComponentBase[]
---@field _actors                            table<string, Engine.Actor[]>
---@field _player                            Engine.Actor | nil
---@field _scene                             GlobalCore.SceneBase | nil
---@field _particleSystem                    Engine.ParticleSystem | nil
---@field _actorPixelShatterShader           sf.Shader | nil
---@field _actorPixelShatterEffects          table<string, Global.CustomEffects.ActorPixelShatterEffect[]>
---@field _actorPixelShatterByActor          table<Engine.Actor, Global.CustomEffects.ActorPixelShatterEffect>
---@field _actorPixelShatterSeed             integer
---@field _staticTransmission                sf.RenderTexture | nil
---@field _dynamicTransmission               sf.RenderTexture | nil
---@field _directLight                       sf.RenderTexture | nil
---@field _directLightCleared                boolean
---@field _staticDirectLight                 sf.RenderTexture | nil
---@field _surfaceMask                       sf.RenderTexture | nil
---@field _lightPassQuad                     sf.RectangleShape | nil
---@field _unobstructedLightVertices         sf.VertexArray | nil
---@field _unobstructedLightVertex           sf.Vertex | nil
---@field _surfaceTileRenderStates           sf.RenderStates | nil
---@field _surfaceActorRenderStates          sf.RenderStates | nil
---@field _transmissionTileRenderStates      sf.RenderStates | nil
---@field _transmissionActorRenderStates     sf.RenderStates | nil
---@field _lightPassRenderStates             sf.RenderStates | nil
---@field _unobstructedLightPassRenderStates sf.RenderStates | nil
---@field _autoTileResolver                  fun(autoTileName: string): Engine.AutoTile | nil
---@field _damageTextConfig                  Engine.PlainTextConfig | nil
---@field _actorShaderBuffer                 sf.RenderTexture | nil
---@field _actorHueBuffer                    sf.RenderTexture | nil
---@field _cachedActiveLights                Global.GameMap.LightCacheEntry[] | nil
---@field _unobstructedLightCache            Global.GameMap.LightCacheEntry[] | nil
---@field _cachedLightMaterialRevision       integer
---@field _cachedLightTransmissionSignature  Global.GameMap.StaticTransmissionSignature | nil
---@field _staticTransmissionRevision        integer
---@field _staticTransmissionSignature       Global.GameMap.StaticTransmissionSignature | nil
---@field _staticHasTransmissionLoss         boolean
---@field _staticTextureOrigin               sf.Vector2f
---@field _staticTextureSize                 sf.Vector2f
---@field _staticOccupancyOrigin             sf.Vector2f
---@field _staticOccupancySize               sf.Vector2f
---@field _materialDirty                     boolean
---@field _materialRevision                  integer
---@field _tilePassableGrid                  boolean[][] | nil
---@field _staticOccupancyPrefix             number[][] | nil
---@field _layerMaskTextureCache             table<string, GameMapLayerMaskTextureCacheEntry>
---@field _coverLayerStates                  GameMapCoverLayerState[] | nil
---@field mapName                            string
---@field new                                fun(mapName: string, tilemap: Engine.Tilemap, camera?: GlobalCore.Camera, previewOnly?: boolean, sparseWorldConfig?: Global.GameMap.SparseWorldConfig): GameMap
local GameMap = {}

---@brief Construct a game map.
---
--- - @param mapName The name of the map.
--- - @param tilemap The tilemap data.
--- - @param camera Optional camera; a default one is created if not provided.
--- - @param previewOnly Whether to skip resources only needed by the live map.
--- - @param sparseWorldConfig Optional sparse-world native query configuration.
---@param mapName           string
---@param tilemap           Engine.Tilemap
---@param camera            GlobalCore.Camera | nil
---@param previewOnly       boolean | nil
---@param sparseWorldConfig Global.GameMap.SparseWorldConfig | nil
function GameMap:init(mapName, tilemap, camera, previewOnly, sparseWorldConfig) end

---@return boolean
function GameMap:isWorldMap() end

---@param deltaTime number
function GameMap:updateAutoTileAnimation(deltaTime) end

function GameMap:disposeStreaming() end

function GameMap:drawMapFogOverlay() end

---@param component ComponentBase
function GameMap:addComponent(component) end

---@param resolver fun(autoTileName: string): Engine.AutoTile | nil
function GameMap:setAutoTileResolver(resolver) end

---@param curve Engine.Curve
function GameMap:setDamageTextSpeedCurve(curve) end

---@param config Engine.PlainTextConfig
function GameMap:setDamageTextConfig(config) end

---@brief Get the player actor.
---
--- - @return The player actor, or nil.
---@return Engine.Actor | nil
function GameMap:getPlayer() end

---@brief Set the player actor and parent the camera to it.
---
--- - @param player The player actor to set, or nil.
---@param player Engine.Actor | nil
function GameMap:setPlayer(player) end

---@brief Get all actors across all layers.
---
--- - @return A flat list of all actors.
---@return Engine.Actor[]
function GameMap:getAllActors() end

---@brief Get the layer that directly contains an actor.
---
--- - @param actor Actor to look up.
--- - @return Layer name, or nil when the actor is not on this map.
---@param actor Engine.Actor
---@return string | nil
function GameMap:getActorLayer(actor) end

---@brief Get actors at a specific map position.
---
--- - @param position The map position to query.
--- - @return A list of actors at the given position.
---@param position sf.Vector2i
---@return Engine.Actor[]
function GameMap:getActorsByPosition(position) end

---@brief Get the first actor on a given layer at a position.
---
--- - @param layer The layer name.
--- - @param position The map position.
--- - @return The matching actor, or nil.
---@param layer    string
---@param position sf.Vector2i
---@return Engine.Actor | nil
function GameMap:getActorByLayerAndPosition(layer, position) end

---@brief Get actors within a range of a position.
---
--- - @param position The centre position.
--- - @param radius The search radius in tiles.
--- - @return A list of actors within the range.
---@param position sf.Vector2i
---@param radius   integer
---@return Engine.Actor[]
function GameMap:getActorsByRange(position, radius) end

---@brief Get the actor with a given map-placement tag.
---
--- - @param tag The map-placement tag to search for.
--- - @return The matching actor, or nil.
---@param tag string
---@return Engine.Actor | nil
function GameMap:getActorByTag(tag) end

---@brief Get all actors with a given map-placement tag.
---
--- - @param tag The map-placement tag to search for.
--- - @return A list of matching actors.
---@param tag string
---@return Engine.Actor[]
function GameMap:getAllActorsByTag(tag) end

---@brief Remove actors matching any map-placement tag without replaying destroy events.
---
--- - @param tags Map-placement tags to remove.
---@param tags string[]
function GameMap:removeActorsByTags(tags) end

---@brief Apply persisted actor position changes to the current map.
---
--- - @param actorPositions Map-placement-tag-indexed tile positions.
---@param actorPositions table<string, sf.Vector2i>
function GameMap:applyActorPositions(actorPositions) end

---@brief Check if an actor can move to a target position.
---
--- Considers tile passability, direction, and actor collision.
---
--- - @param actor The moving actor.
--- - @param targetPosition The target map position.
--- - @return True if the position is passable.
---@param actor          Engine.Actor
---@param targetPosition sf.Vector2i
---@return boolean
function GameMap:isPassable(actor, targetPosition) end

---@brief Spawn an actor on a layer.
---
--- - @param actor The actor to spawn.
--- - @param layer The layer name to place the actor on.
--- - @param emitCreateEvent Whether to run the actor's onCreate blueprint event.
---@param actor            Engine.Actor
---@param layer            string
---@param emitCreateEvent? boolean
function GameMap:spawnActor(actor, layer, emitCreateEvent) end

function GameMap:beginActorBatch() end

function GameMap:endActorBatch() end

---@brief Create an actor instance from a class and spawn it on a layer.
---
--- - @param actorClass The actor class to instantiate.
--- - @param layer The layer name to place the created actor on.
--- - @param kwargs Optional keyword arguments passed to the actor constructor.
--- - @param emitCreateEvent Whether to run the actor's onCreate blueprint event.
--- - @return The created actor instance.
---@param actorClass      Class.ClassType<Engine.Actor>
---@param layer           string
---@param kwargs          table<string, any> | nil
---@param emitCreateEvent boolean
---@return Engine.Actor
function GameMap:createActor(actorClass, layer, kwargs, emitCreateEvent) end

---@brief Initialise pending actor create events and actor components recursively.
function GameMap:initialiseActorsAndComponents() end

---@brief Queue an actor for destruction on the next tick.
---
--- - @param actor The actor to destroy.
---@param actor Engine.Actor
function GameMap:destroyActor(actor) end

---@brief Queue a visual-only world-pixel shatter effect for an Actor.
---
--- The Actor remains responsible for its normal destroy lifecycle. This method
--- only retains its final visual until the transient effect is prepared on the
--- render thread. It returns false when shaders are unavailable, the Actor is
--- already destroyed or queued, or the Actor is not on a visible live-map layer.
---@param actor Engine.Actor
---@return boolean
function GameMap:playActorPixelShatterEffect(actor) end

---@brief Get the camera attached to this map.
---
--- - @return The Camera, or nil.
---@return GlobalCore.Camera | nil
function GameMap:getCamera() end

---@brief Set the camera for this map.
---
--- - @param camera The camera to set.
---@param camera GlobalCore.Camera
function GameMap:setCamera(camera) end

---@brief Get the tilemap.
---
--- - @return The Tilemap.
---@return Engine.Tilemap
function GameMap:getTilemap() end

---@brief Get the terrain tile ID at a layer position.
---
--- - @param layerName The tile layer to query.
--- - @param position The sf.Vector2i tile coordinate.
--- - @return The static tile ID, autotile key, or nil.
---@param layerName string
---@param position  sf.Vector2i
---@return Global.GameMap.TerrainTileID
function GameMap:getTerrainTile(layerName, position) end

---@brief Get all coordinates matching a terrain tile ID on one layer.
---
--- - @param layerName The tile layer to query.
--- - @param tileID The static tile ID, autotile key, or nil to find empty cells.
--- - @return A list of matching tile coordinates.
---@param layerName string
---@param tileID    Global.GameMap.TerrainTileID
---@return sf.Vector2i[]
function GameMap:getTerrainTilePositions(layerName, tileID) end

---@brief Replace one terrain tile on the current map.
---
--- - @param layerName The tile layer to edit.
--- - @param position The sf.Vector2i tile coordinate.
--- - @param tileID The replacement tile ID, autotile key, or nil to clear the tile.
--- - @return True if the tile was replaced.
---@param layerName string
---@param position  sf.Vector2i
---@param tileID    Global.GameMap.TerrainTileID
---@return boolean
function GameMap:setTerrainTile(layerName, position, tileID) end

---@brief Replace multiple terrain tiles on the current map.
---
--- - @param layerName The tile layer to edit.
--- - @param positions The sf.Vector2i tile coordinates.
--- - @param tileID The replacement tile ID, autotile key, or nil to clear the tiles.
--- - @return The successfully changed positions.
---@param layerName string
---@param positions sf.Vector2i[]
---@param tileID    Global.GameMap.TerrainTileID
---@return sf.Vector2i[]
function GameMap:setTerrainTiles(layerName, positions, tileID) end

---@brief Apply persisted terrain replacements to the current map.
---
--- - @param terrainDestructions Layer-indexed terrain replacement records.
---@param terrainDestructions table<string, table<string, { position: sf.Vector2i, tileID: Global.GameMap.TerrainTileID }>>
function GameMap:applyTerrainDestructions(terrainDestructions) end

---@brief Get all lights on the map.
---
--- - @return A list of Light objects.
---@return GlobalCore.Light[]
function GameMap:getLights() end

---@brief Replace all lights on the map.
---
--- - @param lights The new list of Light objects.
---@param lights GlobalCore.Light[]
function GameMap:setLights(lights) end

---@brief Add a light to the map.
---
--- - @param light The Light to add.
---@param light GlobalCore.Light
function GameMap:addLight(light) end

---@brief Remove a light from the map.
---
--- - @param light The Light to remove.
---@param light GlobalCore.Light
function GameMap:removeLight(light) end

---@brief Set the position of a light.
---
--- - @param light The light to modify.
--- - @param position The new position as an sf.Vector2f.
---@param light    GlobalCore.Light
---@param position sf.Vector2f
function GameMap:setLightPosition(light, position) end

---@brief Set the colour of a light.
---
--- - @param light The light to modify.
--- - @param colour The new colour.
---@param light  GlobalCore.Light
---@param colour sf.Color
function GameMap:setLightColour(light, colour) end

---@brief Set the radius of a light.
---
--- - @param light The light to modify.
--- - @param radius The new radius.
---@param light  GlobalCore.Light
---@param radius number
function GameMap:setLightRadius(light, radius) end

---@brief Set the intensity of a light.
---
--- - @param light The light to modify.
--- - @param intensity The new intensity.
---@param light     GlobalCore.Light
---@param intensity number
function GameMap:setLightIntensity(light, intensity) end

---@brief Get the ambient light colour.
---
--- - @return The ambient light Colour.
---@return sf.Color
function GameMap:getAmbientLight() end

---@brief Set the ambient light colour.
---
--- - @param ambientLight The new ambient light Colour.
---@param ambientLight sf.Color
function GameMap:setAmbientLight(ambientLight) end

---@brief Get the map size in tiles.
---
--- - @return The map size.
---@return sf.Vector2u
function GameMap:getSize() end

---@return Global.WorldGeometry.CellRect
function GameMap:_getGameplayCellRect() end

---@brief Get the logical-screen rectangle occupied by the map canvas.
---
--- - @return The immutable map canvas rectangle for this GameMap instance.
---@return sf.IntRect
function GameMap:getMapViewRect() end

---@brief Get the topmost visible material at a position.
---
--- - @param pos The map position.
--- - @return The top Material, or nil.
---@param pos sf.Vector2i
---@return Engine.Material | nil
function GameMap:getTopMaterial(pos) end

---@brief Find a path from start to goal using pathfinding.
---
--- - @param start The starting position.
--- - @param goal The goal position.
--- - @param actor The moving actor used for multi-cell footprint checks.
--- - @param excludedAnchors Optional actor-anchor cells that A* must not enter.
--- - @return Path data containing offsets, points, and route.
---@param start            sf.Vector2i
---@param goal             sf.Vector2i
---@param actor            Engine.Actor
---@param excludedAnchors? sf.Vector2i[]
---@return GlobalCore.PathResult
function GameMap:findPathResult(start, goal, actor, excludedAnchors) end

---@brief Find a path from start to goal using pathfinding.
---
--- - @param start The starting position.
--- - @param goal The goal position.
--- - @param actor The moving actor used for multi-cell footprint checks.
--- - @param excludedAnchors Optional actor-anchor cells that A* must not enter.
--- - @return A list of per-step movement offsets.
---@param start            sf.Vector2i
---@param goal             sf.Vector2i
---@param actor            Engine.Actor
---@param excludedAnchors? sf.Vector2i[]
---@return sf.Vector2i[]
function GameMap:findPath(start, goal, actor, excludedAnchors) end

---@brief Check if a target position is passable for automatic pathfinding.
---
--- Automatic pathfinding treats non-colliding actors with an implemented
--- `onOverlap` event as blockers so routes do not step onto interactive
--- triggers by accident. Actors whose `onOverlap` has no executable content
--- do not block.
---
--- - @param actor The moving actor.
--- - @param targetPosition The target map position.
--- - @return True if automatic pathfinding may route through the position.
---@param actor          Engine.Actor
---@param targetPosition sf.Vector2i
---@return boolean
function GameMap:isPathfindingPassable(actor, targetPosition) end

---@brief Check whether a cell contains an overlap-event actor that blocks auto pathfinding.
---
--- - @param actor The moving actor.
--- - @param targetPosition The target map position.
--- - @return True if the position has a non-colliding actor with implemented `onOverlap`.
---@param actor          Engine.Actor
---@param targetPosition sf.Vector2i
---@return boolean
function GameMap:hasPathBlockingOverlapActor(actor, targetPosition) end

---@brief Get the scene this map belongs to.
---
--- - @return The parent SceneBase.
---@return GlobalCore.SceneBase | nil
function GameMap:getScene() end

---@brief Set the scene this map belongs to.
---
--- - @param scene The parent scene.
---@param scene GlobalCore.SceneBase
function GameMap:setScene(scene) end

---@brief Display a floating tip text in the parent scene.
---
--- - @param text The tip message to display.
---@param text string
function GameMap:addCommonTip(text) end

---@brief Display floating damage text in the map particle system.
---
--- - @param text Damage text content.
--- - @param position World position used as the spawn point.
---@param text     string
---@param position sf.Vector2f
function GameMap:addDamageText(text, position) end

---@brief Convert a world position for drawing while the map view is active.
---
--- - @param position World position to convert.
--- - @return Position relative to the current camera view.
---@param position sf.Vector2f
---@return sf.Vector2f
function GameMap:worldToMapViewPosition(position) end

---@brief Convert a world position to logical UI-screen coordinates.
---
--- - @param position World position to convert.
--- - @return Logical UI position aligned with the map canvas rectangle.
---@param position sf.Vector2f
---@return sf.Vector2f
function GameMap:worldToUIScreenPosition(position) end

---@brief Convert a world position for raw drawing in the default canvas view.
---
--- - @param position World position to convert.
--- - @return Scaled canvas position aligned with the default view.
---@param position sf.Vector2f
---@return sf.Vector2f
function GameMap:worldToCanvasPosition(position) end

---@brief Get all actors colliding with a given actor at a target position.
---
--- - @param actor The querying actor.
--- - @param targetPosition The map position to check.
--- - @return Colliding actors on the topmost occupied layer, ordered from visually topmost to bottommost.
---@param actor          Engine.Actor
---@param targetPosition sf.Vector2i
---@return Engine.Actor[]
function GameMap:getCollision(actor, targetPosition) end

---@brief Get all actors overlapping with the given actor.
---
--- - @param actor The querying actor.
--- - @return A list of overlapping actors.
---@param actor Engine.Actor
---@return Engine.Actor[]
function GameMap:getOverlaps(actor) end

---@brief Rebuild the flat actor list from all layers, including children.
function GameMap:updateActorList() end

---@brief Get a 2D map of material property values.
---
--- - @param functionName The material property function name.
--- - @param invalidValue The default value for invalid positions.
--- - @return A 2D grid of property values.
---@param functionName string
---@param invalidValue number | boolean
---@return (number | boolean)[][]
function GameMap:getMaterialPropertyMap(functionName, invalidValue) end

---@brief Get a light blocking map for a specific actor layer.
---
--- - @param layerName The layer name.
--- - @param size The map size.
--- - @return A 2D grid of light block values, or nil.
---@param layerName string
---@param size      sf.Vector2u
---@return number[][] | nil
function GameMap:getActorLayerLightBlockMap(layerName, size) end

---@brief Update all actors, components, and particles each frame.
---
--- - @param deltaTime Elapsed time in seconds.
---@param deltaTime number
function GameMap:onTick(deltaTime) end

---@brief Late-update all actors, components, and particles each frame.
---
--- - @param deltaTime Elapsed time in seconds.
---@param deltaTime number
function GameMap:onLateTick(deltaTime) end

---@brief Fixed-timestep update for all actors and components.
---
--- - @param fixedDelta Fixed timestep in seconds.
---@param fixedDelta number
function GameMap:onFixedTick(fixedDelta) end

---@brief Draw visible tile layers and actors to a render target.
---
--- - @param target Render target receiving the map content.
--- - @param states Optional render states for normal map draws.
--- - @param applyPlayerCover Whether to apply the player cover transparency pass.
---@param target            sf.RenderTarget
---@param states            sf.RenderStates | nil
---@param applyPlayerCover? boolean
function GameMap:drawMapContent(target, states, applyPlayerCover) end

---@brief Render the full map including layers, actors, lights, and particles.
function GameMap:show() end

---@brief Refresh the material shader uniforms with current lighting data.
function GameMap:refreshShader() end

---@brief Mark the passability cache as dirty for rebuild on next query.
function GameMap:markPassabilityDirty() end

---@brief Refresh one actor's occupancy without rebuilding tile passability.
---
--- - @param actor The actor whose cached occupancy should be refreshed.
---@param actor Engine.Actor
function GameMap:updateActorOccupancy(actor) end

return GameMap
