---@meta Source.NodeFunctions.State

--- @brief Get the battler currently hosting this state's blueprint execution.
---
--- - @return The hosting `Battler` instance, or nil if not attached.
---@return Source.Battler.Battler | nil
function State.GetStateOwner() end

--- @brief Get a keyword argument injected into the current blueprint event.
---
--- Convenience wrapper that reads `__<name>__` from the local graph context.
--- Useful keys for state events: "battler".
---
--- - @param name Argument name (without surrounding underscores).
--- - @param default Value to return when the key is missing.
--- - @return The argument value or `default`.
---@generic T
---@param name    string
---@param default T | nil
---@return T | nil
function State.GetEventArg(name, default) end

--- @brief Read a named attribute from any battler (e.g. HP, MAXHP, ATK, DEF).
---
--- - @param battler The target battler.
--- - @param attrName Attribute name.
--- - @param default Default when missing.
--- - @return Attribute value or default.
---@generic T
---@param battler  Source.Battler.Battler | nil
---@param attrName string
---@param default  T | nil
---@return T | nil
function State.GetBattlerAttr(battler, attrName, default) end

--- @brief Write a named attribute on any battler.
---
--- - @param battler The target battler.
--- - @param attrName Attribute name.
--- - @param value New value.
---@generic T
---@param battler  Source.Battler.Battler | nil
---@param attrName string
---@param value    T
function State.SetBattlerAttr(battler, attrName, value) end

--- @brief Subtract HP from any battler (floored at 0).
---
--- - @param battler The target battler.
--- - @param amount HP to remove.
---@param battler Source.Battler.Battler | nil
---@param amount  integer
function State.DamageBattler(battler, amount) end

--- @brief Restore HP on any battler (capped at MAXHP when present).
---
--- - @param battler The target battler.
--- - @param amount HP to restore.
---@param battler Source.Battler.Battler | nil
---@param amount  integer
function State.HealBattler(battler, amount) end

--- @brief Check whether a battler currently carries the given state.
---
--- - @param battler The target battler.
--- - @param stateID State identifier.
--- - @return True if active.
---@param battler Source.Battler.Battler | nil
---@param stateID string
---@return boolean
function State.BattlerHasState(battler, stateID) end

--- @brief Apply a state (by ID) to any battler.
---
--- - @param battler The target battler.
--- - @param stateID State identifier.
--- - @param stacks Stack count to apply or add.
---@param battler Source.Battler.Battler | nil
---@param stateID string
---@param stacks  integer
function State.AddStateTo(battler, stateID, stacks) end

--- @brief Remove a state (by ID) from any battler.
---
--- - @param battler The target battler.
--- - @param stateID State identifier.
---@param battler Source.Battler.Battler | nil
---@param stateID string
function State.RemoveStateFrom(battler, stateID) end

--- @brief Reduce a state stack count on any battler and remove it at zero.
---
--- - @param battler The target battler.
--- - @param stateID State identifier.
--- - @param stacks Stack count to reduce.
---@param battler Source.Battler.Battler | nil
---@param stateID string
---@param stacks  integer
function State.ReduceStateFrom(battler, stateID, stacks) end

--- @brief Explicitly trigger one active state's hook event on a battler.
---
--- - @param battler The target battler.
--- - @param stateID State identifier.
---@param battler Source.Battler.Battler | nil
---@param stateID string
function State.TriggerStateHook(battler, stateID) end

return State
