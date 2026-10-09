---@meta Internal.UIBase.UiCollection

--- Owns the dynamic rows in one container. Authored preview children are detached when the collection is created.
---@class Internal.UIBase.UiCollection<T: Internal.UIBase.UiController>
---@field container        Engine.Canvas | Engine.ListView
---@field items            T[]
---@field _controllerClass { new: fun(model: any): T }
---@field _disposers       (fun(controller: T))[]
---@field _idle            T[]
---@field _idleDisposers   (fun(controller: T))[]
---@field _disposed        boolean
local UiCollection = {}

---@generic T: Internal.UIBase.UiController
---@param container       Engine.Canvas | Engine.ListView
---@param controllerClass { new: fun(model: any): T }
---@return Internal.UIBase.UiCollection<T>
function UiCollection.new(container, controllerClass) end

--- Reuse an idle row or create one, then bind, prepare and attach it. Its Controller is owned by this collection.
---@param model       any
---@param logicalSize sf.Vector2u | nil
---@return T
function UiCollection:add(model, logicalSize) end

--- Recompute ListView positions after a batch of changes.
function UiCollection:layout() end

--- Detach all rows and release their bindings. Retain up to 64 idle rows locally; dispose of excess rows.
function UiCollection:clear() end

--- Terminally dispose of every active and idle row.
function UiCollection:dispose() end

return UiCollection
