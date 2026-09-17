local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local LazyWindow = require("Source.UIBase.LazyWindow")

local Direction = Engine.FocusDirection
local FocusGroup = GlobalCore.FocusGroup
local FocusNeighbor = GlobalCore.FocusNeighbor
local FocusTransition = GlobalCore.FocusTransition

local MENU_Z_ORDER = 1

local Windows = {}

---@param manager GlobalCore.UIManager
---@return table<string, GlobalCore.FocusGroup>
local function createFocusGroups(manager)
    local groups = {}
    for _, name in ipairs({
        "menu",
        "item",
        "equip-slot",
        "equip-select",
        "shop-item",
        "floor-command",
        "floor-preview",
        "save-slot",
        "player-name"
    }) do
        local group = FocusGroup.new(name)
        groups[name] = group
        manager:registerFocusGroup(group)
    end
    groups.item:setNeighbor(Direction.LEFT, groups.menu)
    groups["equip-slot"]:setNeighbor(Direction.LEFT, groups.menu)
    groups["equip-slot"]:setNeighbor(
        Direction.RIGHT, FocusNeighbor.new(groups["equip-select"], FocusTransition.EXPLICIT)
    )
    groups["equip-select"]:setNeighbor(
        Direction.LEFT, FocusNeighbor.new(groups["equip-slot"], FocusTransition.EXPLICIT)
    )
    groups["floor-command"]:setNeighbor(
        Direction.RIGHT, FocusNeighbor.new(groups["floor-preview"], FocusTransition.EXPLICIT)
    )
    groups["floor-preview"]:setNeighbor(
        Direction.LEFT, FocusNeighbor.new(groups["floor-command"], FocusTransition.EXPLICIT)
    )
    groups["save-slot"]:setNeighbor(Direction.LEFT, groups.menu)
    return groups
end

---@param group   GlobalCore.FocusGroup
---@param control Engine.FunctionalBase
local function bindFocusControl(group, control)
    group:addItem(control)
    group.activeOwner = control
end

function Windows.Create(self)
    local manager = assert(self:getUIManager(), "Scene map UI manager is unavailable")
    local groups = createFocusGroups(manager)
    local function onSubMenuClose()
        self:_onHotkeySubMenuClose()
    end
    self._playerNameMoveEnabledBeforeOpen = true
    self._shopMoveEnabledBeforeOpen = true
    self._attrShopMoveEnabledBeforeOpen = true
    self._enemyBookMoveEnabledBeforeOpen = true
    self._floorTeleporterMoveEnabledBeforeOpen = true
    self._itemMoveEnabledBeforeOpen = true
    self._equipMoveEnabledBeforeOpen = true
    self._saveLoadMoveEnabledBeforeOpen = true
    self._messageWindow = LazyWindow.new(function ()
        local WindowMessage = require("Source.Windows.WindowMessage")

        local window = WindowMessage.new()
        window:mount(manager)
        return window
    end)
    self._windowPlayerName = LazyWindow.new(function ()
        local WindowPlayerName = require("Source.Windows.WindowPlayerName")

        local window = WindowPlayerName.new(self.player, function ()
            self:_onPlayerNameClose()
        end)
        for _, control in ipairs(window:getFocusControls()) do
            groups["player-name"]:addItem(control)
        end
        groups["player-name"].activeOwner = window
        window:mount(manager)
        return window
    end)
    self._windowItem = LazyWindow.new(function ()
        local WindowItem = require("Source.Windows.WindowItem")

        local window = WindowItem.new(self.player)
        window:setZOrder(MENU_Z_ORDER)
        window:setOnCloseCallback(onSubMenuClose)
        window:setOnUseCallback(function ()
            self:_onHotkeyItemUsed()
        end)
        bindFocusControl(groups.item, window)
        window:mount(manager)
        return window
    end)
    self._windowEquip = LazyWindow.new(function ()
        local WindowEquip = require("Source.Windows.WindowEquip")

        local window = WindowEquip.new(self.player)
        window:setZOrder(MENU_Z_ORDER)
        window:setOnCloseCallback(onSubMenuClose)
        local slot, selection = window:getFocusControls()
        bindFocusControl(groups["equip-slot"], slot)
        bindFocusControl(groups["equip-select"], selection)
        window:mount(manager)
        return window
    end)
    self._windowShop = LazyWindow.new(function ()
        local WindowShop = require("Source.Windows.WindowShop")

        local window = WindowShop.new(self.player, function ()
            self:_onShopClose()
        end)
        bindFocusControl(groups["shop-item"], window:getItemWindow())
        window:mount(manager)
        return window
    end)
    self._windowAttrShop = LazyWindow.new(function ()
        local WindowAttrShop = require("Source.Windows.WindowAttrShop")

        local window = WindowAttrShop.new(self.player, function ()
            self:_onAttrShopClose()
        end)
        window:mount(manager)
        return window
    end)
    self._windowEnemyBook = LazyWindow.new(function ()
        local WindowEnemyBook = require("Source.Windows.WindowEnemyBook")

        local window = WindowEnemyBook.new(
            self.player,
            function ()
                self:_onEnemyBookClose()
            end,
            function (entry)
                self:_onEnemyBookConfirm(entry)
            end
        )
        window:mount(manager)
        return window
    end)
    self._windowEnemyEncyclopedia = LazyWindow.new(function ()
        local WindowEnemyEncyclopedia = require("Source.Windows.WindowEnemyEncyclopedia")

        local window = WindowEnemyEncyclopedia.new(function ()
            self:_onEnemyEncyclopediaClose()
        end)
        window:mount(manager)
        return window
    end)
    self._windowFloorTeleporter = LazyWindow.new(function ()
        local WindowFloorTeleporter = require("Source.Windows.WindowFloorTeleporter")

        local window = WindowFloorTeleporter.new(
            self.inst,
            function (mapKey, telepoint, previewSize, previewScale, showTelepointMarker)
                return self:_buildFloorMapPreview(mapKey, telepoint, previewSize, previewScale, showTelepointMarker)
            end,
            function (mapKey, telepoint)
                self:_onFloorTeleporterConfirm(mapKey, telepoint)
            end,
            function ()
                self:_onFloorTeleporterClose()
            end,
            function (mapKey)
                return self._mapBuilder:resolveMapPath(mapKey, self:_getCurrentRegionMap())
            end,
            function ()
                self._mapBuilder:clearFloorMapPreviewCache()
            end
        )
        bindFocusControl(groups["floor-command"], window:getCommandWindow())
        bindFocusControl(groups["floor-preview"], window:getPreviewWindow())
        window:mount(manager)
        return window
    end)
    self._windowSaveLoad = LazyWindow.new(function ()
        local WindowSaveLoad = require("Source.Windows.WindowSaveLoad")

        local window = WindowSaveLoad.new(
            false,
            function ()
                return self:_getSaveSource()
            end,
            function (reason)
                self:_onSaveLoadClose(reason)
            end,
            function (inst)
                self:applyLoadedGame(inst)
            end
        )
        window:setZOrder(MENU_Z_ORDER)
        bindFocusControl(groups["save-slot"], window:getSlotWindow())
        window:mount(manager)
        return window
    end)
    self._configWindow = LazyWindow.new(function ()
        local ConfigWindow = require("Source.Windows.ConfigWindow")

        local window = ConfigWindow.new(function ()
            self:_onConfigClose()
        end)
        window:setZOrder(MENU_Z_ORDER)
        window:mount(manager)
        return window
    end)
    self._windowMenu = LazyWindow.new(function ()
        local WindowMenu = require("Source.Windows.WindowMenu")

        local window = WindowMenu.new(
            self.player,
            {
                item = self._windowItem,
                equip = self._windowEquip,
                saveLoad = self._windowSaveLoad,
                config = self._configWindow
            },
            function ()
                local SceneTitle = require("Source.Scenes.SceneTitle")

                GlobalCore.System.setScene(SceneTitle.new())
            end
        )
        window:setZOrder(MENU_Z_ORDER)
        window:setMoveRestoreGuard(function ()
            return self:_canRestoreMoveAfterMenuClose()
        end)
        bindFocusControl(groups.menu, window)
        window:mount(manager)
        return window
    end)
    self._blockingWindows = {
        self._windowShop, self._windowAttrShop, self._windowEnemyBook, self._windowEnemyEncyclopedia,
        self._windowFloorTeleporter, self._windowPlayerName, self._windowItem, self._windowEquip, self._windowSaveLoad,
        self._configWindow
    }
end

function Windows.Dispose(self)
    local windows = {
        self._messageWindow, self._windowMenu, self._windowItem, self._windowEquip, self._windowAttrShop,
        self._windowEnemyBook, self._windowEnemyEncyclopedia, self._windowSaveLoad, self._windowShop,
        self._windowFloorTeleporter, self._configWindow, self._windowPlayerName
    }
    for _, window in ipairs(windows) do
        window:dispose()
    end
end

return Windows
