---@meta Source.NodeFunctions.Scene

--- @brief Transition to a map and optionally place the player at a tile coordinate.
---
--- - @param mapPath Path to the map data file (relative to Data/Maps/).
--- - @param blockTransition Whether to skip the map transition effect.
--- - @param position Target tile coordinate as an `sf.Vector2i`, or `nil` to keep the current position.
---@param mapPath         string
---@param blockTransition boolean
---@param position        sf.Vector2i | nil
function Scene.GotoMap(mapPath, blockTransition, position) end

--- @brief Switch to the game over scene.
function Scene.GameOver() end

--- @brief Add a timer on the current scene.
---
--- - @param interval Time in seconds before the timer fires.
--- - @param blocking Whether scene input should be blocked until the timer fires.
--- - @return A condition callable that becomes True when the timer fires.
---@param interval number
---@param blocking boolean
---@return function
function Scene.AddTimer(interval, blocking) end

--- @brief Show a dialogue message on the current map scene by actor tag.
---@param name        string
---@param message     string
---@param refActorTag string
---@return function
function Scene.ShowMessageByTag(name, message, refActorTag) end

--- @brief Show a dialogue message on the current map scene positioned by a direct actor reference.
---@param name    string
---@param message string
---@param actor   Engine.Actor
---@return function
function Scene.ShowMessage(name, message, actor) end

--- @brief Play a non-spatial voice clip and show a dialogue message on the current map scene by actor tag.
---
--- The returned latent condition stops the current Voice before reporting that the dialogue finished.
---@param name          string
---@param message       string
---@param voiceFileName string
---@param refActorTag   string
---@return function
function Scene.ShowVoiceMessageByTag(name, message, voiceFileName, refActorTag) end

--- @brief Play a spatial voice clip relative to an actor and show a dialogue message on the current map scene.
---
--- The returned latent condition stops the current Voice before reporting that the dialogue finished.
---@param name          string
---@param message       string
---@param voiceFileName string
---@param refActor      Engine.Actor
---@param minDistance   number
---@return function
function Scene.ShowVoiceMessage(name, message, voiceFileName, refActor, minDistance) end

--- @brief Show a selection window on the current map scene.
---@param name        string
---@param options     string[]
---@param refActorTag string
---@param allowCancel boolean
---@return function
function Scene.ShowSelection(name, options, refActorTag, allowCancel) end

--- @brief Show a selection window on the current map scene positioned by a direct actor reference.
---@param name        string
---@param options     string[]
---@param refActor    Engine.Actor | nil
---@param allowCancel boolean
---@return function
function Scene.ShowRefSelection(name, options, refActor, allowCancel) end

--- @brief Lock the current map camera to the player.
function Scene.LockCamera() end

--- @brief Unlock the current map camera from the player.
function Scene.UnlockCamera() end

--- @brief Attach the current map camera to an actor, or detach it when nil is passed.
---
--- - @param actor The actor for the camera to follow, or nil to stop following.
---@param actor Engine.Actor | nil
function Scene.AttachCamera(actor) end

--- @brief Move the current map camera by a delta offset and clamp it to the map bounds.
---
--- - @param delta The `sf.Vector2f` viewport offset to apply.
---@param delta sf.Vector2f
function Scene.MoveCamera(delta) end

--- @brief Record a telepoint in the current game instance.
---
--- - @param mapPath The map path where the telepoint is located.
--- - @param x Telepoint tile column.
--- - @param y Telepoint tile row.
---@param mapPath string
---@param x       integer
---@param y       integer
function Scene.RecordTelepoint(mapPath, x, y) end

--- @brief Create an actor from a blueprint path and spawn it on the current map.
---
--- - @param bpPath The blueprint class path, such as Data.Blueprints.Enemies.BP_Enemy_redKing.
--- - @param layerName The layer name to place the created actor on.
--- - @param position Optional `sf.Vector2i` tile coordinate for the created actor.
--- - @param tag Optional tag string for the created actor.
--- - @param emitCreateEvent Whether to run the actor's onCreate blueprint event.
--- - @return The created actor instance, or nil when the actor cannot be created.
---@param bpPath          string
---@param layerName       string
---@param position        sf.Vector2i | nil
---@param tag             string
---@param emitCreateEvent boolean
---@return Engine.Actor | nil
function Scene.CreateActorFromBPPath(bpPath, layerName, position, tag, emitCreateEvent) end

--- @brief Create an actor from a blueprint path with per-instance class default overrides.
---
--- - @param bpPath The blueprint class path, such as Data.Blueprints.Enemies.BP_Enemy_redKing.
--- - @param defaults Class attribute defaults to override on the created actor instance.
--- - @param layerName The layer name to place the created actor on.
--- - @param position Optional `sf.Vector2i` tile coordinate for the created actor.
--- - @param tag Optional tag string for the created actor.
--- - @param emitCreateEvent Whether to run the actor's onCreate blueprint event.
--- - @return The created actor instance, or nil when the actor cannot be created.
---@param bpPath          string
---@param defaults        table<string, Source.Data.ClassVarValue> | nil
---@param layerName       string
---@param position        sf.Vector2i | nil
---@param tag             string
---@param emitCreateEvent boolean
---@return Engine.Actor | nil
function Scene.CreateActorFromBPPathWithDefaults(bpPath, defaults, layerName, position, tag, emitCreateEvent) end

--- @brief Replace and persist one terrain tile on the current map.
---
--- - @param layerName The tile layer to edit.
--- - @param position The `sf.Vector2i` tile coordinate.
--- - @param tileID The replacement tile ID, autotile key, or nil.
---@param layerName string
---@param position  sf.Vector2i
---@param tileID    integer | string | nil
function Scene.DestroyTerrain(layerName, position, tileID) end

--- @brief Replace and persist multiple terrain tiles on the current map.
---
--- - @param layerName The tile layer to edit.
--- - @param positions The `sf.Vector2i` tile coordinates.
--- - @param tileID The replacement tile ID, autotile key, or nil.
---@param layerName string
---@param positions sf.Vector2i[]
---@param tileID    integer | string | nil
function Scene.DestroyTerrainList(layerName, positions, tileID) end

--- @brief Get the terrain tile ID on the current map.
---
--- - @param layerName The tile layer to query.
--- - @param position The `sf.Vector2i` tile coordinate.
--- - @return The static tile ID, autotile key, or nil.
---@param layerName string
---@param position  sf.Vector2i
---@return integer | string | nil
function Scene.GetTerrainTile(layerName, position) end

--- @brief Get all current-map coordinates that match a tile ID on one layer.
---
--- - @param layerName The tile layer to query.
--- - @param tileID The static tile ID, autotile key, or nil.
--- - @return A list of matching tile coordinates.
---@param layerName string
---@param tileID    integer | string | nil
---@return sf.Vector2i[]
function Scene.GetTerrainTilePositions(layerName, tileID) end

--- @brief Record an added actor for persistence on the current map scene.
---
--- - @param actor The added actor.
---@param actor Engine.Actor
function Scene.RecordAddedActor(actor) end

--- @brief Record the blueprint owner as an added actor for persistence on the current map scene.
function Scene.SelfRecordAdded() end

--- @brief Record an actor position change for persistence on the current map scene.
---
--- - @param actor The moved actor.
---@param actor Engine.Actor
function Scene.RecordActorPosition(actor) end

--- @brief Record the blueprint owner's position change for persistence on the current map scene.
function Scene.SelfRecordActorPosition() end

--- @brief Record a destroyed actor for persistence on the current map scene.
---
--- - @param actor The destroyed actor.
---@param actor Engine.Actor
function Scene.RecordDestroyedActor(actor) end

--- @brief Record the blueprint owner as a destroyed actor for persistence on the current map scene.
function Scene.SelfRecordDestroyed() end

--- @brief Record a destroyed actor for persistence and destroy it on the current map scene.
---
--- - @param actor The actor to record and destroy.
---@param actor Engine.Actor
function Scene.RecordAndDestroyActor(actor) end

--- @brief Record the blueprint owner as destroyed for persistence and destroy it on the current map scene.
function Scene.SelfRecordAndDestroy() end

--- @brief Open the map-bound shop.
---
--- - @param items Item IDs available for purchase.
--- - @param canSell Whether selling is available.
--- - @return A condition callable that becomes True when the shop closes.
---@param items   string[]
---@param canSell boolean
---@return function
function Scene.OpenShop(items, canSell) end

--- @brief Open an attribute shop on the current map scene.
---
--- - @param actor The actor whose avatar is shown.
--- - @param shopName Locale key for the shop name.
--- - @param shopDescription Locale key for the shop description.
--- - @param abilities Mapping of player attribute names to purchased increments.
--- - @param price Shared price, ordered price list, or game variable name containing either.
--- - @param priceIncrement Amount added to the price after each purchase.
--- - @param moneyName Player info component attribute used as currency.
--- - @return A condition callable that becomes True when the shop closes.
---@param actor           Engine.Actor | nil
---@param shopName        string
---@param shopDescription string
---@param abilities       table<string, integer>
---@param price           integer | integer[]
---@param priceIncrement  integer
---@param moneyName       string
---@return function
function Scene.OpenAttrShop(actor, shopName, shopDescription, abilities, price, priceIncrement, moneyName) end

--- @brief Open an attribute shop on the current map scene.
---
--- - @param actorTag Tag of the actor whose avatar is shown.
--- - @param shopName Locale key for the shop name.
--- - @param shopDescription Locale key for the shop description.
--- - @param abilities Mapping of player attribute names to purchased increments.
--- - @param price Shared price, ordered price list, or game variable name containing either.
--- - @param priceIncrement Amount added to the price after each purchase.
--- - @param moneyName Player info component attribute used as currency.
--- - @return A condition callable that becomes True when the shop closes.
---@param actorTag        string
---@param shopName        string
---@param shopDescription string
---@param abilities       table<string, integer>
---@param price           integer | integer[]
---@param priceIncrement  integer
---@param moneyName       string
---@return function
function Scene.OpenAttrShopByTag(actorTag, shopName, shopDescription, abilities, price, priceIncrement, moneyName) end

return Scene
