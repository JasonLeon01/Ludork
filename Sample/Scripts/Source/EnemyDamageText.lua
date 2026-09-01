local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local GameplayEventData = require("Global.Gameplay.GameplayEventData")
---@type { Item: Source.Configs.GeneralEnum.Item }
local GeneralEnum = require("Source.Configs.GeneralEnum")
local Data = require("Source.Data")
local EnemyText = require("Source.EnemyText")
local MotaBattleAbility = require("Source.Gameplay.MotaBattleAbility")
local Utils = require("Source.NodeFunctions.Utils")

local Actor = Engine.Actor
local PlainText = Engine.PlainText
local UIFunctions = GlobalFunctions.UI
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

---@type function
local getParentSize
---@type function
local getPlayer
---@type function
local getBlankTexture
---@type function
local getScratchRenderTexture
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
    super(EnemyDamageText, self).init(getBlankTexture(), nil, tag)
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
    local player = getPlayer()
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
    local width, height = getParentSize(parent)
    ---@cast width integer
    ---@cast height integer
    local parentRevision = parent:getAbilitySystemComponent():getRevision()
    local playerRevision = player:getAbilitySystemComponent():getRevision()
    if not self._renderDirty and rawequal(parent, self._currentBattlers[1])
        and rawequal(player, self._currentBattlers[2]) and parentRevision == self._currentParentRevision
        and playerRevision == self._currentPlayerRevision and width == self._currentOverlayWidth
        and height == self._currentOverlayHeight then
        return
    end
    self:_ensureText()
    local battleResult = MotaBattleAbility
        .new()
        :calculate(parent:getAbilitySystemComponent(), GameplayEventData.new({ target = player }))
    ---@type integer | nil
    local damage = nil
    if battleResult.code ~= MotaBattleAbility.BattleResult.CANNOT_DAMAGE then
        damage = battleResult.data.damage
    end
    local damageText = damage == nil and "???" or tostring(Utils.ToShortNumber(damage))
    local criticalText = EnemyText.FormatCritical(MotaBattleAbility.CalculateCriticalValue(parent, player))
    local playerHP = player.attributes.HP
    ---@cast playerHP integer
    if self:_setOverlayText(damageText, criticalText, EnemyDamageText.GetDamageColor(damage, playerHP), width, height) then
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
    self._text = PlainText.new(Data.GetPlainTextConfig(self.textConfig), "")
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
    local replaceTexture = self._overlayTexture == nil or width ~= self._overlayTextureWidth
        or height ~= self._overlayTextureHeight
    local overlayTexture = not replaceTexture and self._overlayTexture or nil
    local padding = 2
    ---@cast size sf.Vector2u
    local renderTexture = getScratchRenderTexture(size, width, height)
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
        self:setTexture(getBlankTexture(), true)
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
    if self.damageTextOffset.x == self._currentOffsetX and self.damageTextOffset.y == self._currentOffsetY then
        return
    end
    self._currentOffsetX = self.damageTextOffset.x
    self._currentOffsetY = self.damageTextOffset.y
    self:setRelativePosition(self.damageTextOffset)
end

---@param parent Engine.Actor | nil
---@return integer, integer
function getParentSize(parent)
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
function getPlayer()
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

---@param damage   integer | nil
---@param playerHP integer
---@return sf.Color
function EnemyDamageText.GetDamageColor(damage, playerHP)
    if damage == nil then
        return UIFunctions.GetDimGrey()
    end
    damage = math.max(0, damage)
    playerHP = math.max(0, playerHP)
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

---@return sf.Texture
function getBlankTexture()
    if EnemyDamageText._blankTexture == nil then
        local size = sf.Vector2u.new(1, 1)
        ---@cast size sf.Vector2u
        EnemyDamageText._blankTexture = sf.Texture.new(sf.Image.new(size, sf.Color.Transparent))
    end
    local blankTexture = EnemyDamageText._blankTexture
    ---@cast blankTexture sf.Texture
    return blankTexture
end

---@param size   sf.Vector2u
---@param width  integer
---@param height integer
---@return sf.RenderTexture | nil
function getScratchRenderTexture(size, width, height)
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
