local Pool = require("Global.Pool")
local Render = require("Global.Utils.Render")

---@class (partial) GameMap
local GameMapRendering = {}

---@param layerKeys        string[]
---@param playerLayerIndex integer
---@return boolean, sf.Vector2i | nil
function GameMapRendering:_preparePlayerCover(layerKeys, playerLayerIndex)
    if self._player == nil or playerLayerIndex == -1 then
        self:_resetTransparentTiles()
        return false, nil
    end
    local playerPosition = self._player:getMapPosition()
    local reusable = self._coverPlayerX == playerPosition.x and self._coverPlayerY == playerPosition.y
        and self._coverPlayerLayerIndex == playerLayerIndex and self._coverAlpha == self.DefaultCoverAlpha
        and self._coverMaterialRevision == self._materialRevision and self._coverLayerStates ~= nil
        and #self._coverLayerStates == #layerKeys
    if reusable then
        ---@cast self._coverLayerStates GameMapCoverLayerState[]
        for index, layerName in ipairs(layerKeys) do
            local layer = self._tilemap:getLayer(layerName)
            ---@cast layer Engine.TileLayer
            ---@diagnostic disable-next-line: need-check-nil
            if self._coverLayerStates[index].layer ~= layer or self._coverLayerStates[index].visible ~= layer.visible then
                reusable = false
                break
            end
        end
    end
    if reusable then
        return false, playerPosition
    end
    self:_resetTransparentTiles()
    self._coverPlayerX = playerPosition.x
    self._coverPlayerY = playerPosition.y
    self._coverPlayerLayerIndex = playerLayerIndex
    self._coverAlpha = self.DefaultCoverAlpha
    self._coverMaterialRevision = self._materialRevision
    self._coverLayerStates = {}
    for index, layerName in ipairs(layerKeys) do
        local layer = self._tilemap:getLayer(layerName)
        ---@cast layer Engine.TileLayer
        self._coverLayerStates[index] = { layer = layer, visible = layer.visible }
    end
    return true, playerPosition
end

function GameMapRendering:_resetTransparentTiles()
    for _, item in ipairs(self._transparentTiles) do
        if item[1].resetTileColor ~= nil then
            item[1]:resetTileColor(item[2], item[3])
        end
    end
    self._transparentTiles = {}
    self._coverLayerStates = nil
    self._coverPlayerX = nil
    self._coverPlayerY = nil
    self._coverPlayerLayerIndex = nil
    self._coverAlpha = nil
    self._coverMaterialRevision = nil
end

---@param layerKeys string[]
---@return integer
function GameMapRendering:_getPlayerLayerIndex(layerKeys)
    if self._player == nil then
        return -1
    end
    for index, name in ipairs(layerKeys) do
        if table.contains(self._actors[name] or {}, self._player) then
            return index - 1
        end
    end
    return -1
end

---@param layer            Engine.TileLayer
---@param layerIndex       integer
---@param playerLayerIndex integer
---@param playerPosition   sf.Vector2i
function GameMapRendering:_applyPlayerCover(layer, layerIndex, playerLayerIndex, playerPosition)
    if self._player == nil or layerIndex <= playerLayerIndex or playerLayerIndex == -1 then
        return
    end
    if layer:get(playerPosition) == nil then
        return
    end
    if layer.floodFillTransparent ~= nil then
        for _, position in ipairs(
            layer:floodFillTransparent(playerPosition.x, playerPosition.y, self._playerCoverColour)
        ) do
            self._transparentTiles[#self._transparentTiles + 1] = { layer, position.x, position.y }
        end
    elseif layer.setTileColor ~= nil then
        layer:setTileColor(playerPosition.x, playerPosition.y, self._playerCoverColour)
        self._transparentTiles[#self._transparentTiles + 1] = { layer, playerPosition.x, playerPosition.y }
    end
end

---@param target           sf.RenderTarget
---@param states           sf.RenderStates
---@param layerName        string
---@param layerIndex       integer
---@param playerLayerIndex integer
---@param applyPlayerCover boolean
function GameMapRendering:_drawLayerActors(target, states, layerName, layerIndex, playerLayerIndex, applyPlayerCover)
    for _, actor in ipairs(self._actors[layerName] or {}) do
        if self._actorPixelShatterByActor[actor] == nil then
            local actorAlpha = 255
            if applyPlayerCover and self._player ~= nil and layerIndex > playerLayerIndex and playerLayerIndex ~= -1
                and actor ~= self._player and actor:intersects(self._player) then
                actorAlpha = self.DefaultCoverAlpha
            end
            ---@cast actorAlpha integer
            self:_drawActor(target, states, actor, actorAlpha)
        end
    end
    self:_drawActorPixelShatterEffects(target, layerName)
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

---@param target     sf.RenderTarget
---@param states     sf.RenderStates
---@param actor      Engine.Actor
---@param actorAlpha integer
function GameMapRendering:_drawActor(target, states, actor, actorAlpha)
    local hue = Render.NormaliseActorHue(actor.hue or 0.0)
    local hasHue = self._actorHueShader ~= nil and not Render.IsNeutralActorHue(hue)
    local hasShaderError = actor:hasShaderError()
    local actorColour = Pool.Get("sf.Color", sf.Color, {
        r = 255,
        g = hasShaderError and 0 or 255,
        b = 255,
        a = actorAlpha
    })
    if hasShaderError then
        actor:setColor(actorColour)
        Pool.Put("sf.Color", actorColour)
        target:draw(actor, states)
        return
    end
    actor:setColor(actorColour)
    Pool.Put("sf.Color", actorColour)
    local actorShader = actor:getShader()
    if actorShader ~= nil then
        local texture = actor:getTexture()
        ---@cast texture sf.Texture
        Render.BindActorShader(actorShader, texture, actor:getTextureRect(), self._shaderTime)
        if hasHue and self:_drawActorShaderWithHue(target, actor, actorShader, hue, actorAlpha) then
            return
        end
        local renderStates = sf.RenderStates.new()
        renderStates.shader = actorShader
        target:draw(actor, renderStates)
        return
    end
    if hasHue then
        self:_applyActorHueUniform(hue)
        local renderStates = sf.RenderStates.new(states.blendMode)
        renderStates.transform = states.transform
        renderStates.texture = states.texture
        renderStates.shader = self._actorHueShader
        target:draw(actor, renderStates)
        return
    end
    target:draw(actor, states)
end

---@param target      sf.RenderTarget
---@param actor       Engine.Actor
---@param actorShader sf.Shader
---@param hue         number
---@param actorAlpha  integer
---@return boolean
function GameMapRendering:_drawActorShaderWithHue(target, actor, actorShader, hue, actorAlpha)
    if self._actorHueShader == nil then
        return false
    end
    local texture = actor:getTexture()
    ---@cast texture sf.Texture
    local rect = actor:getTextureRect()
    local size = Pool.Get("sf.Vector2u", sf.Vector2u, {
        x = math.max(1, math.floor(rect.size.x)),
        y = math.max(1, math.floor(rect.size.y))
    })
    local shaderBuffer = self:_ensureActorShaderBuffer(size)
    local hueBuffer = self:_ensureActorHueBuffer(size)
    Pool.Put("sf.Vector2u", size)
    local localSprite = sf.Sprite.new(texture, rect)
    local actorColour = Pool.Get("sf.Color", sf.Color, {
        r = 255,
        g = 255,
        b = 255,
        a = actorAlpha
    })
    localSprite:setColor(actorColour)
    Pool.Put("sf.Color", actorColour)
    local shaderStates = sf.RenderStates.new()
    shaderStates.shader = actorShader
    shaderBuffer:clear(sf.Color.Transparent)
    shaderBuffer:draw(localSprite, shaderStates)
    shaderBuffer:display()
    self:_applyActorHueUniform(hue)
    local hueStates = sf.RenderStates.new()
    hueStates.shader = self._actorHueShader
    local sourceSprite = self:_ensureActorHueSourceSprite(shaderBuffer:getTexture())
    sourceSprite:setTexture(shaderBuffer:getTexture(), true)
    sourceSprite:setColor(sf.Color.White)
    hueBuffer:clear(sf.Color.Transparent)
    hueBuffer:draw(sourceSprite, hueStates)
    hueBuffer:display()
    local resultSprite = sf.Sprite.new(hueBuffer:getTexture())
    local renderStates = sf.RenderStates.new()
    renderStates.transform = renderStates.transform:combine(actor:getTransform())
    target:draw(resultSprite, renderStates)
    return true
end

---@param size sf.Vector2u
---@return sf.RenderTexture
function GameMapRendering:_ensureActorShaderBuffer(size)
    if self._actorShaderBuffer == nil or self._actorShaderBuffer:getSize() ~= size then
        self._actorShaderBuffer = sf.RenderTexture.new(size)
    end
    return self._actorShaderBuffer
end

---@param size sf.Vector2u
---@return sf.RenderTexture
function GameMapRendering:_ensureActorHueBuffer(size)
    if self._actorHueBuffer == nil or self._actorHueBuffer:getSize() ~= size then
        self._actorHueBuffer = sf.RenderTexture.new(size)
    end
    return self._actorHueBuffer
end

---@param texture sf.Texture
---@return sf.Sprite
function GameMapRendering:_ensureActorHueSourceSprite(texture)
    if self._actorHueSourceSprite == nil then
        self._actorHueSourceSprite = sf.Sprite.new(texture)
    end
    return self._actorHueSourceSprite
end

---@param hue number
function GameMapRendering:_applyActorHueUniform(hue)
    if self._actorHueShader ~= nil then
        self._actorHueShader:setUniform("screenTex", sf.Shader.CurrentTexture)
        self._actorHueShader:setUniform("hue", hue)
    end
end

return class(GameMapRendering)
