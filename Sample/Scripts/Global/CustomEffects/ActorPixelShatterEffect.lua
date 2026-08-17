local DURATION = 0.65
local STAGGER_DURATION = 0.1

local ActorPixelShatterEffect = {}

local function appendVertex(vertices, vertex, x, y, textureCoordinate)
    vertex.position = sf.Vector2f.new(x, y)
    vertex.texCoords = textureCoordinate
    vertices:append(vertex)
end

local function buildVertices(originX, originY, width, height)
    local vertices = sf.VertexArray.new(sf.PrimitiveType.Triangles)
    local vertex = sf.Vertex.new()
    for y = 0, height - 1 do
        for x = 0, width - 1 do
            local left = originX + x
            local top = originY + y
            local right = left + 1
            local bottom = top + 1
            local textureCoordinate = sf.Vector2f.new(x + 0.5, y + 0.5)
            appendVertex(vertices, vertex, left, top, textureCoordinate)
            appendVertex(vertices, vertex, right, top, textureCoordinate)
            appendVertex(vertices, vertex, right, bottom, textureCoordinate)
            appendVertex(vertices, vertex, left, top, textureCoordinate)
            appendVertex(vertices, vertex, right, bottom, textureCoordinate)
            appendVertex(vertices, vertex, left, bottom, textureCoordinate)
        end
    end
    return vertices
end

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
    local sourceActor = self._sourceActor
    assert(sourceActor ~= nil, "ActorPixelShatterEffect source Actor is unavailable")
    local bounds = sourceActor:getGlobalBounds()
    local originX = math.floor(bounds.position.x)
    local originY = math.floor(bounds.position.y)
    local maximumX = math.ceil(bounds.position.x + bounds.size.x)
    local maximumY = math.ceil(bounds.position.y + bounds.size.y)
    local width = math.max(1, maximumX - originX)
    local height = math.max(1, maximumY - originY)
    local snapshotSize = sf.Vector2f.new(width, height)
    local renderTextureSize = sf.Vector2u.new(width, height)
    ---@cast renderTextureSize sf.Vector2u
    local snapshot = sf.RenderTexture.new(renderTextureSize)
    snapshot:setSmooth(false)
    snapshot:setView(sf.View.new(
        sf.Vector2f.new(originX + width * 0.5, originY + height * 0.5),
        snapshotSize
    ))
    snapshot:clear(sf.Color.Transparent)
    drawActor(snapshot, sourceActor)
    snapshot:display()

    local renderStates = sf.RenderStates.new()
    renderStates.texture = snapshot:getTexture()
    renderStates.shader = self._shader
    self._snapshot = snapshot
    self._snapshotOrigin = sf.Vector2f.new(originX, originY)
    self._snapshotSize = snapshotSize
    self._vertices = buildVertices(originX, originY, width, height)
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
    local snapshotOrigin = self._snapshotOrigin
    local snapshotSize = self._snapshotSize
    local vertices = self._vertices
    local renderStates = self._renderStates
    assert(snapshotOrigin ~= nil)
    assert(snapshotSize ~= nil)
    assert(vertices ~= nil)
    assert(renderStates ~= nil)
    self._shader:setUniform("texture", sf.Shader.CurrentTexture)
    self._shader:setUniform("elapsed", self._elapsed)
    self._shader:setUniform("duration", DURATION)
    self._shader:setUniform("staggerDuration", STAGGER_DURATION)
    self._shader:setUniform("snapshotOrigin", snapshotOrigin)
    self._shader:setUniform("snapshotSize", snapshotSize)
    self._shader:setUniform("seed", self._seed)
    target:draw(vertices, renderStates)
end

return class(ActorPixelShatterEffect)
