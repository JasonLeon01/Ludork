---@meta Source.UIBase.UiObserver

--- Owns named native field subscriptions without retaining the Controller or observed objects.
---@class Source.UIBase.UiObserver<T: Source.UIBase.UiController>
---@field private _controller T[]
---@field private _stops      table<function, boolean>
local UiObserver = {}

---@generic T: Source.UIBase.UiController
---@param controller T
---@return Source.UIBase.UiObserver<T>
function UiObserver.new(controller) end

--- Observe a field through Class.monitor and call an unbound Controller method with the new value first.
--- The initial call is immediate by default and receives Class.MISSING as its old value.
--- Later calls preserve native equality, missing-value and nil-assignment semantics.
--- Each watch owns a distinct subscription; stopping it does not affect other observers.
---@generic V
---@param target     table | userdata
---@param field      string
---@param method     fun(controller: T, newValue: V | nil, oldValue: V | Class.MissingValue)
---@param immediate? boolean
---@return fun() stop Idempotently remove this subscription.
function UiObserver:watch(target, field, method, immediate) end

--- Remove every owned subscription. Controller disposal calls this automatically.
function UiObserver:dispose() end

return UiObserver
