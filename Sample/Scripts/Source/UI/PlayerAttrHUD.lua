local Data = require("Source.Data")
local EventKeys = require("Source.Configs.EventKeys")
---@type { Item: Source.Configs.GeneralEnum.Item, State: Source.Configs.GeneralEnum.State }
local GeneralEnum = require("Source.Configs.GeneralEnum")
local LocaleCore = require("Source.Locale.Core")
local NodeUtils = require("Source.NodeFunctions.Utils")
local IconTexture = require("Source.UI.IconTexture")
local PlayerStateRowUI = require("Source.UI.Parts.PlayerAttrHUD.PlayerStateRow")
local Ui = require("Source.UI.Ui")

---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat
local Item = GeneralEnum.Item
local State = GeneralEnum.State
local ToShortNumber = NodeUtils.ToShortNumber
local createStateSignature = tuple
local createSignature = tuple
---@cast createStateSignature fun(values: string[]): tuple<string>
---@cast createSignature fun(...: any): tuple<any>

---@class Source.UI.PlayerAttrHUD.PlayerAttrHUDUI
local PlayerAttrHUDUI = {}

PlayerAttrHUDUI.refreshEvents = { EventKeys.LocaleChanged }

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
        local stateData = Data.getGeneralStateData(state.ID)
        values[#values + 1] = state.ID
        values[#values + 1] = state.icon or stateData.icon or ""
        values[#values + 1] = state.name or ""
    end
    return createStateSignature(values)
end

local function stateSignatureMatches(ui, signature)
    return ui._stateSignature == signature and #ui._stateUIs == #signature
end

function PlayerAttrHUDUI:init(model)
    self._avatarTexture = nil
    self._avatarRect = nil
    self._avatarSize = model._AVATAR_MIN_SIZE
    self._infoStartX = model._AVATAR_MIN_SIZE
    self._hpBarWidth = model._HP_BAR_WIDTH
    self._stateUIs = {}
    self._stateWidgets = {}
    self._stateSignature = nil
    self._stateDisplaySignature = nil
    self._stateIconCache = {}
    self._hpRate = 0.0
    self._language = ""
    self._headerSignature = nil
    self._combatSignature = nil
    self._hpSignature = nil
    self._statSignature = nil
    self._stackSignature = nil
    self._progressSignature = nil
    self._keySignature = nil
    self._layoutDirty = false
    self:_initialiseAvatar(model._player)
    self:_initialiseLayout(model)
    super(PlayerAttrHUDUI, self).init(model)
end

---@param player Source.Player.Player
function PlayerAttrHUDUI:_initialiseAvatar(player)
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
    self._avatarSize = math.max(self._avatarSize, frameSize)
    self._infoStartX = math.max(self._infoStartX, self._avatarSize)
end

function PlayerAttrHUDUI:_initialiseLayout(model)
    local hudWidth = math.max(
        self._infoStartX + self._hpBarWidth, self._hpBarWidth + model._AVATAR_MIN_SIZE, model._STAT_VALUE_X + 16
    )
    self._hpBarWidth = hudWidth
    local keyRowHeight = math.max(model._FONT_SIZE, model._KEY_ICON_HEIGHT)
    local hudHeight = model._KEY_ROW_Y + keyRowHeight + 4
    ---@cast hudWidth integer
    ---@cast hudHeight integer
    local logicalSize = sf.Vector2u.new(hudWidth, hudHeight)
    ---@cast logicalSize sf.Vector2u
    self._logicalSize = logicalSize

    model._avatarSize = self._avatarSize
    model._infoStartX = self._infoStartX
    model._hpBarWidth = self._hpBarWidth
    model._hudWidth = hudWidth
    model._hudHeight = hudHeight
    model._stateWidgets = self._stateWidgets
    model._stateSignature = self._stateSignature
    model._stateIconCache = self._stateIconCache
end

function PlayerAttrHUDUI:bind()
    self._avatar = self:requireControl("Avatar")
    self._mapNameText = self:requireControl("MapName")
    self._levelText = self:requireControl("Level")
    self._stateHost = self:requireControl("StateHost")
    self._hpBack = self:requireControl("HpBack")
    self._hpFill = self:requireControl("HpFill")
    self._hpLabelText = self:requireControl("HpLabel")
    self._hpText = self:requireControl("HpValue")
    self._atkDebuffText = self:requireControl("AtkDebuff")
    self._defDebuffText = self:requireControl("DefDebuff")
    self._hpPoisonText = self:requireControl("HpPoison")
    self._keyIcon = self:requireControl("KeyIcon")
    self._itemText = self:requireControl("ItemCounts")
    self._statValueTexts = {
        ATK = self:requireControl("AtkValue"),
        DEF = self:requireControl("DefValue"),
        EXP = self:requireControl("ExpValue"),
        GOLD = self:requireControl("GoldValue")
    }
    ---@cast self._avatar Engine.Button
    ---@cast self._mapNameText Engine.PlainText
    ---@cast self._levelText Engine.PlainText
    ---@cast self._stateHost Engine.Canvas
    ---@cast self._hpBack Engine.SolidRect
    ---@cast self._hpFill Engine.SolidRect
    ---@cast self._hpLabelText Engine.PlainText
    ---@cast self._hpText Engine.RichText
    ---@cast self._atkDebuffText Engine.PlainText
    ---@cast self._defDebuffText Engine.PlainText
    ---@cast self._hpPoisonText Engine.PlainText
    ---@cast self._keyIcon Engine.Image
    ---@cast self._itemText Engine.RichText
    ---@cast self._statValueTexts table<string, Engine.PlainText>
    self:_bindAvatar()
    self:_publishControls()
end

function PlayerAttrHUDUI:_bindAvatar()
    if self._avatarTexture == nil then
        self:setProperty("Avatar", "visible", false)
        return
    end
    ---@cast self._avatarTexture sf.Texture
    ---@cast self._avatarRect sf.IntRect
    self._avatar:setTexture(self._avatarTexture, true)
    self._avatar:setTextureRect(self._avatarRect)
    self:setProperty("Avatar", "visible", true)
    if self.model._openMenuCallback == nil then
        return
    end
    ---@type Source.Windows.PlayerAttrHUD[]
    local modelRef = setmetatable({ self.model }, {
        __mode = "v"
    })
    self._avatar:addClickCallback(function ()
        local model = modelRef[1]
        if model ~= nil and model._openMenuCallback ~= nil then
            model._openMenuCallback()
        end
    end)
end

function PlayerAttrHUDUI:_publishControls()
    self.model._avatar = self._avatarTexture ~= nil and self._avatar or nil
    self.model._mapNameText = self._mapNameText
    self.model._levelText = self._levelText
    self.model._hpBack = self._hpBack
    self.model._hpFill = self._hpFill
    self.model._hpLabelText = self._hpLabelText
    self.model._hpText = self._hpText
    self.model._statValueTexts = self._statValueTexts
    self.model._atkDebuffText = self._atkDebuffText
    self.model._defDebuffText = self._defDebuffText
    self.model._hpPoisonText = self._hpPoisonText
    self.model._keyIcon = self._keyIcon
    self.model._itemText = self._itemText
end

function PlayerAttrHUDUI:getLogicalSize()
    return self._logicalSize
end

function PlayerAttrHUDUI:loadStateIcon(iconPath)
    if not bool(iconPath) then
        return nil
    end
    if self._stateIconCache[iconPath] ~= nil then
        return self._stateIconCache[iconPath]
    end
    local texture = IconTexture.load(iconPath, "Icons/States")
    self._stateIconCache[iconPath] = texture
    return texture
end

function PlayerAttrHUDUI:clearStateRows()
    for _, rowUI in ipairs(self._stateUIs) do
        self._stateHost:removeChild(rowUI:getRoot())
    end
    self._stateUIs = {}
    self._stateWidgets = {}
    self._stateSignature = nil
    self._stateDisplaySignature = nil
    self.model._stateWidgets = self._stateWidgets
    self.model._stateSignature = nil
end

function PlayerAttrHUDUI:stateSignatureMatches(states)
    return stateSignatureMatches(self, getStateSignature(states))
end

---@param states    Source.Infos.StateInfo[]
---@param signature tuple<string>
function PlayerAttrHUDUI:_rebuildStateRows(states, signature)
    self:clearStateRows()
    self._stateSignature = signature
    for index in ipairs(states) do
        self._stateUIs[index] = PlayerStateRowUI.new({
            iconSize = self.model._STATE_ICON_SIZE,
            iconTexture = nil,
            name = ""
        })
    end
    self.model._stateSignature = self._stateSignature
end

---@param states Source.Infos.StateInfo[]
function PlayerAttrHUDUI:_updateStateRows(states)
    local x = 0.0
    for index, state in ipairs(states) do
        local stateData = Data.getGeneralStateData(state.ID)
        local iconPath = state.icon or stateData.icon or ""
        local texture = bool(iconPath) and self:loadStateIcon(iconPath) or nil
        ---@diagnostic disable: need-check-nil
        self._stateUIs[index].model.iconTexture = texture
        self._stateUIs[index].model.name = state.name
        local rowRoot = self._stateUIs[index]:prepare()
        if self._stateWidgets[index] == nil then
            self._stateHost:addChild(rowRoot)
            self._stateWidgets[index] = rowRoot
        end
        rowRoot:setPosition(sf.Vector2f.new(x, 0.0))
        x = x + self._stateUIs[index]:getWidth() + self.model._STATE_GAP
        ---@diagnostic enable: need-check-nil
    end
    self.model._stateWidgets = self._stateWidgets
end

function PlayerAttrHUDUI:refreshStates(language)
    local states = self.model._player:getStates()
    local signature = getStateSignature(states)
    local rebuild = not stateSignatureMatches(self, signature)
    if rebuild then
        self:_rebuildStateRows(states, signature)
    end
    local displaySignature = getStateDisplaySignature(states, language or LocaleCore.getLanguage())
    if not rebuild and self._stateDisplaySignature == displaySignature then
        return false
    end
    self._stateDisplaySignature = displaySignature
    if bool(states) then
        self:_updateStateRows(states)
    end
    return true
end

function PlayerAttrHUDUI.formatMapName(mapName)
    return LOC(tostring(mapName))
end

function PlayerAttrHUDUI:getMapDisplayName()
    local gameMap = self.model._player:getMap()
    if gameMap == nil then
        return ""
    end
    ---@cast gameMap GameMap
    return PlayerAttrHUDUI.formatMapName(gameMap.mapName)
end

function PlayerAttrHUDUI:refresh()
    local layoutDirty = false
    local language = LocaleCore.getLanguage()
    local localeChanged = self._language ~= language
    local gameMap = self.model._player:getMap()
    local mapName = ""
    if gameMap ~= nil then
        ---@cast gameMap GameMap
        mapName = tostring(gameMap.mapName)
    end
    local headerSignature = createSignature(language, mapName)
    if self._headerSignature ~= headerSignature then
        self._headerSignature = headerSignature
        self._language = language
        self:setText("MapName", PlayerAttrHUDUI.formatMapName(mapName))
        self:setText("HpLabel", LOC("HP"))
        self:setText("AtkLabel", LOC("ATK"))
        self:setText("DefLabel", LOC("DEF"))
        self:setText("ExpLabel", LOC("EXP"))
        self:setText("GoldLabel", LOC("GOLD"))
        layoutDirty = true
    end

    local combatSignature = createSignature(self.model._player.infoComp, self.model._player:getCombatRevision())
    local refreshStateRows = localeChanged
    if self._combatSignature ~= combatSignature then
        self._combatSignature = combatSignature
        refreshStateRows = true

        local hpSignature = createSignature(self.model._player.infoComp.HP, self.model._player.infoComp.MAXHP)
        if self._hpSignature ~= hpSignature then
            self._hpSignature = hpSignature
            self:setText(
                "HpValue",
                "#default#" .. tostring(ToShortNumber(self.model._player.infoComp.HP)) .. "/#max#"
                    .. tostring(ToShortNumber(self.model._player.infoComp.MAXHP)) .. "#default#"
            )
            self._hpRate = self.model._player.infoComp.HP / self.model._player.infoComp.MAXHP
            layoutDirty = true
        end

        local statSignature = createSignature(self.model._player.infoComp.ATK, self.model._player.infoComp.DEF)
        if self._statSignature ~= statSignature then
            self._statSignature = statSignature
            self:setText("AtkValue", tostring(ToShortNumber(self.model._player.infoComp.ATK)))
            self:setText("DefValue", tostring(ToShortNumber(self.model._player.infoComp.DEF)))
            layoutDirty = true
        end

        local weakStacks = self.model._player:getStateStackCount(State.Weak)
        local poisonStacks = self.model._player:getStateStackCount(State.Poisoned)
        local stackSignature = createSignature(weakStacks, poisonStacks)
        if self._stackSignature ~= stackSignature then
            self._stackSignature = stackSignature
            local debuffString = weakStacks > 0 and "(-" .. tostring(weakStacks) .. ")" or ""
            self:setText("AtkDebuff", debuffString)
            self:setText("DefDebuff", debuffString)
            self:setText("HpPoison", poisonStacks > 0 and "(" .. tostring(poisonStacks) .. ")" or "")
            layoutDirty = true
        end
    end

    local progressSignature = createSignature(
        self.model._player.infoComp.LEVEL, self.model._player.infoComp.EXP, self.model._player.infoComp.GOLD
    )
    if self._progressSignature ~= progressSignature then
        self._progressSignature = progressSignature
        self:setText("Level", "Lv. " .. tostring(self.model._player.infoComp.LEVEL))
        self:setText("ExpValue", tostring(ToShortNumber(self.model._player.infoComp.EXP)))
        self:setText("GoldValue", tostring(ToShortNumber(self.model._player.infoComp.GOLD)))
        layoutDirty = true
    end

    local keyYCount = self.model._player:getItemCount(Item.KEY_Y)
    local keyBCount = self.model._player:getItemCount(Item.KEY_B)
    local keyRCount = self.model._player:getItemCount(Item.KEY_R)
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
    self._layoutDirty = layoutDirty
end

function PlayerAttrHUDUI:_applyGeometry()
    self._mapNameText:setPosition(sf.Vector2f.new(self._infoStartX, self.model._HEADER_ROW_Y))
    self._levelText:setPosition(sf.Vector2f.new(0.0, self._avatarSize))
    self._stateHost:setPosition(sf.Vector2f.new(0.0, self._avatarSize + self.model._ROW_SHIFT))
    self._hpFill:setSize(sf.Vector2f.new(self._hpBarWidth * self._hpRate, self.model._HP_BAR_HEIGHT))

    local hpBounds = self._hpText:getLocalBounds()
    local textY = self.model._HP_ROW_Y + (self.model._HP_TEXT_LAYOUT_HEIGHT - hpBounds.size.y) / 2.0
        - hpBounds.position.y
    local textX = self.model._STAT_VALUE_X - hpBounds.size.x - hpBounds.position.x
    self._hpLabelText:setPosition(sf.Vector2f.new(0.0, textY))
    self._hpText:setPosition(sf.Vector2f.new(textX, textY))
    self._hpPoisonText:setPosition(sf.Vector2f.new(self.model._STAT_VALUE_X + self.model._DEBUFF_TEXT_OFFSET_X, textY))

    local itemBounds = self._itemText:getLocalBounds()
    local itemX = self.model._STAT_VALUE_X - itemBounds.size.x - itemBounds.position.x
    local keyRowHeight = math.max(self.model._FONT_SIZE, self.model._KEY_ICON_HEIGHT)
    local itemY = self.model._KEY_ROW_Y + (keyRowHeight - itemBounds.size.y) / 2.0 - itemBounds.position.y
    self._itemText:setPosition(sf.Vector2f.new(itemX, itemY))
    local iconY = self.model._KEY_ROW_Y + (keyRowHeight - self.model._KEY_ICON_HEIGHT) / 2.0
    self._keyIcon:setPosition(sf.Vector2f.new(0.0, iconY))
end

function PlayerAttrHUDUI:prepare(logicalSize)
    local root = super(PlayerAttrHUDUI, self).prepare(logicalSize or self._logicalSize)
    self:_applyGeometry()
    self._layoutDirty = false
    return root
end

function PlayerAttrHUDUI:attach(logicalSize)
    self:attachTo(self.model, logicalSize)
end

function PlayerAttrHUDUI:tick()
    self:refresh()
    if not self._layoutDirty then
        return
    end
    self.view:reflow(self._logicalSize)
    self:_applyGeometry()
    self._layoutDirty = false
end

return Ui.define("PlayerAttrHUD", PlayerAttrHUDUI)
