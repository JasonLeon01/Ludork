local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local Item = require("Enums.GeneralData.Item")
local WindowTransitionProfile = require("Enums.WindowTransitionProfile")
local Logging = require("Global.Utils.Logging")
local LazyWindow = require("Internal.UIBase.LazyWindow")
local GameSystem = require("Source.System")
local GameInstance = require("Source.GameInstance")
local Save = require("Source.Save")
local WindowSaveSlot = require("Source.Windows.WindowSaveLoad.Slot")
local SaveLoadMode = require("Enums.SaveLoadMode")
local SaveLoadCloseReason = require("Enums.SaveLoadCloseReason")

local Direction = Engine.FocusDirection
local FocusGroup = GlobalCore.FocusGroup
local FocusNeighbor = GlobalCore.FocusNeighbor
local FocusTransition = GlobalCore.FocusTransition
local AudioManager = GlobalCore.AudioManager
local Graphics = GlobalCore.Graphics

local MENU_Z_ORDER = 1
local MAP_INPUT_BLOCK_FRAMES = 2
local WINDOW_CLOSE_INPUT_BLOCK_FRAMES = 1

---@class Source.Scenes.SceneMap.Windows
local Windows = {
    _FOCUS_GROUP_NAME = {
        MENU = "menu",
        ITEM = "item",
        EQUIP_SLOT = "equip-slot",
        EQUIP_SELECT = "equip-select",
        SHOP_ITEM = "shop-item",
        SHOP_RECORDS = "shop-records",
        FLOOR_COMMAND = "floor-command",
        FLOOR_PREVIEW = "floor-preview",
        SAVE_SLOT = "save-slot",
        PLAYER_NAME = "player-name"
    },
    _SAVE_PREVIEW_MAX_WIDTH = 224,
    _SAVE_PREVIEW_MAX_HEIGHT = 168
}

---@param manager GlobalCore.UIManager
---@return table<string, GlobalCore.FocusGroup>
local function createFocusGroups(manager)
    local groups = {}
    for _, name in ipairs({
        Windows._FOCUS_GROUP_NAME.MENU,
        Windows._FOCUS_GROUP_NAME.ITEM,
        Windows._FOCUS_GROUP_NAME.EQUIP_SLOT,
        Windows._FOCUS_GROUP_NAME.EQUIP_SELECT,
        Windows._FOCUS_GROUP_NAME.SHOP_ITEM,
        Windows._FOCUS_GROUP_NAME.SHOP_RECORDS,
        Windows._FOCUS_GROUP_NAME.FLOOR_COMMAND,
        Windows._FOCUS_GROUP_NAME.FLOOR_PREVIEW,
        Windows._FOCUS_GROUP_NAME.SAVE_SLOT,
        Windows._FOCUS_GROUP_NAME.PLAYER_NAME
    }) do
        local group = FocusGroup.new(name)
        groups[name] = group
        manager:registerFocusGroup(group)
    end
    groups.item:setNeighbor(Direction.LEFT, groups.menu)
    groups[Windows._FOCUS_GROUP_NAME.EQUIP_SLOT]:setNeighbor(Direction.LEFT, groups.menu)
    groups[Windows._FOCUS_GROUP_NAME.EQUIP_SLOT]:setNeighbor(
        Direction.RIGHT, FocusNeighbor.new(groups[Windows._FOCUS_GROUP_NAME.EQUIP_SELECT], FocusTransition.EXPLICIT)
    )
    groups[Windows._FOCUS_GROUP_NAME.EQUIP_SELECT]:setNeighbor(
        Direction.LEFT, FocusNeighbor.new(groups[Windows._FOCUS_GROUP_NAME.EQUIP_SLOT], FocusTransition.EXPLICIT)
    )
    groups[Windows._FOCUS_GROUP_NAME.FLOOR_COMMAND]:setNeighbor(
        Direction.RIGHT, FocusNeighbor.new(groups[Windows._FOCUS_GROUP_NAME.FLOOR_PREVIEW], FocusTransition.EXPLICIT)
    )
    groups[Windows._FOCUS_GROUP_NAME.FLOOR_PREVIEW]:setNeighbor(
        Direction.LEFT, FocusNeighbor.new(groups[Windows._FOCUS_GROUP_NAME.FLOOR_COMMAND], FocusTransition.EXPLICIT)
    )
    groups[Windows._FOCUS_GROUP_NAME.SAVE_SLOT]:setNeighbor(Direction.LEFT, groups.menu)
    return groups
end

---@param group   GlobalCore.FocusGroup
---@param control Engine.FunctionalBase
local function bindFocusControl(group, control)
    group:addItem(control)
    group.activeOwner = control
end

---@param player  Source.MapActors.Player.Player
---@param enabled boolean
---@return Source.Scenes.SceneMap.Windows.Movement
local function createMovement(player, enabled)
    return { player = player, enabled = enabled }
end

---@param self     Source.Scenes.SceneMap.Windows
---@param movement Source.Scenes.SceneMap.Windows.Movement | nil
---@param frames   integer
local function restoreMovement(self, movement, frames)
    if movement == nil or movement.player ~= self._scene:getPlayer() or movement.player:isDestroyed() then
        return
    end
    movement.player:setMoveEnabled(movement.enabled)
    self._scene:blockMapInput(frames)
end

---@param player  Source.MapActors.Player.Player
---@param enabled boolean
---@return Source.Scenes.SceneMap.Windows.Session
local function createSession(player, enabled)
    return { player = player, enabled = enabled, operation = Engine.AsyncOperation.new() }
end

---@param self    Source.Scenes.SceneMap.Windows
---@param session Source.Scenes.SceneMap.Windows.Session | nil
---@param handle  Internal.UIBase.LazyWindow<Source.Scenes.SceneMap.Windows.AsyncWindow>
local function cancelSession(self, session, handle)
    if session == nil then
        return
    end
    session.operation:cancel()
    local window = handle:peek()
    if window ~= nil then
        window:dismiss()
    end
    restoreMovement(self, session, WINDOW_CLOSE_INPUT_BLOCK_FRAMES)
end

---@param self  Source.Scenes.SceneMap.Windows
---@param actor Engine.Actor | nil
local function recordShopVisit(self, actor)
    if actor == nil or actor:getMap() ~= self._scene:getGameMap() then
        return
    end
    local tag = actor:getMapTag()
    if bool(tag) then
        self._scene:getGameInstance():recordShop(assert(self._scene:getGameInstance():getCurrentMapPath()), tag)
    end
end

---@param self Source.Scenes.SceneMap.Windows
local function releaseRemoteShop(self)
    local actor = self._remoteShopActor
    self._remoteShopActor = nil
    if actor ~= nil then
        actor:cancelAsyncOperations()
        actor:setGraph(nil)
    end
end

local function captureScreenSnapshot()
    local clock = sf.Clock.new()
    local canvas = Graphics.getCanvas()
    local sourceTexture = canvas:getTexture()
    local sourceSize = sourceTexture:getSize()
    if sourceSize.x == 0 or sourceSize.y == 0 then
        GameSystem.SetSavedScreenImage(nil)
        return
    end
    local scale = math.min(
        Windows._SAVE_PREVIEW_MAX_WIDTH / sourceSize.x, Windows._SAVE_PREVIEW_MAX_HEIGHT / sourceSize.y, 1
    )
    local previewSize = sf.Vector2u.new(
        math.max(1, math.floor(sourceSize.x * scale)), math.max(1, math.floor(sourceSize.y * scale))
    )
    ---@cast previewSize sf.Vector2u
    local scaled = sf.RenderTexture.new(previewSize)
    scaled:clear(sf.Color.Black)
    local sprite = sf.Sprite.new(sourceTexture)
    sprite:setScale(sf.Vector2f.new(previewSize.x / sourceSize.x, previewSize.y / sourceSize.y))
    scaled:draw(sprite)
    scaled:display()
    GameSystem.SetSavedScreenImage(scaled:getTexture():copyToImage())
    Logging.info("Save screenshot capture: %.2f ms", clock:getElapsedTime():asMicroseconds() / 1000)
end

function Windows:init(scene)
    self._scene = scene
    self._pendingMenuOpen = false
    self._pendingSaveLoadOpen = nil
    self._pendingQuickSave = false
    self._shopSession = nil
    self._attrShopSession = nil
    self._playerNameSession = nil
    self._itemMovement = nil
    self._equipMovement = nil
    self._saveLoadMovement = nil
    self._enemyBookMovement = nil
    self._floorTeleporterMovement = nil
    self._shopRecordsMovement = nil
    self._remoteShopActor = nil
    local manager = assert(scene:getUIManager(), "Scene map UI manager is unavailable")
    local groups = createFocusGroups(manager)
    self._windowPlayerName = LazyWindow.new(function ()
        local WindowPlayerName = require("Source.Windows.WindowPlayerName")

        local window = WindowPlayerName.new(self._scene:getPlayer(), function ()
            self:onPlayerNameClose()
        end)
        for _, control in ipairs(window:getFocusControls()) do
            groups[Windows._FOCUS_GROUP_NAME.PLAYER_NAME]:addItem(control)
        end
        groups[Windows._FOCUS_GROUP_NAME.PLAYER_NAME].activeOwner = window
        window:mount(manager)
        return window
    end)
    self._windowItem = LazyWindow.new(function ()
        local WindowItem = require("Source.Windows.WindowItem")

        local window = WindowItem.new(self._scene:getPlayer())
        window:setZOrder(MENU_Z_ORDER)
        window:setOnCloseCallback(function ()
            self:onItemClose()
        end)
        window:setOnUseCallback(function ()
            self:onHotkeyItemUsed()
        end)
        bindFocusControl(groups.item, window)
        window:mount(manager)
        return window
    end)
    self._windowEquip = LazyWindow.new(function ()
        local WindowEquip = require("Source.Windows.WindowEquip")

        local window = WindowEquip.new(self._scene:getPlayer())
        window:setZOrder(MENU_Z_ORDER)
        window:setOnCloseCallback(function ()
            self:onEquipClose()
        end)
        local slot, selection = window:getFocusControls()
        bindFocusControl(groups[Windows._FOCUS_GROUP_NAME.EQUIP_SLOT], slot)
        bindFocusControl(groups[Windows._FOCUS_GROUP_NAME.EQUIP_SELECT], selection)
        window:mount(manager)
        return window
    end)
    self._windowShop = LazyWindow.new(function ()
        local WindowShop = require("Source.Windows.WindowShop")

        local window = WindowShop.new(self._scene:getPlayer(), function ()
            self:onShopClose()
        end)
        bindFocusControl(groups[Windows._FOCUS_GROUP_NAME.SHOP_ITEM], window:getItemWindow())
        window:mount(manager)
        return window
    end)
    self._windowAttrShop = LazyWindow.new(function ()
        local WindowAttrShop = require("Source.Windows.WindowAttrShop")

        local window = WindowAttrShop.new(self._scene:getPlayer(), function ()
            self:onAttrShopClose()
        end)
        window:mount(manager)
        return window
    end)
    self._windowEnemyBook = LazyWindow.new(function ()
        local WindowEnemyBook = require("Source.Windows.WindowEnemyBook")

        local window = WindowEnemyBook.new(
            self._scene:getPlayer(),
            function ()
                self:onEnemyBookClose()
            end,
            function (entry)
                self:onEnemyBookConfirm(entry)
            end
        )
        window:mount(manager)
        return window
    end)
    self._windowEnemyEncyclopedia = LazyWindow.new(function ()
        local WindowEnemyEncyclopedia = require("Source.Windows.WindowEnemyEncyclopedia")

        local window = WindowEnemyEncyclopedia.new(function ()
            self:onEnemyEncyclopediaClose()
        end)
        window:mount(manager)
        return window
    end)
    self._windowFloorTeleporter = LazyWindow.new(function ()
        local WindowFloorTeleporter = require("Source.Windows.WindowFloorTeleporter")

        local window = WindowFloorTeleporter.new(
            self._scene:getGameInstance(),
            function (mapKey, telepoint, previewSize, previewScale, showTelepointMarker)
                return self._scene:buildFloorMapPreview(
                    mapKey, telepoint, previewSize, previewScale, showTelepointMarker
                )
            end,
            function (mapKey, telepoint)
                self:onFloorTeleporterConfirm(mapKey, telepoint)
            end,
            function ()
                self:onFloorTeleporterClose()
            end,
            function (mapKey)
                return self._scene:getFloorMapPreviewRevision(mapKey)
            end,
            function ()
                self._scene:clearFloorMapPreviewCache()
            end
        )
        bindFocusControl(groups[Windows._FOCUS_GROUP_NAME.FLOOR_COMMAND], window:getCommandWindow())
        bindFocusControl(groups[Windows._FOCUS_GROUP_NAME.FLOOR_PREVIEW], window:getPreviewWindow())
        window:mount(manager)
        return window
    end)
    self._windowShopRecords = LazyWindow.new(function ()
        local WindowShopRecords = require("Source.Windows.WindowShopRecords")

        local window = WindowShopRecords.new(function (mapPath, tag)
            self:onShopRecordConfirm(mapPath, tag)
        end,
            function ()
                self:onShopRecordsClose()
            end)
        bindFocusControl(groups[Windows._FOCUS_GROUP_NAME.SHOP_RECORDS], window)
        window:mount(manager)
        return window
    end)
    self._windowSaveLoad = LazyWindow.new(function ()
        local constructionClock = sf.Clock.new()
        local WindowSaveLoad = require("Source.Windows.WindowSaveLoad")

        local window = WindowSaveLoad.new(
            false,
            function ()
                return self._scene:getGameInstance()
            end,
            function (reason)
                self:onSaveLoadClose(reason)
            end,
            function (inst)
                self._scene:applyLoadedGame(inst)
            end
        )
        window:setZOrder(MENU_Z_ORDER)
        bindFocusControl(groups[Windows._FOCUS_GROUP_NAME.SAVE_SLOT], window:getSlotWindow())
        window:mount(manager)
        Logging.info("Save window construction: %.2f ms", constructionClock:getElapsedTime():asMicroseconds() / 1000)
        return window
    end)
    self._configWindow = LazyWindow.new(function ()
        local ConfigWindow = require("Source.Windows.ConfigWindow")

        local window = ConfigWindow.new(function ()
            self:onConfigClose()
        end)
        window:setZOrder(MENU_Z_ORDER)
        window:mount(manager)
        return window
    end)
    self._windowMenu = LazyWindow.new(function ()
        local WindowMenu = require("Source.Windows.WindowMenu")

        local window = WindowMenu.new(
            self._scene:getPlayer(),
            {
                item = self._windowItem,
                equip = self._windowEquip,
                saveLoad = self._windowSaveLoad,
                config = self._configWindow
            },
            function ()
                local SceneTitle = require("Source.Scenes.SceneTitle")

                GlobalCore.SceneManager.setScene(SceneTitle.new())
            end
        )
        window:setZOrder(MENU_Z_ORDER)
        window:setMoveRestoreGuard(function ()
            return not self:hasVisibleBlockingWindow()
        end)
        bindFocusControl(groups.menu, window)
        window:mount(manager)
        return window
    end)
    self._blockingWindows = {
        self._windowShop, self._windowAttrShop, self._windowEnemyBook, self._windowEnemyEncyclopedia,
        self._windowFloorTeleporter, self._windowPlayerName, self._windowItem, self._windowEquip, self._windowSaveLoad,
        self._configWindow, self._windowShopRecords
    }
end

function Windows:canOpenMenu()
    return not self._scene:isTutorialBlocking() and not self._pendingMenuOpen and self._pendingSaveLoadOpen == nil
        and not self._pendingQuickSave and not self:isBlocking() and not self._scene:isDialogueBlocking()
end

function Windows:canOpenItemOverlay()
    local menu = self._windowMenu:peek()
    local item = self._windowItem:peek()
    return menu ~= nil and menu:getVisible()
        and item ~= nil and item:getVisible()
        and not self._scene:isDialogueBlocking()
end

function Windows:isMenuBlocking()
    local menu = self._windowMenu:peek()
    return menu ~= nil and menu:isBlocking()
end

function Windows:hasVisibleBlockingWindow()
    for _, handle in ipairs(self._blockingWindows) do
        local window = handle:peek()
        if window ~= nil and window:getVisible() then
            return true
        end
    end
    return false
end

function Windows:isBlocking()
    return self:isMenuBlocking() or self:hasVisibleBlockingWindow()
end

function Windows:openMenu()
    if not self:canOpenMenu() then
        return false
    end
    self._pendingMenuOpen = true
    return true
end

function Windows:openSaveLoadUI(mode)
    if not self:canOpenMenu() then
        return false
    end
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self._pendingSaveLoadOpen = mode
    return true
end

function Windows:openSaveUI()
    return self:openSaveLoadUI(SaveLoadMode.SAVE)
end

function Windows:openLoadUI()
    return self:openSaveLoadUI(SaveLoadMode.LOAD)
end

function Windows:openItemUI()
    if not self:canOpenMenu() then
        return false
    end
    AudioManager.playSound(GameSystem.GetDecisionSE())
    local player = self._scene:getPlayer()
    self._itemMovement = createMovement(player, true)
    player:setMoveEnabled(false)
    self._windowItem:get():open(WindowTransitionProfile.DEFAULT)
    self._scene:blockMapInput(MAP_INPUT_BLOCK_FRAMES)
    return true
end

function Windows:openEquipUI()
    if not self:canOpenMenu() then
        return false
    end
    AudioManager.playSound(GameSystem.GetDecisionSE())
    local player = self._scene:getPlayer()
    self._equipMovement = createMovement(player, true)
    player:setMoveEnabled(false)
    self._windowEquip:get():open(WindowTransitionProfile.DEFAULT)
    self._scene:blockMapInput(MAP_INPUT_BLOCK_FRAMES)
    return true
end

function Windows:quickSave()
    if not self:canOpenMenu() then
        return false
    end
    self._pendingQuickSave = true
    return true
end

function Windows:quickLoad()
    if not self:canOpenMenu() then
        return false
    end
    local slot = Save.FindLatestSlot(WindowSaveSlot.MAX_SAVE_SLOTS)
    if slot == nil then
        AudioManager.playSound(GameSystem.GetBuzzerSE())
        return true
    end
    local instance = Save.LoadGame(Save.GetSavePath(slot))
    if instance == nil then
        AudioManager.playSound(GameSystem.GetBuzzerSE())
        return true
    end
    AudioManager.playSound(GameSystem.GetLoadSE())
    self._scene:applyLoadedGame(instance)
    return true
end

function Windows:afterRender()
    if self._pendingMenuOpen then
        self._pendingMenuOpen = false
        captureScreenSnapshot()
        self._windowMenu:get():open()
        return
    end
    if self._pendingSaveLoadOpen == nil and not self._pendingQuickSave then
        return
    end
    captureScreenSnapshot()
    if self._pendingSaveLoadOpen ~= nil then
        local pending = { mode = self._pendingSaveLoadOpen }
        self._pendingSaveLoadOpen = nil
        local player = self._scene:getPlayer()
        self._saveLoadMovement = createMovement(player, player:getMoveEnabled())
        player:setMoveEnabled(false)
        self._windowSaveLoad:get():open(WindowTransitionProfile.DEFAULT, pending.mode)
        self._scene:blockMapInput(MAP_INPUT_BLOCK_FRAMES)
    end
    if self._pendingQuickSave then
        self._pendingQuickSave = false
        local instance = self._scene:getGameInstance()
        local slot = Save.FindNextEmptySlot(WindowSaveSlot.MAX_SAVE_SLOTS)
        Save.SaveSlot(slot, instance, GameSystem.GetSavedScreenImage())
        AudioManager.playSound(GameSystem.GetSaveSE())
        self._scene:showMessage("", "{QUICK_SAVE}", nil, { slot = slot })
    end
end

function Windows:showEnemyBook()
    if (not self:canOpenMenu() and not self:canOpenItemOverlay()) or not self._scene:getPlayer():hasItem(Item.EnemyBook) then
        return false
    end
    local window = self._windowEnemyBook:get()
    if not window:getVisible() then
        local player = self._scene:getPlayer()
        self._enemyBookMovement = createMovement(player, self:isMenuBlocking() or player:getMoveEnabled())
        player:setMoveEnabled(false)
    end
    window:open(self._scene:getGameMap())
    self._scene:blockMapInput(MAP_INPUT_BLOCK_FRAMES)
    return true
end

function Windows:showFloorTeleporter()
    if (not self:canOpenMenu() and not self:canOpenItemOverlay()) or not self._scene:getPlayer():hasItem(Item.Teleport) then
        return false
    end
    local window = self._windowFloorTeleporter:get()
    local wasVisible = window:getVisible()
    local player = self._scene:getPlayer()
    local enabled = self:isMenuBlocking() or player:getMoveEnabled()
    if not window:open(self._scene:getGameInstance()) then
        return false
    end
    if not wasVisible then
        self._floorTeleporterMovement = createMovement(player, enabled)
        player:setMoveEnabled(false)
    end
    self._scene:blockMapInput(MAP_INPUT_BLOCK_FRAMES)
    return true
end

function Windows:showShopRecords()
    if not GameInstance.EnableShopRecord or self._remoteShopActor ~= nil
        or (not self:canOpenMenu() and not self:canOpenItemOverlay())
        or not self._scene:getPlayer():hasItem(Item.Teleport) then
        return false
    end
    local shops = self._scene:getGameInstance():getShops()
    if #shops == 0 then
        return false
    end
    local window = self._windowShopRecords:get()
    if not window:open(shops, self._scene:getGameInstance():getCurrentMapPath()) then
        return false
    end
    local player = self._scene:getPlayer()
    self._shopRecordsMovement = createMovement(player, self:isMenuBlocking() or player:getMoveEnabled())
    player:setMoveEnabled(false)
    self._scene:blockMapInput(MAP_INPUT_BLOCK_FRAMES)
    return true
end

function Windows:onShopRecordsClose()
    restoreMovement(self, self._shopRecordsMovement, WINDOW_CLOSE_INPUT_BLOCK_FRAMES)
    self._shopRecordsMovement = nil
end

function Windows:onShopRecordConfirm(mapPath, tag)
    if not GameInstance.EnableShopRecord or self._remoteShopActor ~= nil
        or not self._scene:getPlayer():hasItem(Item.Teleport) then
        AudioManager.playSound(GameSystem.GetBuzzerSE())
        return
    end
    local found = false
    for _, record in ipairs(self._scene:getGameInstance():getShops()) do
        if record[1] == mapPath and record[2] == tag then
            found = true
            break
        end
    end
    if not found then
        AudioManager.playSound(GameSystem.GetBuzzerSE())
        return
    end
    local ShopRecords = require("Source.Utils.ShopRecords")

    local actor = ShopRecords.CreateActor(mapPath, tag)
    if actor == nil then
        AudioManager.playSound(GameSystem.GetBuzzerSE())
        return
    end
    self._remoteShopActor = actor
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self._windowShopRecords:get():close(function ()
        if self._remoteShopActor ~= actor then
            return
        end
        self:onShopRecordsClose()
        Engine.Actor.BlueprintEvent(
            actor,
            Engine.Actor,
            "onCollision",
            { other = { self._scene:getPlayer() } },
            function ()
                if self._remoteShopActor == actor then
                    releaseRemoteShop(self)
                end
            end
        )
    end)
end

function Windows:openPlayerName()
    if self._playerNameSession ~= nil and self._playerNameSession.operation:getStatus() == Engine.AsyncOperation.PENDING then
        return self._playerNameSession.operation
    end
    local player = self._scene:getPlayer()
    self._playerNameSession = createSession(player, player:getMoveEnabled())
    local window = self._windowPlayerName:get()
    if not window:getVisible() then
        player:setMoveEnabled(false)
        window:open()
        self._scene:blockMapInput(MAP_INPUT_BLOCK_FRAMES)
    end
    return self._playerNameSession.operation
end

function Windows:openShop(buyItemIDs, canSell, actor)
    local player = self._scene:getPlayer()
    local enabled = self:isMenuBlocking() or player:getMoveEnabled()
    if self._shopSession ~= nil then
        enabled = self._shopSession.enabled
        self._shopSession.operation:cancel()
    end
    self._shopSession = createSession(player, enabled)
    player:setMoveEnabled(false)
    self._windowShop:get():open(buyItemIDs, canSell)
    recordShopVisit(self, actor)
    return self._shopSession.operation
end

function Windows:openAttrShop(actor, shopName, shopDescription, abilities, priceRef, priceIncrement, moneyName)
    local player = self._scene:getPlayer()
    local enabled = self:isMenuBlocking() or player:getMoveEnabled()
    if self._attrShopSession ~= nil then
        enabled = self._attrShopSession.enabled
        self._attrShopSession.operation:cancel()
    end
    self._attrShopSession = createSession(player, enabled)
    player:setMoveEnabled(false)
    self._windowAttrShop:get():open(actor, shopName, shopDescription, abilities, priceRef, priceIncrement, moneyName)
    recordShopVisit(self, actor)
    return self._attrShopSession.operation
end

function Windows:onShopClose()
    local released = { session = self._shopSession }
    self._shopSession = nil
    if released.session ~= nil then
        restoreMovement(self, released.session, WINDOW_CLOSE_INPUT_BLOCK_FRAMES)
        released.session.operation:complete(true)
    end
end

function Windows:onAttrShopClose()
    local released = { session = self._attrShopSession }
    self._attrShopSession = nil
    if released.session ~= nil then
        restoreMovement(self, released.session, WINDOW_CLOSE_INPUT_BLOCK_FRAMES)
        released.session.operation:complete(true)
    end
end

function Windows:onPlayerNameClose()
    local released = { session = self._playerNameSession }
    self._playerNameSession = nil
    if released.session ~= nil then
        restoreMovement(self, released.session, MAP_INPUT_BLOCK_FRAMES)
        released.session.operation:complete(true)
    end
end

function Windows:onItemClose()
    local menu = self._windowMenu:peek()
    if menu ~= nil and menu:getVisible() then
        menu:onSubMenuClose()
        return
    end
    restoreMovement(self, self._itemMovement, WINDOW_CLOSE_INPUT_BLOCK_FRAMES)
    self._itemMovement = nil
end

function Windows:onEquipClose()
    local menu = self._windowMenu:peek()
    if menu ~= nil and menu:getVisible() then
        menu:onSubMenuClose()
        return
    end
    restoreMovement(self, self._equipMovement, WINDOW_CLOSE_INPUT_BLOCK_FRAMES)
    self._equipMovement = nil
end

function Windows:onHotkeyItemUsed()
    local menu = self._windowMenu:peek()
    if menu ~= nil and menu:getVisible() then
        menu:close()
        return
    end
    restoreMovement(self, self._itemMovement, WINDOW_CLOSE_INPUT_BLOCK_FRAMES)
    self._itemMovement = nil
end

function Windows:onEnemyBookClose()
    restoreMovement(self, self._enemyBookMovement, WINDOW_CLOSE_INPUT_BLOCK_FRAMES)
end

function Windows:onEnemyBookConfirm(entry)
    self._windowEnemyEncyclopedia:get():open(entry)
    self._scene:blockMapInput(MAP_INPUT_BLOCK_FRAMES)
end

function Windows:onEnemyEncyclopediaClose()
    restoreMovement(self, self._enemyBookMovement, WINDOW_CLOSE_INPUT_BLOCK_FRAMES)
end

function Windows:onFloorTeleporterClose()
    restoreMovement(self, self._floorTeleporterMovement, WINDOW_CLOSE_INPUT_BLOCK_FRAMES)
end

function Windows:onFloorTeleporterConfirm(mapKey, telepoint)
    local targetMap = self._scene:resolveRegionMapPath(mapKey)
    local targetPosition = sf.Vector2i.new(telepoint.x, telepoint.y)
    ---@cast targetPosition sf.Vector2i
    local window = self._windowFloorTeleporter:peek()
    if window ~= nil then
        window:close()
    end
    self._scene:gotoMapAndPos(targetMap, targetPosition)
    restoreMovement(self, self._floorTeleporterMovement, MAP_INPUT_BLOCK_FRAMES)
end

function Windows:onSaveLoadClose(reason)
    local menu = self._windowMenu:peek()
    if menu ~= nil and menu:getVisible() then
        if reason == SaveLoadCloseReason.CANCEL then
            menu:onSaveLoadClose()
            return
        end
        menu:close()
        return
    end
    restoreMovement(self, self._saveLoadMovement, WINDOW_CLOSE_INPUT_BLOCK_FRAMES)
    self._saveLoadMovement = nil
end

function Windows:onConfigClose()
    local menu = self._windowMenu:peek()
    if menu ~= nil then
        menu:onConfigClose()
    end
end

function Windows:cancelAsyncOperations()
    local released = {
        shop = self._shopSession,
        attrShop = self._attrShopSession,
        playerName = self._playerNameSession
    }
    self._shopSession = nil
    self._attrShopSession = nil
    self._playerNameSession = nil
    cancelSession(self, released.shop, self._windowShop)
    cancelSession(self, released.attrShop, self._windowAttrShop)
    cancelSession(self, released.playerName, self._windowPlayerName)
    local records = self._windowShopRecords:peek()
    if records ~= nil then
        records:dismiss()
    end
    self:onShopRecordsClose()
    releaseRemoteShop(self)
end

function Windows:cancelPendingRequests()
    self._pendingMenuOpen = false
    self._pendingSaveLoadOpen = nil
    self._pendingQuickSave = false
end

function Windows:rebindPlayer()
    self._itemMovement = nil
    self._equipMovement = nil
    self._saveLoadMovement = nil
    self._enemyBookMovement = nil
    self._floorTeleporterMovement = nil
    self._shopRecordsMovement = nil
    local windows = {
        self._windowItem, self._windowEquip, self._windowMenu, self._windowShop, self._windowAttrShop,
        self._windowEnemyBook, self._windowPlayerName
    }
    for _, handle in ipairs(windows) do
        local window = handle:peek()
        if window ~= nil then
            window:setPlayer(self._scene:getPlayer())
        end
    end
end

function Windows:refreshLocale()
    local menu = self._windowMenu:peek()
    if menu ~= nil then
        menu:refreshRows()
    end
    local windows = {
        self._windowItem, self._windowEquip, self._windowShop, self._windowAttrShop, self._windowEnemyBook,
        self._windowEnemyEncyclopedia, self._windowFloorTeleporter, self._windowPlayerName, self._windowShopRecords
    }
    for _, handle in ipairs(windows) do
        local window = handle:peek()
        if window ~= nil then
            window:refreshLocale()
        end
    end
end

function Windows:dispose()
    self:cancelAsyncOperations()
    self:cancelPendingRequests()
    local windows = {
        self._windowMenu, self._windowItem, self._windowEquip, self._windowAttrShop, self._windowEnemyBook,
        self._windowEnemyEncyclopedia, self._windowSaveLoad, self._windowShop, self._windowFloorTeleporter,
        self._configWindow, self._windowPlayerName, self._windowShopRecords
    }
    for _, window in ipairs(windows) do
        window:dispose()
    end
end

return class(Windows)
