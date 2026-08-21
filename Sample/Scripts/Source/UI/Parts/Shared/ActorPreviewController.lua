local Render = require("Global.Utils.Render")

local _DEFAULT_SWITCH_INTERVAL = 0.2
local _MIN_SCALE = 0.01

---@class Source.UI.Parts.Shared.ActorPreviewController
local ActorPreviewController = {}

function ActorPreviewController:init(imageControl)
    self._imageControl = imageControl
    self:clear()
end

function ActorPreviewController:_syncPreviewState()
    local preview = assert(self._preview, "Actor preview is not available")
    self._displayTexture = preview.texture
    self._displayRect = preview.rect
    self._rect = copy(preview.sourceRect)
    self._animatable = preview.animatable
    self._switchInterval = preview.switchInterval
    self._switchTimer = preview.switchTimer
end

function ActorPreviewController:_assignDisplay()
    self._visible = self._displayTexture ~= nil and self._displayRect ~= nil
    if self._visible then
        ---@cast self._displayTexture sf.Texture
        ---@cast self._displayRect sf.IntRect
        self._imageControl:setTexture(self._displayTexture, true)
        self._imageControl:setTextureRect(self._displayRect)
    end
    self._imageControl:setVisible(self._visible)
end

function ActorPreviewController:setEntry(entry)
    self:clear()
    self._entry = entry
    if entry.visual ~= nil then
        self._preview = Render.CreateActorPreview(entry.visual)
    end
    self._texture = entry.texture
    if entry.rect ~= nil then
        self._rect = copy(entry.rect)
    end
    if entry.scale ~= nil then
        self._scale = copy(entry.scale)
    end
    self._animatable = entry.animatable
    self._switchInterval = entry.switchInterval
    self._displayTexture = self._texture
    self._displayRect = self._rect
    if self._preview ~= nil then
        self:_syncPreviewState()
    end
    self:_assignDisplay()
end

function ActorPreviewController:clear()
    self._entry = nil
    self._preview = nil
    self._texture = nil
    self._rect = nil
    self._scale = sf.Vector2f.new(1.0, 1.0)
    self._displayTexture = nil
    self._displayRect = nil
    self._animatable = false
    self._switchInterval = _DEFAULT_SWITCH_INTERVAL
    self._switchTimer = 0.0
    self._visible = false
    self._imageControl:setScale(sf.Vector2f.new(1.0, 1.0))
    self._imageControl:setPosition(sf.Vector2f.new(0.0, 0.0))
    self._imageControl:setVisible(false)
end

function ActorPreviewController:layout(bounds, verticalAlignment)
    if not self._visible then
        return sf.Vector2f.new(0.0, 0.0)
    end
    local displayRect = assert(self._displayRect)
    local displayScale = sf.Vector2f.new(1.0, 1.0)
    if self._preview == nil then
        displayScale = sf.Vector2f.new(
            math.max(_MIN_SCALE, math.abs(self._scale.x)), math.max(_MIN_SCALE, math.abs(self._scale.y))
        )
    end
    local displayWidth = math.max(1.0, displayRect.size.x * displayScale.x)
    local displayHeight = math.max(1.0, displayRect.size.y * displayScale.y)
    local fit = math.min(1.0, bounds.size.x / displayWidth, bounds.size.y / displayHeight)
    displayScale = sf.Vector2f.new(displayScale.x * fit, displayScale.y * fit)
    displayWidth = displayRect.size.x * displayScale.x
    displayHeight = displayRect.size.y * displayScale.y
    local positionX = bounds.position.x + (bounds.size.x - displayWidth) / 2.0
    local positionY = bounds.position.y
    if verticalAlignment ~= "top" then
        positionY = positionY + (bounds.size.y - displayHeight) / 2.0
    end
    self._imageControl:setScale(displayScale)
    self._imageControl:setPosition(sf.Vector2f.new(positionX, positionY))
    return sf.Vector2f.new(displayWidth, displayHeight)
end

function ActorPreviewController:tick(deltaTime)
    if self._preview ~= nil then
        Render.UpdateActorPreview(self._preview, deltaTime)
        self:_syncPreviewState()
        self:_assignDisplay()
        return
    end
    if not self._animatable or self._texture == nil or self._rect == nil then
        return
    end
    local nextRect, switchTimer, changed = Render.UpdateActorPreviewFrame(
        self._texture, self._rect, self._animatable, self._switchInterval, self._switchTimer, deltaTime
    )
    self._rect = nextRect
    self._switchTimer = switchTimer
    if not changed then
        return
    end
    self._displayRect = self._rect
    self._imageControl:setTextureRect(self._rect)
end

function ActorPreviewController:getState()
    return {
        preview = self._preview,
        texture = self._texture,
        rect = self._rect,
        scale = self._scale,
        displayTexture = self._displayTexture,
        displayRect = self._displayRect,
        animatable = self._animatable,
        switchInterval = self._switchInterval,
        switchTimer = self._switchTimer,
        visible = self._visible
    }
end

return class(ActorPreviewController)
