local GlobalCore = require("GlobalCore")

local GlobalSystem = GlobalCore.System

local HUE_EPSILON = 0.0001

local Render = {}

local function normaliseSignatureNumber(value)
    local number = value
    if math.abs(number) <= 0.000000001 then
        number = 0.0
    end
    return string.format("%.9g", number)
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
            normaliseSignatureNumber(Render.NormaliseActorHue(visual.hue or 0.0)),
            normaliseSignatureNumber(visual.scale.x), normaliseSignatureNumber(visual.scale.y)
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

return Render
