local BattlerInfoComponent = require("Source.Components.BattlerInfoComponent")

---@class Source.Components.PlayerInfoComponent
local PlayerInfoComponent = {}

PlayerInfoComponent.name = ""
PlayerInfoComponent.desc = ""
PlayerInfoComponent.LEVEL = 1
PlayerInfoComponent.CLASS = ""

return class(PlayerInfoComponent, BattlerInfoComponent)
