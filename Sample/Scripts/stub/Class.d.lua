---@meta

---@class Class
Class = Class or {}

---@class Class.MissingValue

---@class Class.ClassType<T>
---@field __ludorkClass boolean
---@field __bases table
---@field __base table|nil
---@field new fun(...: any): T
---@field init fun(self: T, ...: any)|nil
---@field dispose fun(self: T)|nil
---@field __getters table<string, fun(self: T): any>|nil
---@field __setters table<string, fun(self: T, value: any)>|nil

--- Finalize a definition table as a Ludork class with C3 MRO.
---@generic T: table
---@param definition T
---@param ... table|userdata
---@return T & Class.ClassType<T>
function class(definition, ...) end

--- Return whether value is an instance of targetClass (including MRO).
---@generic T
---@param value T
---@param targetClass Class.ClassType<any>|table
---@return boolean
function Class.isInstance(value, targetClass) end

--- Return whether value is a subclass of targetClass (including MRO).
---@param value Class.ClassType<any>|table
---@param targetClass Class.ClassType<any>|table
---@return boolean
function Class.isSubclass(value, targetClass) end

--- Return the Class type of value, or a native type / type name fallback.
---@generic T
---@param value T
---@return Class.ClassType|string
function Class.type(value) end

--- Return whether key is stored directly on a table or composite instance.
---@param value table|userdata
---@param key any
---@return boolean
function Class.hasOwnField(value, key) end

--- Return a detached copy of the class MRO.
---@param value table|userdata
---@return table[]
function Class.getMro(value) end

--- Return the declared parameter names, excluding self.
---@param callable function
---@return string[]
function Class.getParameterNames(callable) end

--- Construct a class using argument names from its init function.
---@generic T
---@param type Class.ClassType<T>
---@param arguments? table<string, any>
---@return T
function Class.constructNamed(type, arguments) end

--- Resolve a super proxy for the defining class on the call stack, or for cls/self.
---@generic T
---@param cls  T | Class.ClassType<T> | nil
---@param self T | nil
---@return any
function Class.super(cls, self) end

---@generic V
---@param target table|userdata
---@param name string
---@param callback fun(oldValue: V|Class.MissingValue, newValue: V, ...)
---@param params table|nil
function Class.monitor(target, name, callback, params) end

---@param target table|userdata
---@param name string
function Class.unmonitor(target, name) end

---@param name string
---@param callback function
function Class.registerService(name, callback) end

---@param name string
function Class.unregisterService(name) end

---@type Class.MissingValue
Class.MISSING = {}

--- Global super proxy (same as Class.super).
---@generic T
---@param cls  T | Class.ClassType<T> | nil
---@param self T | nil
---@return any
function super(cls, self) end
