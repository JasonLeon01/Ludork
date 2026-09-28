local Render = require("Global.Utils.Render")

local GameMapRendering = {}

---@param self GameMapImplState
function GameMapRendering.ResetTransparentTiles(self)
    if self._renderer ~= nil then
        self._renderer:resetTransparentTiles()
    end
end

---@param target     sf.RenderTarget
---@param states     sf.RenderStates
---@param actor      Engine.Actor
---@param actorAlpha integer
---@param self       GameMapImplState
function GameMapRendering.DrawActor(self, target, states, actor, actorAlpha)
    assert(self._renderer ~= nil, "GameMap renderer is unavailable")
    self._renderer:drawActor(target, states, actor, actorAlpha, self._shaderTime)
end

---@param actor  Engine.Actor
---@param hidden boolean
---@param self   GameMapImplState
function GameMapRendering.SetActorEffectHidden(self, actor, hidden)
    if self._renderer ~= nil then
        self._renderer:setActorEffectHidden(actor, hidden)
    end
end

---@param self GameMapImplState
function GameMapRendering.PrepareActorPixelShatterEffects(self)
    local function drawActor(snapshotTarget, actor)
        self:_drawActor(snapshotTarget, sf.RenderStates.new(), actor, 255)
    end
    Render.PrepareActorPixelShatterEffects(self._actorPixelShatterEffects, drawActor)
end

---@param target    sf.RenderTarget
---@param layerName string
---@param self      GameMapImplState
function GameMapRendering.DrawActorPixelShatterEffects(self, target, layerName)
    if not bool(self._actorPixelShatterEffects[layerName]) then
        return
    end
    for _, effect in ipairs(self._actorPixelShatterEffects[layerName]) do
        local visible = not effect:isFinished()
        for _, position in ipairs(effect:getSourcePositions()) do
            if not self:isCellVisible(position) then
                visible = false
                break
            end
        end
        if visible then
            effect:draw(target)
        end
    end
end

return GameMapRendering
