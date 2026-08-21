local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")

local ManagerFunctions = GlobalFunctions.Manager
local GlobalSystem = GlobalCore.System

local PREVIEW_MIN_SCALE = 0.01
local HUE_EPSILON = 0.0001

---@type sf.Shader|nil
local actorHueShader = nil

local Render = {}

local function normaliseSignatureNumber(value)
    local number = value
    if math.abs(number) <= 0.000000001 then
        number = 0.0
    end
    return string.format("%.9g", number)
end

local function getActorHueShader()
    if actorHueShader == nil and sf.Shader.isAvailable() then
        actorHueShader = ManagerFunctions.loadShader("Global/Hue.frag", sf.Shader.Type.Fragment)
    end
    return actorHueShader
end

local function applyActorHueUniform(shader, hue)
    shader:setUniform("screenTex", sf.Shader.CurrentTexture)
    shader:setUniform("hue", hue)
end

---@param buffer sf.RenderTexture|nil
---@param size sf.Vector2u
---@return sf.RenderTexture
local function ensurePreviewBuffer(buffer, size)
    if buffer == nil or buffer:getSize() ~= size then
        buffer = sf.RenderTexture.new(size)
        buffer:setSmooth(false)
    end
    return buffer
end

function Render.UpdateActorPreviewFrame(texture, rect, animatable, switchInterval, switchTimer, deltaTime)
    if not animatable then
        return rect, switchTimer, false
    end
    switchTimer = switchTimer + deltaTime
    if switchTimer < switchInterval then
        return rect, switchTimer, false
    end
    switchTimer = 0.0
    local textureWidth = texture:getSize().x
    if textureWidth <= 0 then
        return rect, switchTimer, false
    end
    local positionX = (rect.position.x + rect.size.x) % textureWidth
    ---@cast positionX integer
    local nextPosition = sf.Vector2i.new(positionX, rect.position.y)
    ---@cast nextPosition sf.Vector2i
    local nextSize = sf.Vector2i.new(rect.size.x, rect.size.y)
    ---@cast nextSize sf.Vector2i
    local nextRect = sf.IntRect.new(nextPosition, nextSize)
    ---@cast nextRect sf.IntRect
    return nextRect, switchTimer, true
end

---@param preview Global.Utils.Render.ActorPreview
---@param deltaTime number
---@return boolean
local function advancePreviewFrame(preview, deltaTime)
    local nextRect, switchTimer, changed = Render.UpdateActorPreviewFrame(
        preview.visual.texture, preview.sourceRect, preview.animatable, preview.switchInterval, preview.switchTimer,
        deltaTime
    )
    preview.sourceRect = nextRect
    preview.switchTimer = switchTimer
    return changed
end

---@param preview Global.Utils.Render.ActorPreview
---@return sf.Texture, sf.IntRect, sf.Color, sf.Shader|nil
local function drawPreviewSource(preview)
    local texture = preview.visual.texture
    local sourceRect = preview.sourceRect
    local composedTexture = texture
    local composedRect = sourceRect
    local composedColour = preview.shaderError and sf.Color.new(255, 0, 255, 255) or sf.Color.White
    local finalShader = nil

    preview._sourceSprite:setTexture(texture, true)
    preview._sourceSprite:setTextureRect(sourceRect)
    preview._sourceSprite:setColor(sf.Color.White)

    if not preview.shaderError and preview.shader ~= nil and preview.hasHue then
        preview._effectBuffer = ensurePreviewBuffer(preview._effectBuffer, preview._sourceSize)
        Render.BindActorShader(preview.shader, texture, sourceRect, preview.shaderTime)
        local shaderStates = sf.RenderStates.new()
        shaderStates.shader = preview.shader
        preview._effectBuffer:clear(sf.Color.Transparent)
        preview._effectBuffer:draw(preview._sourceSprite, shaderStates)
        preview._effectBuffer:display()
        composedTexture = preview._effectBuffer:getTexture()
        composedRect = preview._bufferRect
    elseif not preview.shaderError and preview.shader ~= nil then
        finalShader = preview.shader
    end

    if not preview.shaderError and preview.hasHue then
        preview._hueBuffer = ensurePreviewBuffer(preview._hueBuffer, preview._sourceSize)
        local hueShader = preview._hueShader
        ---@cast hueShader sf.Shader
        applyActorHueUniform(hueShader, preview.hue)
        preview._postSprite:setTexture(composedTexture, true)
        preview._postSprite:setTextureRect(composedRect)
        preview._postSprite:setColor(sf.Color.White)
        local hueBlendMode = preview.shader ~= nil and sf.BlendAlpha or sf.BlendNone
        local hueStates = sf.RenderStates.new(hueBlendMode)
        hueStates.shader = hueShader
        preview._hueBuffer:clear(sf.Color.Transparent)
        preview._hueBuffer:draw(preview._postSprite, hueStates)
        preview._hueBuffer:display()
        composedTexture = preview._hueBuffer:getTexture()
        composedRect = preview._bufferRect
    end

    return composedTexture, composedRect, composedColour, finalShader
end

---@param scaleComponent number
---@return number
local function getDisplayScale(scaleComponent)
    if math.abs(scaleComponent) >= PREVIEW_MIN_SCALE then
        return scaleComponent
    end
    if scaleComponent < 0.0 then
        return -PREVIEW_MIN_SCALE
    end
    return PREVIEW_MIN_SCALE
end

---@param preview Global.Utils.Render.ActorPreview
local function renderActorPreview(preview)
    local composedTexture, composedRect, composedColour, finalShader = drawPreviewSource(preview)
    local scaleX = getDisplayScale(preview.scale.x)
    local scaleY = getDisplayScale(preview.scale.y)
    local displayWidth = math.max(1.0, math.abs(composedRect.size.x * scaleX))
    local displayHeight = math.max(1.0, math.abs(composedRect.size.y * scaleY))
    local fit = math.min(1.0, preview.size / displayWidth, preview.size / displayHeight)

    preview._finalSprite:setTexture(composedTexture, true)
    preview._finalSprite:setTextureRect(composedRect)
    preview._finalSprite:setColor(composedColour)
    preview._finalSprite:setOrigin(sf.Vector2f.new(composedRect.size.x / 2.0, composedRect.size.y / 2.0))
    preview._finalSprite:setScale(sf.Vector2f.new(scaleX * fit, scaleY * fit))
    preview._finalSprite:setPosition(sf.Vector2f.new(preview.size / 2.0, preview.size / 2.0))

    local outputStates = sf.RenderStates.new(sf.BlendNone)
    if finalShader ~= nil then
        Render.BindActorShader(finalShader, preview.visual.texture, preview.sourceRect, preview.shaderTime)
        outputStates.shader = finalShader
    end
    preview._outputBuffer:clear(sf.Color.Transparent)
    preview._outputBuffer:draw(preview._finalSprite, outputStates)
    preview._outputBuffer:display()
    preview.texture:update(preview._outputBuffer:getTexture())
end

function Render.GetRealSize(inSize)
    local realSize = sf.Vector2f.new(inSize.x, inSize.y)
    return realSize * GlobalSystem.getScale()
end

function Render.NormaliseActorHue(hue)
    return hue % 360.0
end

function Render.IsNeutralActorHue(hue)
    local normalisedHue = Render.NormaliseActorHue(hue)
    return normalisedHue <= HUE_EPSILON or math.abs(normalisedHue - 360.0) <= HUE_EPSILON
end

function Render.BindActorShader(shader, texture, rect, time)
    local textureSize = texture:getSize()
    shader:setUniform("texture", sf.Shader.CurrentTexture)
    shader:setUniform("time", time)
    shader:setUniform("textureSize", sf.Vector2f.new(textureSize.x, textureSize.y))
    shader:setUniform("textureRect", sf.Vector4f.new(rect.position.x, rect.position.y, rect.size.x, rect.size.y))
end

function Render.CaptureActorVisual(actor)
    local texture = actor:getTexture()
    local rect = actor:getTextureRect()
    local scale = actor:getScale()
    local shaderError = actor:hasShaderError()
    local shader = nil
    if not shaderError then
        shader = actor:getShader()
    end
    local visualRect = copy(rect)
    return {
        texture = texture,
        texturePath = tostring(actor.texturePath or ""),
        textureNativeHandle = texture:getNativeHandle(),
        rect = visualRect,
        textureRect = visualRect,
        scale = copy(scale),
        shader = shader,
        shaderPath = tostring(actor:getShaderPath() or ""),
        shaderError = shaderError,
        hasShaderError = shaderError,
        hue = Render.NormaliseActorHue(actor.hue or 0.0),
        animatable = actor:getAnimatable(),
        switchInterval = actor.switchInterval
    }
end

function Render.GetActorVisualSignature(enemyID, visual)
    local rect = visual.rect or visual.textureRect
    ---@cast rect sf.IntRect
    local textureHandle = visual.textureNativeHandle or visual.texture:getNativeHandle()
    if bool(visual.animatable) then
        ---@type table<integer, string>
        local values = {
            tostring(enemyID), tostring(visual.texturePath or ""), tostring(textureHandle), tostring(rect.position.y),
            tostring(rect.size.x), tostring(rect.size.y), "animated", tostring(visual.shaderPath or ""),
            normaliseSignatureNumber(Render.NormaliseActorHue(visual.hue or 0.0)), normaliseSignatureNumber(visual.scale.x),
            normaliseSignatureNumber(visual.scale.y)
        }
        return tuple(values)
    end
    ---@type table<integer, string>
    local values = {
        tostring(enemyID), tostring(visual.texturePath or ""), tostring(textureHandle), tostring(rect.position.y),
        tostring(rect.size.x), tostring(rect.size.y), "static", tostring(rect.position.x),
        tostring(visual.shaderPath or ""), normaliseSignatureNumber(Render.NormaliseActorHue(visual.hue or 0.0)),
        normaliseSignatureNumber(visual.scale.x), normaliseSignatureNumber(visual.scale.y)
    }
    return tuple(values)
end

function Render.CreateActorPreview(visual)
    local sourceRect = copy(visual.rect or visual.textureRect)
    ---@cast sourceRect sf.IntRect
    local sourceSize = sf.Vector2u.new(
        math.max(1, math.abs(sourceRect.size.x)), math.max(1, math.abs(sourceRect.size.y))
    )
    ---@cast sourceSize sf.Vector2u
    local previewSize = math.max(1, Engine.CellSize)
    ---@cast previewSize integer
    local outputSize = sf.Vector2u.new(previewSize, previewSize)
    ---@cast outputSize sf.Vector2u
    local outputBuffer = sf.RenderTexture.new(outputSize)
    outputBuffer:setSmooth(false)
    local outputTexture = sf.Texture.new(outputSize)
    outputTexture:setSmooth(false)
    local hue = Render.NormaliseActorHue(visual.hue or 0.0)
    local hueShader = getActorHueShader()
    local outputRect = sf.IntRect.new(0, 0, previewSize, previewSize)
    ---@cast outputRect sf.IntRect
    local bufferRect = sf.IntRect.new(0, 0, sourceSize.x, sourceSize.y)
    ---@cast bufferRect sf.IntRect
    ---@type Global.Utils.Render.ActorPreview
    local preview = {
        visual = visual,
        texture = outputTexture,
        rect = outputRect,
        outputRect = outputRect,
        sourceRect = sourceRect,
        scale = copy(visual.scale),
        shader = visual.shader,
        shaderPath = visual.shaderPath or "",
        shaderError = visual.shaderError == true or visual.hasShaderError == true,
        hue = hue,
        animatable = bool(visual.animatable),
        switchInterval = visual.switchInterval or 0.2,
        switchTimer = 0.0,
        shaderTime = 0.0,
        size = previewSize,
        hasHue = hueShader ~= nil and not Render.IsNeutralActorHue(hue),
        _hueShader = hueShader,
        _sourceSize = sourceSize,
        _bufferRect = bufferRect,
        _sourceSprite = sf.Sprite.new(visual.texture, sourceRect),
        _postSprite = sf.Sprite.new(visual.texture, sourceRect),
        _finalSprite = sf.Sprite.new(visual.texture, sourceRect),
        _effectBuffer = nil,
        _hueBuffer = nil,
        _outputBuffer = outputBuffer
    }
    renderActorPreview(preview)
    return preview
end

function Render.UpdateActorPreview(preview, deltaTime)
    preview.shaderTime = preview.shaderTime + deltaTime
    local frameChanged = advancePreviewFrame(preview, deltaTime)
    local redraw = frameChanged or preview.shader ~= nil
    if redraw then
        renderActorPreview(preview)
    end
    return redraw
end

return Render
