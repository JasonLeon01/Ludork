local Engine = require("Engine")
local Pool = require("Global.Pool")

---@class (partial) DamageTextParticle
local DamageTextParticle = {}

local MOVE_X = 32.0
local DURATION = 0.5
local active = {}

function DamageTextParticle:init(particleSystem, text, position, textConfig, speedCurve, isVisible)
    assert(speedCurve ~= nil, "DamageTextParticle speed curve must not be nil")
    assert(textConfig ~= nil, "DamageTextParticle text config must not be nil")
    self._particleSystem = particleSystem
    self._speedCurve = speedCurve
    self._isVisible = isVisible
    self._textParticle = nil
    self._startPosition = position:copy()
    self._destroyRequested = false
    self._destroyed = false
    local message = tostring(text)
    if not bool(message) then
        self._destroyed = true
        return
    end
    ---@type DamageTextParticle[]
    local selfRef = setmetatable({ self }, { __mode = "v" })
    local textParticle = Engine.TextParticle.new(self._particleSystem, function (_deltaTime, countTime, _particle)
        local instance = selfRef[1]
        if instance ~= nil then
            instance:update(countTime)
        end
    end, 0.0, "", textConfig, true
    )
    textParticle:setString(message)
    self._textParticle = textParticle
    self:_applyPosition(0.0)
    self._particleSystem:addText(textParticle)
    active[#active + 1] = self
end

function DamageTextParticle:destroy()
    if self._destroyed then
        return
    end
    self._destroyed = true
    local particleState = { textParticle = self._textParticle }
    self._textParticle = nil
    if particleState.textParticle ~= nil then
        local parent = particleState.textParticle:getParent()
        if parent ~= nil then
            parent:removeText(particleState.textParticle)
        end
    end
    local index = table.index(active, self)
    if index ~= nil then
        table.remove(active, index)
    end
end

function DamageTextParticle:update(countTime)
    if self._destroyed or self._destroyRequested then
        return
    end
    self:_applyPosition(countTime)
    if countTime >= DURATION then
        self:_requestDestroy()
    end
end

function DamageTextParticle:_requestDestroy()
    if self._destroyRequested then
        return
    end
    self._destroyRequested = true
    if self._textParticle ~= nil then
        self._textParticle:setColour(sf.Color.new(255, 255, 255, 0))
    end
    local eventName = "DamageTextParticle.destroy." .. tostring(self)
    Engine.once(eventName, function (_)
        self:destroy()
    end)
    Engine.post(eventName)
end

---@param countTime number
function DamageTextParticle:_applyPosition(countTime)
    if self._textParticle == nil then
        return
    end
    local xProgress = math.min(1.0, countTime / DURATION)
    local yOffset = self._speedCurve:evaluate(countTime)
    local position = Pool.Get("sf.Vector2f", sf.Vector2f, {
        x = self._startPosition.x + MOVE_X * xProgress,
        y = self._startPosition.y + yOffset
    })
    self._textParticle:setPosition(position)
    if self._isVisible ~= nil then
        self._textParticle:setColour(sf.Color.new(255, 255, 255, self._isVisible() and 255 or 0))
    end
    Pool.Put("sf.Vector2f", position)
end

return class(DamageTextParticle)
