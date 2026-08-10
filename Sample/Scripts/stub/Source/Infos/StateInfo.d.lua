---@meta Source.Infos.StateInfo
--- @brief State data + logic layer.
---
--- A `StateInfo` is the data + blueprint container for a battler status effect
--- (poisoned, burning, blessed, etc). Each active state is owned by exactly one
--- `Battler` (the host). State blueprints expose walking behaviour and one
--- developer-triggered hook; combat resolution is handled directly by the
--- battle system.
---
--- Defines state-related blueprint events:
---     onWalk, onHookTriggered.
--- Independent of Actor; can be used standalone in inventory/shop UI.
---
---@class StateInfo: Engine.InfoBase
---@field name string
---@field icon string
---@field stacks integer
---@field _owner Source.Battler.Battler | nil
local StateInfo = {}

--- @brief Construct a state info with no host yet.
function StateInfo:init() end

--- @brief Get the current stack count for this state.
---
--- - @return Active stack count.
---@return integer
function StateInfo:getStacks() end

--- @brief Get the battler currently affected by this state.
---
--- - @return The hosting `Battler` or nil if not attached.
---@return Source.Battler.Battler | nil
function StateInfo:getOwner() end

--- @brief Bind this state to a host battler.
---
--- - @param owner The hosting `Battler` instance, or nil to detach.
---@param owner Source.Battler.Battler | nil
function StateInfo:setOwner(owner) end

--- @brief Blueprint event: called each step the affected battler takes.
---
--- - @param battler The hosting battler.
---@param battler Source.Battler.Battler | nil
function StateInfo:onWalk(battler) end

--- @brief Blueprint event: called when the hosting battler explicitly triggers its state hook.
---
--- - @param battler The hosting battler.
---@param battler Source.Battler.Battler | nil
function StateInfo:onHookTriggered(battler) end

return StateInfo
