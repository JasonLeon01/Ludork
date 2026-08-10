local Engine = require("Engine")
local GeneralEnum = require("Source.Configs.GeneralEnum")

local InfoBase = Engine.InfoBase
local GeneralDataKey = GeneralEnum.GeneralDataKey

local StateInfo = {}

StateInfo._infoType = GeneralDataKey.State

function StateInfo:init()
    super(StateInfo, self).init()
    self._owner = nil
    self.stacks = 0
end

function StateInfo:getStacks()
    return self.stacks
end

function StateInfo:getOwner()
    return self._owner
end

function StateInfo:setOwner(owner)
    self._owner = owner
end

function StateInfo:onWalk(battler)
end

function StateInfo:onHookTriggered(battler)
end

return class(StateInfo, InfoBase)
