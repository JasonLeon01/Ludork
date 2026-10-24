local cjson = require("cjson")
local GlobalCore = require("GlobalCore")

local SIGNATURE_NUMBER_ZERO_TOLERANCE = 0.000000001

local Display = GlobalCore.Display

local Render = {}

local function normaliseSignatureNumber(value)
    local number = value
    if math.abs(number) <= SIGNATURE_NUMBER_ZERO_TOLERANCE then
        number = 0.0
    end
    return string.format("%.9g", number)
end

function Render.GetRealSize(inSize)
    local realSize = sf.Vector2f.new(inSize.x, inSize.y)
    return realSize * Display.getScale()
end

local function materialParameterSignature(parameters)
    local values = {}
    for _, key in ipairs(table.orderedStringKeys(parameters)) do
        values[#values + 1] = cjson.encode(key) .. ":" .. cjson.encode(parameters[key])
    end
    return table.concat(values, ",")
end

function Render.CaptureActorVisual(actor)
    local texture = actor:getTexture()
    local rect = actor:getTextureRect()
    local scale = actor:getScale()
    local visualRect = rect:copy()
    return {
        texture = texture,
        texturePath = tostring(actor.texturePath or ""),
        textureNativeHandle = texture:getNativeHandle(),
        rect = visualRect,
        textureRect = visualRect,
        scale = scale:copy(),
        material = actor:getMaterial(),
        materialParameters = deepcopy(actor.materialParameters),
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
            tostring(rect.size.x), tostring(rect.size.y), "animated", visual.material,
            materialParameterSignature(visual.materialParameters), normaliseSignatureNumber(visual.scale.x),
            normaliseSignatureNumber(visual.scale.y)
        }
        return tuple(values)
    end
    ---@type table<integer, string>
    local values = {
        tostring(enemyID), tostring(visual.texturePath or ""), tostring(textureHandle), tostring(rect.position.y),
        tostring(rect.size.x), tostring(rect.size.y), "static", tostring(rect.position.x), visual.material,
        materialParameterSignature(visual.materialParameters), normaliseSignatureNumber(visual.scale.x),
        normaliseSignatureNumber(visual.scale.y)
    }
    return tuple(values)
end

function Render.PrepareActorPixelShatterEffects(effectsByLayer, drawActor)
    for _, effects in pairs(effectsByLayer) do
        for _, effect in ipairs(effects) do
            if not effect:isPrepared() then
                effect:prepare(drawActor)
            end
        end
    end
end

return Render
