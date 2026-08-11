local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local Battler = require("Source.Battler")
local GeneralEnum = require("Source.Configs.GeneralEnum")
local Data = require("Source.Data")
local Utils = require("Source.NodeFunctions.Utils")

local Actor = Engine.Actor
local PlainText = Engine.PlainText
local UIFunctions = GlobalFunctions.UI
local DamageType = Battler.DamageType
local Item = GeneralEnum.Item

---@param self Source.EnemyDamageText
local function syncTextDisplayScale(self)
    local displayScale = math.max(Engine.Scale, 0.000001)
    if self._textDisplayScale == displayScale then
        return
    end
    self._textDisplayScale = displayScale
    self._textRenderStates = sf.RenderStates.new()
    local inverseScale = 1.0 / displayScale
    self._textRenderStates.transform:scale(sf.Vector2f.new(inverseScale, inverseScale))
    self._renderDirty = true
end

---@class Source.EnemyDamageText
local EnemyDamageText = {}

EnemyDamageText.DamageHintLevel = { NONE = 0, BATTLE = 1, MAP = 2 }
EnemyDamageText.EnemyDamageHintLevel = EnemyDamageText.DamageHintLevel.BATTLE
EnemyDamageText.tickable = true
EnemyDamageText.collisionEnabled = false
EnemyDamageText.requiredItemID = Item.EnemyBook
EnemyDamageText.textConfig = "Enemy/DamageReadout"
EnemyDamageText.damageTextOffset = sf.Vector2f.new(0.0, 0.0)
EnemyDamageText._blankTexture = nil
EnemyDamageText._scratchRenderTexture = nil
EnemyDamageText._scratchWidth = 0
EnemyDamageText._scratchHeight = 0

function EnemyDamageText:init(_texture, _rect, tag)
    super(EnemyDamageText, self).init(EnemyDamageText._getBlankTexture(), nil, tag)
    self._text = nil
    self._overlayTexture = nil
    self._overlayTextureWidth = 0
    self._overlayTextureHeight = 0
    self._currentDamageText = ""
    self._currentCriticalText = ""
    self._currentDamageColorR = nil
    self._currentDamageColorG = nil
    self._currentDamageColorB = nil
    self._currentDamageColorA = nil
    self._currentOverlayWidth = nil
    self._currentOverlayHeight = nil
    self._currentBattlers = setmetatable({}, {
        __mode = "v"
    })
    self._currentParentRevision = nil
    self._currentPlayerRevision = nil
    self._currentOffsetX = nil
    self._currentOffsetY = nil
    self._overlayVisible = false
    self._renderDirty = true
    self._fillColor = sf.Color.White
    self._textDisplayScale = nil
    self._textRenderStates = sf.RenderStates.new()
    syncTextDisplayScale(self)
    self:setVisible(false, false)
end

function EnemyDamageText:onTick(_deltaTime)
    syncTextDisplayScale(self)
    local player = EnemyDamageText._getPlayer()
    if player == nil then
        self:_setOverlayVisible(false)
        return
    end
    local parent = self:getParent()
    local visible = EnemyDamageText.EnemyDamageHintLevel >= EnemyDamageText.DamageHintLevel.BATTLE and parent ~= nil
        and parent:getVisible() and player:hasItem(self.requiredItemID)
    self:_setOverlayVisible(visible)
    if not visible then
        return
    end
    ---@cast parent Source.Enemy
    self:_updateOverlayPosition()
    local width, height = EnemyDamageText._getParentSize(parent)
    local parentRevision = parent:getCombatRevision()
    local playerRevision = player:getCombatRevision()
    if not self._renderDirty and rawequal(parent, self._currentBattlers[1])
        and rawequal(player, self._currentBattlers[2]) and parentRevision == self._currentParentRevision
        and playerRevision == self._currentPlayerRevision and width == self._currentOverlayWidth
        and height == self._currentOverlayHeight then
        return
    end
    self:_ensureText()
    local damageType, damage = parent:getDamage(player)
    local damageText = damageType == DamageType.UNDEFEATABLE and "???" or tostring(Utils.ToShortNumber(damage))
    local criticalText = EnemyDamageText._formatCriticalText(parent:getCriticalValue(player))
    if self:_setOverlayText(
        damageText, criticalText,
        EnemyDamageText.GetDamageColor(damageType, damage, Engine.ToInteger(player.infoComp.HP)), width, height
    ) then
        self._currentBattlers[1] = parent
        self._currentBattlers[2] = player
        self._currentParentRevision = parentRevision
        self._currentPlayerRevision = playerRevision
    end
end

function EnemyDamageText:_ensureText()
    if self._text ~= nil then
        return
    end
    self._text = PlainText.new(Data.getPlainTextConfig(self.textConfig), "")
    self._text:setColour(self._fillColor)
end

---@param damageText   string
---@param criticalText string
---@param damageColor  sf.Color
---@param width        integer
---@param height       integer
---@return boolean
function EnemyDamageText:_setOverlayText(damageText, criticalText, damageColor, width, height)
    if self._text == nil then
        return false
    end
    if not self._renderDirty and damageText == self._currentDamageText and criticalText == self._currentCriticalText
        and damageColor.r == self._currentDamageColorR and damageColor.g == self._currentDamageColorG
        and damageColor.b == self._currentDamageColorB and damageColor.a == self._currentDamageColorA
        and width == self._currentOverlayWidth and height == self._currentOverlayHeight then
        return true
    end
    if not self:_renderTextTexture(damageText, criticalText, damageColor, width, height) then
        self._renderDirty = true
        return false
    end
    self._currentDamageText = damageText
    self._currentCriticalText = criticalText
    self._currentDamageColorR = damageColor.r
    self._currentDamageColorG = damageColor.g
    self._currentDamageColorB = damageColor.b
    self._currentDamageColorA = damageColor.a
    self._currentOverlayWidth = width
    self._currentOverlayHeight = height
    self._fillColor = damageColor
    self._renderDirty = false
    return true
end

---@param damageText   string
---@param criticalText string
---@param damageColor  sf.Color
---@param width        integer
---@param height       integer
---@return boolean
function EnemyDamageText:_renderTextTexture(damageText, criticalText, damageColor, width, height)
    if self._text == nil then
        return false
    end
    width = Engine.ToInteger(math.max(1, width))
    height = Engine.ToInteger(math.max(1, height))
    local size = sf.Vector2u.new(width, height)
    local overlayTexture = self._overlayTexture
    local replaceTexture = overlayTexture == nil or width ~= self._overlayTextureWidth
        or height ~= self._overlayTextureHeight
    local padding = 2
    ---@cast size sf.Vector2u
    local renderTexture = EnemyDamageText._getScratchRenderTexture(size, width, height)
    if renderTexture == nil then
        error("Failed to resize enemy damage text render texture", 0)
    end
    if not renderTexture:setActive(true) then
        error("Failed to activate enemy damage text render texture", 0)
    end
    renderTexture:clear(sf.Color.Transparent)
    self:_drawText(renderTexture, criticalText, sf.Color.White, width, height, padding, false)
    self:_drawText(renderTexture, damageText, damageColor, width, height, padding, true)
    renderTexture:display()
    local renderedImage = renderTexture:getTexture():copyToImage()
    if replaceTexture then
        overlayTexture = sf.Texture.new(size)
    end
    ---@cast overlayTexture sf.Texture
    overlayTexture:update(renderedImage)
    if replaceTexture then
        self:setOrigin(sf.Vector2f.new(0.0, 0.0))
        self:setTexture(overlayTexture, true)
        self._overlayTexture = overlayTexture
        self._overlayTextureWidth = width
        self._overlayTextureHeight = height
    end
    return true
end

---@param renderTexture sf.RenderTexture
---@param text          string
---@param fillColor     sf.Color
---@param width         integer
---@param height        integer
---@param padding       integer
---@param bottomAligned boolean
function EnemyDamageText:_drawText(renderTexture, text, fillColor, width, height, padding, bottomAligned)
    if not bool(text) or self._text == nil then
        return
    end
    self._text:setString(text)
    local bounds = self._text:getLocalBounds()
    local x = width - padding - bounds.size.x - bounds.position.x
    local y = bottomAligned and height - padding - bounds.size.y - bounds.position.y or padding - bounds.position.y
    local basePosition = sf.Vector2f.new(x, y)
    self._text:setColour(fillColor)
    self._text:setPosition(basePosition)
    renderTexture:draw(self._text, self._textRenderStates)
end

function EnemyDamageText:_clearRenderedTexture()
    if self:getTexture() ~= EnemyDamageText._blankTexture then
        self:setTexture(EnemyDamageText._getBlankTexture(), true)
        self:setOrigin(sf.Vector2f.new(0.0, 0.0))
    end
    self._overlayTexture = nil
    self._overlayTextureWidth = 0
    self._overlayTextureHeight = 0
    self._currentDamageText = ""
    self._currentCriticalText = ""
    self._currentDamageColorR = nil
    self._currentDamageColorG = nil
    self._currentDamageColorB = nil
    self._currentDamageColorA = nil
    self._currentOverlayWidth = nil
    self._currentOverlayHeight = nil
    self._currentBattlers[1] = nil
    self._currentBattlers[2] = nil
    self._currentParentRevision = nil
    self._currentPlayerRevision = nil
    self._renderDirty = true
end

---@param visible boolean
function EnemyDamageText:_setOverlayVisible(visible)
    if self._overlayVisible == visible then
        return
    end
    self._overlayVisible = visible
    self:setVisible(visible, false)
    if not visible then
        self:_clearRenderedTexture()
    end
end

function EnemyDamageText:_updateOverlayPosition()
    local offset = self.damageTextOffset
    if offset.x == self._currentOffsetX and offset.y == self._currentOffsetY then
        return
    end
    self._currentOffsetX = offset.x
    self._currentOffsetY = offset.y
    self:setRelativePosition(offset)
end

---@param parent Engine.Actor | nil
---@return integer, integer
function EnemyDamageText._getParentSize(parent)
    if parent == nil then
        return Engine.CellSize, Engine.CellSize
    end
    local rect = parent:getTextureRect()
    if rect == nil then
        return Engine.CellSize, Engine.CellSize
    end
    return Engine.ToInteger(math.max(1, rect.size.x)), Engine.ToInteger(math.max(1, rect.size.y))
end

---@return Source.Player.Player | nil
function EnemyDamageText._getPlayer()
    local scene = GlobalCore.System.getScene()
    if scene == nil then
        return nil
    end
    ---@cast scene Source.Scenes.SceneMap.SceneMap
    local player = scene.player
    if player == nil and scene.inst ~= nil then
        player = scene.inst:getPlayer()
    end
    return player
end

---@param damageType Source.Battler.DamageType
---@param damage     integer
---@param playerHP   integer
---@return sf.Color
function EnemyDamageText.GetDamageColor(damageType, damage, playerHP)
    if damageType == DamageType.UNDEFEATABLE then
        return UIFunctions.GetDimGrey()
    end
    damage = Engine.ToInteger(math.max(0, damage))
    playerHP = Engine.ToInteger(math.max(0, playerHP))
    if damage == 0 then
        return sf.Color.Green
    elseif playerHP <= 0 then
        return UIFunctions.GetDimGrey()
    elseif damage < playerHP / 4 then
        return sf.Color.White
    elseif damage < playerHP / 2 then
        return sf.Color.Yellow
    elseif damage < playerHP * 3 / 4 then
        return UIFunctions.GetCopper()
    elseif damage < playerHP then
        return sf.Color.Red
    end
    return UIFunctions.GetDimGrey()
end

---@param criticalValue integer
---@return string
function EnemyDamageText._formatCriticalText(criticalValue)
    if criticalValue == -2 then
        return ""
    elseif criticalValue == -1 then
        return "???"
    end
    return tostring(Utils.ToShortNumber(criticalValue))
end

---@return sf.Texture
function EnemyDamageText._getBlankTexture()
    if EnemyDamageText._blankTexture == nil then
        EnemyDamageText._blankTexture = sf.Texture.new(sf.Image.new(sf.Vector2u.new(1, 1), sf.Color.Transparent))
    end
    local blankTexture = EnemyDamageText._blankTexture
    ---@cast blankTexture sf.Texture
    return blankTexture
end

---@param size   sf.Vector2u
---@param width  integer
---@param height integer
---@return sf.RenderTexture | nil
function EnemyDamageText._getScratchRenderTexture(size, width, height)
    local renderTexture = EnemyDamageText._scratchRenderTexture
    if renderTexture == nil or width ~= EnemyDamageText._scratchWidth or height ~= EnemyDamageText._scratchHeight then
        renderTexture = sf.RenderTexture.new(size)
        EnemyDamageText._scratchRenderTexture = renderTexture
        EnemyDamageText._scratchWidth = width
        EnemyDamageText._scratchHeight = height
    end
    return renderTexture
end

return class(EnemyDamageText, Actor)
