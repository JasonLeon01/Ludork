local Engine = require("Engine")
local Data = require("Source.Data")
local EventKeys = require("Source.Configs.EventKeys")
---@type { Item: Source.Configs.GeneralEnum.Item, State: Source.Configs.GeneralEnum.State }
local GeneralEnum = require("Source.Configs.GeneralEnum")
local GeneralDataTypes = require("Source.Configs.GeneralDataTypes")
local Effects = require("Source.Gameplay.Effects")
local LocaleCore = require("Source.Locale.Core")
local NumberFormat = require("Source.Utils.NumberFormat")
local IconTexture = require("Source.UIBase.IconTexture")
local PlayerStateRowController = require("Source.Windows.HUDPlayerAttr.PlayerStateRow.Controller")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.PlayerAttrHUD")
local GameplayConstants = require("Source.Configs.GameplayConstants")

---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat
local Canvas = Engine.Canvas
local Item = GeneralEnum.Item
local State = GeneralEnum.State
local ToShortNumber = NumberFormat.ToShortNumber
local createStateSignature = tuple
local createSignature = tuple
---@cast createStateSignature fun(values: string[]): tuple<string>
---@cast createSignature fun(...: any): tuple<any>

local function getStateSignature(states)
    ---@type string[]
    local stateIDs = {}
    for index, state in ipairs(states) do
        stateIDs[index] = state.ID
    end
    return createStateSignature(stateIDs)
end

local function getStateDisplaySignature(states, language)
    ---@type string[]
    local values = { language }
    for _, state in ipairs(states) do
        local stateData = Data.GetGeneralStateData(state.ID)
        values[#values + 1] = state.ID
        values[#values + 1] = state.icon or stateData.icon or ""
        values[#values + 1] = state.name or ""
    end
    return createStateSignature(values)
end

local function stateSignatureMatches(currentSignature, rowCount, signature)
    return currentSignature == signature and rowCount == #signature
end

---@class Source.Windows.PlayerAttrHUD.Controller
local Controller = {}

Controller.refreshEvents = { EventKeys.LocaleChanged, EventKeys.AbilitySystemChanged, EventKeys.PlayerChanged }

function Controller:init(player, openMenuCallback)
    self._player = player
    self._openMenuCallback = openMenuCallback
    self._avatarTexture = nil
    self._avatarRect = nil
    self._avatarOffset = 0
    self._stateSignature = nil
    self._stateDisplaySignature = nil
    self._language = ""
    self._headerSignature = nil
    self._combatSignature = nil
    self._hpSignature = nil
    self._statSignature = nil
    self._stackSignature = nil
    self._progressSignature = nil
    self._keySignature = nil
    self:_initialiseAvatar(player)
    self._states = self:createCollection(self.ui.controls["StateHost"], PlayerStateRowController)
end

function Controller:setPlayer(player)
    self._player = player
    self:refresh()
end

---@param payload Source.Configs.EventKeys.ChangePayload | { language: string } | nil
function Controller:refreshFromEvent(payload)
    if payload ~= nil and payload.owner ~= nil and payload.owner ~= self:getPlayer() then
        return
    end
    super(Controller, self).refreshFromEvent(payload)
end

function Controller:getPlayer()
    return self._player
end

function Controller:openMenu()
    if self._openMenuCallback ~= nil then
        self._openMenuCallback()
    end
end

---@param player Source.Player.Player
function Controller:_initialiseAvatar(player)
    local texture = player:getTexture()
    if texture == nil then
        return
    end
    local textureSize = texture:getSize()
    local frameWidth = math.max(1, math.floor(textureSize.x / 4))
    local frameHeight = math.max(1, math.floor(textureSize.y / 4))
    local frameSize = math.min(frameWidth, frameHeight)
    self._avatarTexture = texture
    local avatarRect = sf.IntRect.new(0, 0, frameWidth, frameHeight)
    ---@cast avatarRect sf.IntRect
    self._avatarRect = avatarRect
    local authoredSize = self.ui.controls["Avatar"]:getLocalBounds().size
    self._avatarOffset = math.max(0, frameSize - math.min(authoredSize.x, authoredSize.y))
end

function Controller:bind()
    if self._avatarTexture == nil then
        self:setProperty("Avatar", "visible", false)
        return
    end
    ---@cast self._avatarTexture sf.Texture
    ---@cast self._avatarRect sf.IntRect
    self.ui.controls["Avatar"]:setTexture(self._avatarTexture, true)
    self.ui.controls["Avatar"]:setTextureRect(self._avatarRect)
    self:setProperty("Avatar", "visible", true)
    self.ui.controls["Avatar"]:addClickCallback(self:bindCallback(Controller.openMenu))
end

---@param states    Source.Configs.GeneralDataTypes.StateAttributeSet[]
---@param signature tuple<string>
function Controller:_rebuildStateRows(states, signature)
    self._states:clear()
    self._stateDisplaySignature = nil
    self._stateSignature = signature
    for _ in ipairs(states) do
        self._states:add({ iconTexture = nil, name = "" })
    end
end

---@param states Source.Configs.GeneralDataTypes.StateAttributeSet[]
function Controller:_updateStateRows(states)
    local x = 0.0
    for index, state in ipairs(states) do
        local stateData = Data.GetGeneralStateData(state.ID)
        local iconPath = state.icon or stateData.icon or ""
        local texture = IconTexture.Load(iconPath)
        local row = assert(self._states.items[index])
        row.model.iconTexture = texture
        row.model.name = state.name
        local rowRoot = row:prepare()
        rowRoot:setPosition(sf.Vector2f.new(x, rowRoot:getPosition().y))
        x = x + row:getWidth()
    end
end

function Controller:refreshStates(language)
    local states = {}
    for _, stateID in ipairs(Effects.GetStateIDs(self:getPlayer())) do
        states[#states + 1] = GeneralDataTypes.Create("State", stateID, Data.GetGeneralStateData(stateID))
    end
    local signature = getStateSignature(states)
    local rebuild = not stateSignatureMatches(self._stateSignature, #self._states.items, signature)
    if rebuild then
        self:_rebuildStateRows(states, signature)
    end
    local displaySignature = getStateDisplaySignature(states, language or LocaleCore.GetLanguage())
    if not rebuild and self._stateDisplaySignature == displaySignature then
        return false
    end
    self._stateDisplaySignature = displaySignature
    if bool(states) then
        self:_updateStateRows(states)
    end
    return true
end

function Controller:refresh()
    local layoutDirty = false
    local language = LocaleCore.GetLanguage()
    local localeChanged = self._language ~= language
    local gameMap = self:getPlayer():getMap()
    local mapName = ""
    if gameMap ~= nil then
        ---@cast gameMap GameMap
        mapName = tostring(gameMap.mapName)
    end
    local playerName = self:getPlayer():getDisplayName()
    local headerSignature = createSignature(language, mapName, playerName)
    if self._headerSignature ~= headerSignature then
        self._headerSignature = headerSignature
        self._language = language
        self:setText("MapName", LOC(tostring(mapName)))
        self:setText(
            "PlayerName",
            Engine.TextLayout.fitPlainText(
                playerName, self.ui.controls["HUDContent"]:getSize().x, self.ui.controls["PlayerName"]
            )
        )
        self:setText("HpLabel", LOC("HP"))
        self:setText("AtkLabel", LOC("ATK"))
        self:setText("DefLabel", LOC("DEF"))
        self:setText("ExpLabel", LOC("EXP"))
        self:setText("GoldLabel", LOC("GOLD"))
        layoutDirty = true
    end

    local abilitySystem = self:getPlayer():getAbilitySystemComponent()
    local combatSignature = createSignature(self:getPlayer().attributes, abilitySystem:getRevision())
    local refreshStateRows = localeChanged
    if self._combatSignature ~= combatSignature then
        self._combatSignature = combatSignature
        refreshStateRows = true

        local hpSignature = createSignature(self:getPlayer().attributes.HP, self:getPlayer().attributes.MAXHP)
        if self._hpSignature ~= hpSignature then
            self._hpSignature = hpSignature
            self:setText(
                "HpValue",
                "#default#" .. tostring(ToShortNumber(self:getPlayer().attributes.HP)) .. "/#max#"
                    .. tostring(ToShortNumber(self:getPlayer().attributes.MAXHP)) .. "#default#"
            )
            self.ui.controls["HpBar"]:setProgress(
                self:getPlayer().attributes.MAXHP > 0 and self:getPlayer().attributes.HP
                        / self:getPlayer().attributes.MAXHP or 0.0
            )
            layoutDirty = true
        end

        local statSignature = createSignature(self:getPlayer().attributes.ATK, self:getPlayer().attributes.DEF)
        if self._statSignature ~= statSignature then
            self._statSignature = statSignature
            self:setText("AtkValue", tostring(ToShortNumber(self:getPlayer().attributes.ATK)))
            self:setText("DefValue", tostring(ToShortNumber(self:getPlayer().attributes.DEF)))
            layoutDirty = true
        end

        local weakStacks = abilitySystem:getActiveEffectStacks(GameplayConstants.STATE_PREFIX .. State.Weak)
        local poisonStacks = abilitySystem:getActiveEffectStacks(GameplayConstants.STATE_PREFIX .. State.Poisoned)
        local stackSignature = createSignature(weakStacks, poisonStacks)
        if self._stackSignature ~= stackSignature then
            self._stackSignature = stackSignature
            local debuffString = weakStacks > 0 and "(-" .. tostring(weakStacks) .. ")" or ""
            self:setText("AtkDebuff", debuffString)
            self:setText("DefDebuff", debuffString)
            self:setText("HpPoison", poisonStacks > 0 and "(" .. tostring(poisonStacks) .. ")" or "")
            layoutDirty = true
        end

        local progressSignature = createSignature(
            self:getPlayer().attributes.LEVEL, self:getPlayer().attributes.EXP, self:getPlayer().attributes.GOLD
        )
        if self._progressSignature ~= progressSignature then
            self._progressSignature = progressSignature
            self:setText("Level", "Lv. " .. tostring(self:getPlayer().attributes.LEVEL))
            self:setText("ExpValue", tostring(ToShortNumber(self:getPlayer().attributes.EXP)))
            self:setText("GoldValue", tostring(ToShortNumber(self:getPlayer().attributes.GOLD)))
            layoutDirty = true
        end
    end

    local keyYCount = self:getPlayer():getItemCount(Item.KEY_Y)
    local keyBCount = self:getPlayer():getItemCount(Item.KEY_B)
    local keyRCount = self:getPlayer():getItemCount(Item.KEY_R)
    local keySignature = createSignature(keyYCount, keyBCount, keyRCount)
    if self._keySignature ~= keySignature then
        self._keySignature = keySignature
        self:setText(
            "ItemCounts",
            "#Yellow#" .. string.format("%02d", keyYCount) .. "#default#  #Blue#" .. string.format("%02d", keyBCount)
                .. "#default#  #Red#" .. string.format("%02d", keyRCount) .. "#default#"
        )
        layoutDirty = true
    end

    if refreshStateRows then
        self:refreshStates(language)
    end
    if layoutDirty then
        self.view:reflow()
        self:_applyContentLayout()
    end
end

function Controller:_applyContentLayout()
    local hpPosition = self.ui.controls["HpValue"]:getPosition()
    for _, name in ipairs({ "HpLabel", "HpPoison" }) do
        local control = self.ui.controls[name]
        local position = control:getPosition()
        control:setPosition(sf.Vector2f.new(position.x, hpPosition.y))
    end
    if self._avatarOffset <= 0 then
        return
    end
    local mapPosition = self.ui.controls["MapName"]:getPosition()
    self.ui.controls["MapName"]:setPosition(sf.Vector2f.new(mapPosition.x + self._avatarOffset, mapPosition.y))
    for _, name in ipairs({ "PlayerName", "Level", "StateHost" }) do
        local control = self.ui.controls[name]
        local position = control:getPosition()
        control:setPosition(sf.Vector2f.new(position.x, position.y + self._avatarOffset))
    end
end

function Controller:prepare(logicalSize)
    local root = super(Controller, self).prepare(logicalSize)
    self:_applyContentLayout()
    return root
end

return Ui.DefineWindow(View, Controller, Canvas)
