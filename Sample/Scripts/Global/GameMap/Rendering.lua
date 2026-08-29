---@type GameMapImplState
local GameMapRendering = {}

function GameMapRendering:_resetTransparentTiles()
    if self._renderer ~= nil then
        self._renderer:resetTransparentTiles()
    end
end

---@param target     sf.RenderTarget
---@param states     sf.RenderStates
---@param actor      Engine.Actor
---@param actorAlpha integer
function GameMapRendering:_drawActor(target, states, actor, actorAlpha)
    assert(self._renderer ~= nil, "GameMap renderer is unavailable")
    self._renderer:drawActor(target, states, actor, actorAlpha, self._shaderTime)
end

---@param actor  Engine.Actor
---@param hidden boolean
function GameMapRendering:_setActorEffectHidden(actor, hidden)
    if self._renderer ~= nil then
        self._renderer:setActorEffectHidden(actor, hidden)
    end
end

function GameMapRendering:_prepareActorPixelShatterEffects()
    local function drawActor(snapshotTarget, actor)
        self:_drawActor(snapshotTarget, sf.RenderStates.new(), actor, 255)
    end
    for _, effects in pairs(self._actorPixelShatterEffects) do
        for _, effect in ipairs(effects) do
            if not effect:isPrepared() then
                effect:prepare(drawActor)
            end
        end
    end
end

---@param target    sf.RenderTarget
---@param layerName string
function GameMapRendering:_drawActorPixelShatterEffects(target, layerName)
    if not bool(self._actorPixelShatterEffects[layerName]) then
        return
    end
    for _, effect in ipairs(self._actorPixelShatterEffects[layerName]) do
        if not effect:isFinished() then
            effect:draw(target)
        end
    end
end

return GameMapRendering
