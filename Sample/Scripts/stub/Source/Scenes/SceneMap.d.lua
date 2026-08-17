---@meta Source.Scenes.SceneMap

---@class Source.Scenes.SceneMap.PendingFloorTransfer
---@field targetMap string
---@field anchorPos sf.Vector2i
---@field moveEnabled boolean

---@class Source.Scenes.SceneMap.DialogueLocaleSource
---@field name string
---@field content string | string[]
---@field localeArgs table<string, any>
---@field localVars table<string, any>
---@field instanceVars table<string, any>

---@class Source.Scenes.SceneMap.SceneMap: GlobalCore.SceneBase
---@field new fun(): Source.Scenes.SceneMap.SceneMap
---@field inst Source.GameInstance.GameInstance
---@field player Source.Player.Player
---@field _mapBuilder Source.SceneComponents.SceneMapBuilder
---@field _mapAudio Source.SceneComponents.SceneMapAudioController
---@field _playerHUD Source.Windows.PlayerAttrHUD
---@field _messageWindow Source.Windows.WindowMessage
---@field _windowItem Source.Windows.WindowItem
---@field _windowEquipSlot Source.Windows.WindowEquipSlot
---@field _windowEquipSelect Source.Windows.WindowEquipSelect
---@field _windowEquipStatus Source.Windows.WindowEquipStatus
---@field _windowShop Source.Windows.WindowShop
---@field _windowAttrShop Source.Windows.WindowAttrShop
---@field _windowEnemyBook Source.Windows.WindowEnemyBook
---@field _windowEnemyEncyclopedia Source.Windows.WindowEnemyEncyclopedia
---@field _windowFloorTeleporter Source.Windows.WindowFloorTeleporter
---@field _windowSaveLoad Source.Windows.WindowSaveLoad
---@field _windowMenu Source.Windows.WindowMenu
---@field _configWindow Source.Windows.ConfigWindow
---@field _blockingWindows any[]
---@field _regionTitleUI Source.UI.RegionTitle
---@field _regionTitleText Engine.RichText
---@field _localeChangedToken integer | nil
---@field _dialogueLocaleSource Source.Scenes.SceneMap.DialogueLocaleSource | nil
---@field _gameMap GameMap | nil
---@field _cachedMapFile string | nil
---@field _currentRegion string | nil
---@field _mapClickMoveBlockedUntilLateTick boolean
---@field _mapInputBlockFrames integer
---@field _pendingMenuOpen boolean
---@field _pendingFloorTransfer Source.Scenes.SceneMap.PendingFloorTransfer | nil
---@field _mapTransferInProgress boolean
---@field _shopMoveEnabledBeforeOpen boolean
---@field _attrShopMoveEnabledBeforeOpen boolean
---@field _enemyBookMoveEnabledBeforeOpen boolean
---@field _floorTeleporterMoveEnabledBeforeOpen boolean
local Scene = {}

---@return sf.IntRect, sf.IntRect
function Scene.GetShopRects() end

---@return sf.IntRect
function Scene.GetAttrShopRect() end

---@return sf.IntRect
function Scene.GetEnemyBookRect() end

---@return sf.IntRect
function Scene.GetEnemyEncyclopediaRect() end

--- @brief Start with a transition effect.
function Scene:onEnter() end

--- @brief Set the game instance for this scene.
---
--- - @param inst The GameInstance to use.
---@param inst Source.GameInstance.GameInstance
function Scene:setInst(inst) end

--- @brief Create player HUD, message window, menu, and load the starting map.
function Scene:onCreate() end

--- @brief Stop map BGM/BGS and weather when leaving this scene.
function Scene:onQuit() end

--- @brief Ensure map BGM/BGS are stopped when scene is destroyed.
function Scene:onDestroy() end

--- @brief Forward fixed timestep updates to the game map.
---
--- - @param fixedDelta Fixed timestep in seconds.
---@param fixedDelta number
function Scene:onFixedTick(fixedDelta) end

--- @brief Handle map hotkeys on the window thread.
function Scene:onInput() end

--- @brief Forward per-frame updates to the game map and handle menu open trigger.
---
--- - @param deltaTime Elapsed time in seconds.
---@param deltaTime number
function Scene:onTick(deltaTime) end

--- @brief Forward late-update to the game map.
---
--- - @param deltaTime Elapsed time in seconds.
---@param deltaTime number
function Scene:onLateTick(deltaTime) end

--- @brief Load and generate a game map from data.
---
--- - @param mapPath Path to the map data file.
--- - @return Resolved map data file path.
---@param mapPath string
---@return string
function Scene:loadMap(mapPath) end

---@return GameMap
function Scene:getGameMap() end

--- @brief Show a dialogue message window.
---
--- - @param name Speaker name.
--- - @param message Message text.
--- - @param refActor Optional reference actor for positioning.
--- - @param localeArgs Optional raw locale values inserted after translating the message template.
--- - @return A callable condition function that returns True when dialogue finishes.
---@param name        string
---@param message     string
---@param refActor    Engine.Actor | nil
---@param localeArgs? table<string, any>
---@return function
function Scene:showMessage(name, message, refActor, localeArgs) end

--- @brief Show a selection window with multiple options.
---
--- - @param name Window title.
--- - @param options List of option strings.
--- - @param refActor Optional reference actor for positioning.
--- - @param allowCancel Whether the player can cancel.
--- - @param localeArgs Optional raw locale values inserted after translating each text template.
--- - @return A callable that returns the selected option index, or -1 for cancel.
---@param name         string
---@param options      string[]
---@param refActor     Engine.Actor | nil
---@param allowCancel  boolean
---@param localeArgs?  table<string, any>
---@return function
function Scene:showSelection(name, options, refActor, allowCancel, localeArgs) end

--- @brief Apply a loaded game instance and force-reload the cached map.
---
--- - @param inst The restored game instance from a save file.
---@param inst Source.GameInstance.GameInstance
function Scene:applyLoadedGame(inst) end

--- @brief Show the current-map monster handbook.
function Scene:showEnemyBook() end

--- @brief Show the visited-floor teleporter preview window.
function Scene:showFloorTeleporter() end

--- @brief Request the in-game menu to open on the next render pass.
function Scene:openMenu() end

--- @brief Open the map-bound shop and wait until it closes.
---
--- - @param buyItemIDs Item IDs available for purchase.
--- - @param canSell Whether selling is available.
--- - @return A condition callable that becomes True when the shop closes.
---@param buyItemIDs string[]
---@param canSell    boolean
---@return function
function Scene:openShop(buyItemIDs, canSell) end

--- @brief Open the map-bound attribute shop and wait until it closes.
---
--- - @param actor The actor whose avatar is shown.
--- - @param shopName Locale key for the shop name.
--- - @param shopDescription Locale key for the shop description.
--- - @param abilities Mapping of player attribute names to purchased increments.
--- - @param priceRef Mutable reference containing a shared price or ordered price list.
--- - @param priceIncrement Amount added to the price after each purchase.
--- - @param moneyName Player info component attribute used as currency.
--- - @return A condition callable that becomes True when the shop closes.
---@param actor           Engine.Actor
---@param shopName        string
---@param shopDescription string
---@param abilities       table<string, integer>
---@param priceRef        Source.NodeFunctions.Utils.NodeReference<integer | integer[]>
---@param priceIncrement  integer
---@param moneyName       string
---@return function
function Scene:openAttrShop(actor, shopName, shopDescription, abilities, priceRef, priceIncrement, moneyName) end

---@param targetMap   string
---@param anchorPos   sf.Vector2i
---@param moveEnabled boolean
---@return boolean
function Scene:requestFloorTransfer(targetMap, anchorPos, moveEnabled) end

--- @brief Transition to a map and set the player position.
---
--- - @param mapPath Path to the map data file.
--- - @param pos The position to place the player.
--- - @param blockTransition Whether to skip the map transition effect.
---@param mapPath         string
---@param pos             sf.Vector2i | sf.Vector2u | nil
---@param blockTransition? boolean
function Scene:gotoMapAndPos(mapPath, pos, blockTransition) end

--- @brief Teleport the player to the centre-symmetric tile on the current map when passable.
---
--- - @return True when the teleport succeeds.
---@return boolean
function Scene:tryCenterSymmetricTeleport() end

--- @brief Teleport the player to the same coordinates on an adjacent region floor when passable.
---
--- - @param step Region-list offset; +1 goes upstairs and -1 goes downstairs.
--- - @return True when the teleport succeeds.
---@param step integer
---@return boolean
function Scene:tryAdjacentFloorSamePos(step) end

--- @brief Record an added actor for persistence.
---
--- - @param actor The added actor.
---@param actor Engine.Actor
function Scene:recordAddedActor(actor) end

--- @brief Record an actor position change for persistence.
---
--- - @param actor The moved actor.
---@param actor    Engine.Actor
---@param position sf.Vector2i | nil
function Scene:recordActorPosition(actor, position) end

--- @brief Record a destroyed actor for persistence.
---
--- - @param actor The destroyed actor.
---@param actor Engine.Actor
function Scene:recordDestroyedActor(actor) end

--- @brief Replace the current map BGM.
---
--- - @param bgm Music filename under Assets/Musics.
--- - @param bgmFilter Optional music filter to apply.
---@param bgm       string
---@param bgmFilter Engine.MusicFilter | nil
function Scene:playBgm(bgm, bgmFilter) end

--- @brief Set a filter attribute on the current BGM music.
---
--- - @param attr The filter attribute name.
--- - @param value The filter attribute value.
---@param attr  string
---@param value Source.SceneComponents.MusicFilterValue
function Scene:setBgmFilter(attr, value) end

--- @brief Set a filter attribute on the current BGS music.
---
--- - @param attr The filter attribute name.
--- - @param value The filter attribute value.
---@param attr  string
---@param value Source.SceneComponents.MusicFilterValue
function Scene:setBgsFilter(attr, value) end

---@param mapKey string
---@return string
function Scene:resolveRegionMapPath(mapKey) end

return Scene
