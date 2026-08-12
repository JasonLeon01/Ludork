local Data = require("Source.Data")
local LocaleCore = require("Source.Locale.Core")
---@type { Item: Source.Configs.GeneralEnum.Item, State: Source.Configs.GeneralEnum.State }
local GeneralEnum = require("Source.Configs.GeneralEnum")
local NodeUtils = require("Source.NodeFunctions.Utils")
local IconTexture = require("Source.UI.IconTexture")
local PlayerStateRowUI = require("Source.UI.Parts.PlayerAttrHUD.PlayerStateRow")
local Ui = require("Source.UI.Ui")

---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat
local Item = GeneralEnum.Item
local State = GeneralEnum.State
local ToShortNumber = NodeUtils.ToShortNumber
---@type fun(values: string[]): tuple<string>
local createStateSignature = tuple

---@class Source.UI.PlayerAttrHUD.PlayerAttrHUDUI
local PlayerAttrHUDUI = {}

local function getStateSignature(states)
    ---@type string[]
    local stateIDs = {}
    for index, state in ipairs(states) do
        stateIDs[index] = state.ID
    end
    return createStateSignature(stateIDs)
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
    self._stateIconCache = {}
    self._hpRate = 0.0
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
    ---@cast self._avatar Engine.Button
    self._mapNameText = self:requireControl("MapName")
    ---@cast self._mapNameText Engine.PlainText
    self._levelText = self:requireControl("Level")
    ---@cast self._levelText Engine.PlainText
    self._stateHost = self:requireControl("StateHost")
    ---@cast self._stateHost Engine.Canvas
    self._hpBack = self:requireControl("HpBack")
    ---@cast self._hpBack Engine.SolidRect
    self._hpFill = self:requireControl("HpFill")
    ---@cast self._hpFill Engine.SolidRect
    self._hpLabelText = self:requireControl("HpLabel")
    ---@cast self._hpLabelText Engine.PlainText
    self._hpText = self:requireControl("HpValue")
    ---@cast self._hpText Engine.RichText
    self._atkDebuffText = self:requireControl("AtkDebuff")
    ---@cast self._atkDebuffText Engine.PlainText
    self._defDebuffText = self:requireControl("DefDebuff")
    ---@cast self._defDebuffText Engine.PlainText
    self._hpPoisonText = self:requireControl("HpPoison")
    ---@cast self._hpPoisonText Engine.PlainText
    self._keyIcon = self:requireControl("KeyIcon")
    ---@cast self._keyIcon Engine.Image
    self._itemText = self:requireControl("ItemCounts")
    ---@cast self._itemText Engine.RichText
    self._statValueTexts = {
        ATK = self:requireControl("AtkValue"),
        DEF = self:requireControl("DefValue"),
        EXP = self:requireControl("ExpValue"),
        GOLD = self:requireControl("GoldValue")
    }
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
    local cached = self._stateIconCache[iconPath]
    if cached ~= nil then
        return cached
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
    self.model._stateWidgets = self._stateWidgets
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
        local rowUI = self._stateUIs[index]
        ---@cast rowUI Source.UI.Parts.PlayerAttrHUD.PlayerStateRow.PlayerStateRowUI
        rowUI.model.iconTexture = texture
        rowUI.model.name = state.name
        local rowRoot = rowUI:prepare()
        if self._stateWidgets[index] == nil then
            self._stateHost:addChild(rowRoot)
            self._stateWidgets[index] = rowRoot
        end
        rowRoot:setPosition(sf.Vector2f.new(x, 0.0))
        x = x + rowUI:getWidth() + self.model._STATE_GAP
    end
    self.model._stateWidgets = self._stateWidgets
end

function PlayerAttrHUDUI:refreshStates()
    local states = self.model._player:getStates()
    local signature = getStateSignature(states)
    if not stateSignatureMatches(self, signature) then
        self:_rebuildStateRows(states, signature)
    end
    if not bool(states) then
        return
    end
    self:_updateStateRows(states)
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
    local player = self.model._player
    local hp = player.infoComp.HP
    local maxhp = player.infoComp.MAXHP
    local level = player.infoComp.LEVEL
    local atk = player.infoComp.ATK
    local defence = player.infoComp.DEF
    local exp = player.infoComp.EXP
    local gold = player.infoComp.GOLD

    self:setText("MapName", self:getMapDisplayName())
    self:setText("Level", "Lv. " .. tostring(level))
    self:setText("HpLabel", LOC("HP"))
    self:setText("AtkLabel", LOC("ATK"))
    self:setText("DefLabel", LOC("DEF"))
    self:setText("ExpLabel", LOC("EXP"))
    self:setText("GoldLabel", LOC("GOLD"))
    self:refreshStates()
    self:setText(
        "HpValue",
        "#default#" .. tostring(ToShortNumber(hp)) .. "/#max#" .. tostring(ToShortNumber(maxhp)) .. "#default#"
    )
    self:setText("AtkValue", tostring(ToShortNumber(atk)))
    self:setText("DefValue", tostring(ToShortNumber(defence)))
    self:setText("ExpValue", tostring(ToShortNumber(exp)))
    self:setText("GoldValue", tostring(ToShortNumber(gold)))

    local weakStacks = player:getStateStacks()[State.Weak] or 0
    local debuffString = weakStacks > 0 and "(-" .. tostring(weakStacks) .. ")" or ""
    self:setText("AtkDebuff", debuffString)
    self:setText("DefDebuff", debuffString)

    local poisonStacks = player:getStateStacks()[State.Poisoned] or 0
    self:setText("HpPoison", poisonStacks > 0 and "(" .. tostring(poisonStacks) .. ")" or "")

    self._hpRate = hp / maxhp
    local keyYCount = player:getItemCount(Item.KEY_Y)
    local keyBCount = player:getItemCount(Item.KEY_B)
    local keyRCount = player:getItemCount(Item.KEY_R)
    self:setText(
        "ItemCounts",
        "#Yellow#" .. string.format("%02d", keyYCount) .. "#default#  #Blue#" .. string.format("%02d", keyBCount)
            .. "#default#  #Red#" .. string.format("%02d", keyRCount) .. "#default#"
    )
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
    return root
end

function PlayerAttrHUDUI:attach(logicalSize)
    self:attachTo(self.model, logicalSize)
end

function PlayerAttrHUDUI:tick()
    self:refresh()
    self.view:reflow(self._logicalSize)
    self:_applyGeometry()
end

return Ui.define("PlayerAttrHUD", PlayerAttrHUDUI)
