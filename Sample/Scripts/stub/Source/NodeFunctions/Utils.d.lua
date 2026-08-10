---@meta Source.NodeFunctions.Utils

---@alias Source.NodeFunctions.Utils.NodeReference<T> Source.NodeFunctions.Utils.AttrRef<T> | Source.NodeFunctions.Utils.LocalRef<T>

---@class Source.NodeFunctions.Utils.AttrRef<T>
---@field _isNodeReference boolean
---@field obj              table
---@field name             string
local AttrRef = {}

---@generic T
---@param obj  table
---@param name string
function AttrRef:init(obj, name) end

---@generic T
---@return T
function AttrRef:get() end

---@generic T
---@param value T
---@return T
function AttrRef:set(value) end

---@class Source.NodeFunctions.Utils.LocalRef<T>
---@field _isNodeReference boolean
---@field loc              table<string, T>
---@field name             string
---@field default          T | nil
local LocalRef = {}

---@generic T
---@param loc     table<string, T>
---@param name    string
---@param default T | nil
function LocalRef:init(loc, name, default) end

---@generic T
---@return T
function LocalRef:get() end

---@generic T
---@param value T
---@return T
function LocalRef:set(value) end

--- @brief Blueprint conditional branch.
---
--- - @param condition The condition to evaluate.
--- - @return 0 if True, 1 if False.
---@param condition boolean
---@return integer
function Utils.IF(condition) end

--- @brief Set a local variable value.
---
--- - @param valueName The variable name.
--- - @param value The value to set.
---@generic T
---@param valueName string
---@param value     T | nil
function Utils.SetLocalValue(valueName, value) end

--- @brief Get a local variable value.
---
--- - @param valueName The variable name.
--- - @param default Default value if the variable is not found.
--- - @return The variable value.
---@generic T
---@param valueName string
---@param default   T | nil
---@return T | nil
function Utils.GetLocalValue(valueName, default) end

--- @brief Get a reference wrapper for a local variable.
---
--- - @param valueName The variable name.
--- - @param default Default value if the variable is not found.
--- - @return A _localRef wrapper.
---@generic T
---@param valueName string
---@param default   T | nil
---@return Source.NodeFunctions.Utils.LocalRef<T>
function Utils.GetLocalValueRef(valueName, default) end

--- @brief Set a game variable on the current scene's game instance.
---
--- - @param valueName The variable name.
--- - @param value The value to set.
---@param valueName string
---@param value     Source.GameInstance.RecordValue | Source.NodeFunctions.Utils.NodeReference<Source.GameInstance.RecordValue>
function Utils.SetGameVariable(valueName, value) end

--- @brief Get a game variable from the current scene's game instance.
---
--- - @param valueName The variable name.
--- - @param default Default value if the variable is not found.
--- - @return The variable value.
---@param valueName string
---@param default   Source.GameInstance.RecordValue
---@return Source.GameInstance.RecordValue
function Utils.GetGameVariable(valueName, default) end

--- @brief Get a reference wrapper for a game variable.
---
--- - @param valueName The variable name.
--- - @param default Default value if the variable is not found.
--- - @return A _localRef wrapper.
---@param valueName string
---@param default   Source.GameInstance.RecordValue
---@return Source.NodeFunctions.Utils.LocalRef<Source.GameInstance.RecordValue>
function Utils.GetGameVariableRef(valueName, default) end

--- @brief Add a new player by class path.
---
--- - @param playerClass The class path for the player blueprint.
---@param playerClass string
function Utils.AddPlayerByClass(playerClass) end

--- @brief Remove a player by class path.
---
--- - @param playerClass The class path to remove.
---@param playerClass string
function Utils.RemovePlayerByClass(playerClass) end

--- @brief Spawn an animation at a given position.
---
--- - @param animName The animation name.
--- - @param position The world position as an `sf.Vector2f`.
--- - @param rotation The rotation in degrees.
--- - @param scale The scale as an `sf.Vector2f`.
---@param animName string
---@param position sf.Vector2f
---@param rotation number
---@param scale    sf.Vector2f
function Utils.AddAnim(animName, position, rotation, scale) end

--- @brief Spawn an animation at an actor's current position.
---
--- - @param animName The animation name.
--- - @param actorTag The target actor tag.
--- - @param rotation The rotation in degrees.
--- - @param scale The scale as an `sf.Vector2f`.
---@param animName string
---@param actorTag string
---@param rotation number
---@param scale    sf.Vector2f
function Utils.AddAnimOn(animName, actorTag, rotation, scale) end

--- @brief Get the duration of an animation.
---
--- - @param animName The animation name.
--- - @return The duration in seconds.
---@param animName string
---@return number
function Utils.GetAnimLength(animName) end

--- @brief Get the visual duration of an animation, excluding sound track length.
---
--- - @param animName The animation name.
--- - @return The visual duration in seconds.
---@param animName string
---@return number
function Utils.GetAnimVisualLength(animName) end

--- @brief Call the parent implementation of the current blueprint event.
---
--- - @param obj    The object instance calling super.
--- - @param params Positional arguments forwarded to the parent event.
--- - @return True if a parent graph or method handled the event.
---@param obj    table
---@param params table
---@return boolean
function Utils.SUPER(obj, params) end

---@return table
function Utils.SELF() end

---@generic T
---@param obj      table
---@param attrName string
---@return Source.NodeFunctions.Utils.AttrRef<T>
function Utils.GetAttrRef(obj, attrName) end

---@generic T
---@param obj      table
---@param attrName string
---@return T
function Utils.GetAttr(obj, attrName) end

---@generic T
---@param obj      table
---@param attrName string
---@param value    T
function Utils.SetAttr(obj, attrName, value) end

---@return GlobalCore.SceneBase | nil
function Utils.GetScene() end

---@generic T
---@param value T | nil
---@return boolean
function Utils.IsValidValue(value) end

--- @brief Convert large numeric values to short display text.
---
--- - @param value Number or digit-only string to shorten.
--- - @return Shortened text for large numeric values, or the original value.
---@param value number | string
---@return number | string
function Utils.ToShortNumber(value) end

---@param commonFunctionName string
---@return table
function Utils.RunCommonFunction(commonFunctionName) end

--- @brief Subscribe an object's event method to the shared EventBus.
---
--- - @param key EventBus key to subscribe to.
--- - @param obj Target object that owns the event or method.
--- - @param functionName Name of the event or method to invoke.
---@param key          string
---@param obj          table
---@param functionName string
function Utils.RegisterEventBus(key, obj, functionName) end

--- @brief Subscribe an object's blueprint event to the shared EventBus.
---
--- EventBus payload is ignored; the blueprint event is invoked without arguments.
---
--- - @param key EventBus key to subscribe to.
--- - @param obj Target object that owns the blueprint event.
--- - @param eventName Blueprint event name to invoke.
---@param key       string
---@param obj       table
---@param eventName string
function Utils.RegisterEventBusEvent(key, obj, eventName) end

--- @brief Unsubscribe handlers from the shared EventBus by key.
---
--- - @param key EventBus key subscribed to.
--- - @return True if any handler was found and removed, False otherwise.
---@param key string
---@return boolean
function Utils.UnregisterEventBus(key) end

--- @brief Unsubscribe blueprint event handlers from the shared EventBus.
---
--- - @param key EventBus key subscribed to.
--- - @param obj Optional target object. If provided, only that object's handler is removed.
--- - @return True if any handler was found and removed, False otherwise.
---@param key string
---@param obj table | nil
---@return boolean
function Utils.UnregisterEventBusEvent(key, obj) end

--- @brief Post an EventBus event with keyword arguments.
---
--- - @param key EventBus key to trigger.
--- - @param kwargs Keyword arguments passed to registered handlers.
---@param key    string
---@param kwargs table<string, any>
function Utils.TriggerEventBus(key, kwargs) end

--- @brief Trigger a blueprint event on an object without arguments.
---
--- - @param obj Target object that owns the blueprint event.
--- - @param eventName Blueprint event name to invoke.
---@param obj       table
---@param eventName string
function Utils.TriggerBlueprintEvent(obj, eventName) end

function Utils.BackToTitle() end

---@generic T
---@param message T | nil
function Utils.Print(message) end

---@param script string
function Utils.EXEC(script) end

--- @brief Get an attribute value from the blueprint owner.
---
--- - @param attrName The attribute name.
--- - @return The attribute value.
---@generic T
---@param attrName string
---@return T
function Utils.GetSelfAttr(attrName) end

--- @brief Set an attribute value on the blueprint owner.
---
--- - @param attrName The attribute name.
--- - @param value The value to set.
---@generic T
---@param attrName string
---@param value    T
function Utils.SetSelfAttr(attrName, value) end

--- @brief Check whether the player is overlapping the blueprint owner.
---
--- - @return True if the player shares the same cell as the owner.
---@return boolean
function Utils.IfPlayerOverlaps() end

--- @brief Compare a game variable with a value.
---
--- - @param varName The game variable name.
--- - @param op Comparison operator: "==", "!=", "<", "<=", ">", ">=".
--- - @param value The value to compare against.
--- - @return True if the comparison holds.
---@param varName string
---@param op      string
---@param value   any | nil
---@return boolean
function Utils.IfGameVar(varName, op, value) end

return Utils
