local Engine = require("Engine")
local GlobalFunctions = require("GlobalFunctions")

local Component = Engine.Component
local ComponentsFunctions = GlobalFunctions.Components

---@class Source.Components.BattlerInfoComponent
local BattlerInfoComponent = {}

BattlerInfoComponent.MAXHP = 1000
BattlerInfoComponent.ATK = 10
BattlerInfoComponent.DEF = 10
BattlerInfoComponent.EXP = 0
BattlerInfoComponent.GOLD = 0
BattlerInfoComponent.ANIMATION_KEY = ""

function BattlerInfoComponent:init(values)
    values = values or {}
    local defaults = ComponentsFunctions.getComponentFieldDefaults(Class.type(self))
    for name, default in pairs(defaults) do
        self[name] = deepcopy(values[name] == nil and default or values[name])
    end
end

return class(BattlerInfoComponent, Component)
