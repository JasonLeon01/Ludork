local DURATION = 0.65
local STAGGER_DURATION = 0.1

local ActorPixelShatterEffect = {}

function ActorPixelShatterEffect:init(actor, shader, seed)
    self._sourceActor = actor
    self._shader = shader
    self._seed = seed
    self._elapsed = 0.0
    self._prepared = false
    self._snapshot = nil
    self._snapshotOrigin = nil
    self._snapshotSize = nil
    self._vertices = nil
    self._renderStates = nil
end

function ActorPixelShatterEffect:getSourceActor()
    return self._sourceActor
end

function ActorPixelShatterEffect:isPrepared()
    return self._prepared
end

function ActorPixelShatterEffect:prepare(drawActor)
    if bool(self._prepared) then
        return
    end
    assert(self._sourceActor ~= nil, "ActorPixelShatterEffect source Actor is unavailable")
    local bounds = self._sourceActor:getGlobalBounds()
    local originX = math.floor(bounds.position.x)
    local originY = math.floor(bounds.position.y)
    local maximumX = math.ceil(bounds.position.x + bounds.size.x)
    local maximumY = math.ceil(bounds.position.y + bounds.size.y)
    local width = math.max(1, maximumX - originX)
    local height = math.max(1, maximumY - originY)
    local snapshotSize = sf.Vector2f.new(width, height)
    local renderTextureSize = sf.Vector2u.new(width, height)
    local snapshotOrigin = sf.Vector2f.new(originX, originY)
    ---@cast renderTextureSize sf.Vector2u
    local snapshot = sf.RenderTexture.new(renderTextureSize)
    snapshot:setSmooth(false)
    snapshot:setView(sf.View.new(sf.Vector2f.new(originX + width * 0.5, originY + height * 0.5), snapshotSize))
    snapshot:clear(sf.Color.Transparent)
    drawActor(snapshot, self._sourceActor)
    snapshot:display()

    local renderStates = sf.RenderStates.new()
    renderStates.texture = snapshot:getTexture()
    renderStates.shader = self._shader
    self._snapshot = snapshot
    self._snapshotOrigin = snapshotOrigin
    self._snapshotSize = snapshotSize
    self._vertices = Engine.BuildPixelGridVertices(snapshotOrigin, renderTextureSize)
    self._renderStates = renderStates
    self._prepared = true
    self._sourceActor = nil
end

function ActorPixelShatterEffect:onTick(deltaTime)
    if not bool(self._prepared) then
        return
    end
    self._elapsed = math.min(DURATION, self._elapsed + deltaTime)
end

function ActorPixelShatterEffect:isFinished()
    return bool(self._prepared) and self._elapsed >= DURATION
end

function ActorPixelShatterEffect:draw(target)
    if not bool(self._prepared) or self:isFinished() then
        return
    end
    assert(self._snapshotOrigin ~= nil)
    assert(self._snapshotSize ~= nil)
    assert(self._vertices ~= nil)
    assert(self._renderStates ~= nil)
    self._shader:setUniform("texture", sf.Shader.CurrentTexture)
    self._shader:setUniform("elapsed", self._elapsed)
    self._shader:setUniform("duration", DURATION)
    self._shader:setUniform("staggerDuration", STAGGER_DURATION)
    self._shader:setUniform("snapshotOrigin", self._snapshotOrigin)
    self._shader:setUniform("snapshotSize", self._snapshotSize)
    self._shader:setUniform("seed", self._seed)
    target:draw(self._vertices, self._renderStates)
end

return class(ActorPixelShatterEffect)
