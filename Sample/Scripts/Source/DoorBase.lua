local Engine = require("Engine")
local GlobalFunctions = require("GlobalFunctions")

local Actor = Engine.Actor
local SoundFilter = Engine.SoundFilter
local ManagerFunctions = GlobalFunctions.Manager

local LATENT_STARTED = 0
local LATENT_FINISHED = 1

--- Condition callable polled by LatentManager for door animation latents.
---@class Source.DoorBase.DoorAnimationCondition
---@field _door           Source.DoorBase.DoorBase
---@field _finishedAttr   string
---@field _startedEmitted boolean
---@field _finished       boolean
---@param door         Source.DoorBase.DoorBase
---@param finishedAttr string
---@return Source.DoorBase.DoorAnimationCondition
local function newDoorAnimationCondition(door, finishedAttr)
    local condition = { _door = door, _finishedAttr = finishedAttr, _startedEmitted = false, _finished = false }
    ---@return boolean
    function condition:isFinished()
        return self._finished
    end
    return setmetatable(condition, {
        __call = function (self)
            if self._finished then
                return { LATENT_FINISHED }
            end
            if not self._startedEmitted then
                self._startedEmitted = true
                return { LATENT_STARTED }
            end
            if self._door[self._finishedAttr] then
                self._finished = true
                return { LATENT_FINISHED }
            end
            return {}
        end
    })
end

---@class Source.DoorBase.DoorBase: Engine.Actor
local DoorBase = {}

DoorBase.collisionEnabled = true
DoorBase.tickable = true
DoorBase.openInterval = 0.05
DoorBase.gateSE = ""
DoorBase.opening = false
DoorBase.closing = false

function DoorBase:init(texture, rect, tag)
    super(DoorBase, self).init(texture, rect, tag)
    self._frameIndex = 0
    self._animTimer = 0.0
    self._openFinished = false
    self._closeFinished = false
    self._frameWidth = 0
    self._startX = 0
    self._startY = 0
    self:_captureClosedFrameLayout()
end

function DoorBase:setTextureRect(rect)
    super(DoorBase, self).setTextureRect(rect)
    if not self.opening and not self.closing then
        self:_captureClosedFrameLayout(rect)
    end
end

function DoorBase:openDoor()
    if self._openFinished or self:isDestroyed() or self.opening then
        local condition = newDoorAnimationCondition(self, "_openFinished")
        condition._finished = true
        return condition
    end
    if self.closing then
        self.closing = false
        self._closeFinished = false
    end
    self:_playGateSE()
    self.opening = true
    self.closing = false
    self._frameIndex = 0
    self._animTimer = 0.0
    self._openFinished = false
    self._closeFinished = false
    self:_advanceToFrame(0)
    return newDoorAnimationCondition(self, "_openFinished")
end

function DoorBase:closeDoor()
    if self:isDestroyed() or self._openFinished or self.closing then
        local condition = newDoorAnimationCondition(self, "_closeFinished")
        condition._finished = true
        return condition
    end
    if self.opening then
        self.opening = false
        self._openFinished = false
    end
    self:_resolveFrameLayout()
    local currentIndex = self:_getCurrentFrameIndex()
    if currentIndex <= 0 then
        currentIndex = self:_resolveClosingFrameIndex()
    end
    ---@cast currentIndex integer
    if currentIndex <= 0 then
        local condition = newDoorAnimationCondition(self, "_closeFinished")
        condition._finished = true
        return condition
    end
    self:_playGateSE()
    self.closing = true
    self.opening = false
    self._frameIndex = currentIndex
    self._animTimer = 0.0
    self._closeFinished = false
    self._openFinished = false
    return newDoorAnimationCondition(self, "_closeFinished")
end

function DoorBase:onTick(deltaTime)
    if self.opening then
        self:_tickOpen(deltaTime)
    elseif self.closing then
        self:_tickClose(deltaTime)
    end
end

---@param deltaTime number
function DoorBase:_tickOpen(deltaTime)
    local frameCount = self:_getFrameCount()
    self._animTimer = self._animTimer + deltaTime
    while self._animTimer >= self.openInterval do
        self._animTimer = self._animTimer - self.openInterval
        local nextFrameIndex = self._frameIndex + 1
        ---@cast nextFrameIndex integer
        self._frameIndex = nextFrameIndex
        if self._frameIndex >= frameCount then
            self:_finishOpening()
            return
        end
        self:_advanceToFrame(self._frameIndex)
    end
end

---@param deltaTime number
function DoorBase:_tickClose(deltaTime)
    self._animTimer = self._animTimer + deltaTime
    while self._animTimer >= self.openInterval do
        self._animTimer = self._animTimer - self.openInterval
        local nextFrameIndex = self._frameIndex - 1
        ---@cast nextFrameIndex integer
        self._frameIndex = nextFrameIndex
        if self._frameIndex <= 0 then
            self:_advanceToFrame(0)
            self:_finishClosing()
            return
        end
        self:_advanceToFrame(self._frameIndex)
    end
end

function DoorBase:_playGateSE()
    local position = self:getPosition()
    ManagerFunctions.playSE(
        self.gateSE,
        SoundFilter.new({
            spatial = true,
            position = sf.Vector3f.new(position.x, position.y, 0.0),
            relativeToListener = false
        })
    )
end

---@param rect sf.IntRect | nil
function DoorBase:_captureClosedFrameLayout(rect)
    rect = rect or self:getTextureRect()
    self._frameWidth = rect.size.x
    self._startX = rect.position.x
    self._startY = rect.position.y
end

function DoorBase:_resolveFrameLayout()
    if self._frameWidth <= 0 then
        self:_captureClosedFrameLayout()
    end
end

---@return integer
function DoorBase:_getFrameCount()
    self:_resolveFrameLayout()
    if self._frameWidth <= 0 then
        return 1
    end
    local texture = self:getTexture()
    if texture == nil then
        return 1
    end
    local remainingWidth = texture:getSize().x - self._startX
    return math.max(1, math.floor(remainingWidth / self._frameWidth))
end

---@return integer
function DoorBase:_getCurrentFrameIndex()
    self:_resolveFrameLayout()
    if self._frameWidth <= 0 then
        return 0
    end
    local rect = self:getTextureRect()
    return math.max(0, math.floor((rect.position.x - self._startX) / self._frameWidth))
end

---@return integer
function DoorBase:_resolveClosingFrameIndex()
    if self._frameWidth <= 0 then
        return 0
    end
    local rect = self:getTextureRect()
    local texture = self:getTexture()
    if texture ~= nil and rect.position.x + self._frameWidth < texture:getSize().x then
        return 0
    end
    local stripStartX = rect.position.x % self._frameWidth
    if stripStartX == self._startX then
        return 0
    end
    self._startX = stripStartX
    self._startY = rect.position.y
    return self:_getCurrentFrameIndex()
end

function DoorBase:_finishOpening()
    self._openFinished = true
    self.opening = false
    self:destroy()
    local gameMap = self:getMap()
    ---@cast gameMap GameMap
    local scene = gameMap:getScene()
    ---@cast scene Source.Scenes.SceneMap.SceneMap
    scene:recordDestroyedActor(self)
end

function DoorBase:_finishClosing()
    self._closeFinished = true
    self.closing = false
    self._frameIndex = 0
end

--- Set the texture rect to the given frame index (0-based).
---
--- - @param index  Frame index (column in the sprite sheet)
---@param index integer
function DoorBase:_advanceToFrame(index)
    if self._frameWidth <= 0 then
        return
    end
    local rect = self:getTextureRect()
    local position = sf.Vector2i.new(self._startX + index * self._frameWidth, self._startY)
    ---@cast position sf.Vector2i
    self:setTextureRect(sf.IntRect.new(position, rect.size))
end

return class(DoorBase, Actor)
