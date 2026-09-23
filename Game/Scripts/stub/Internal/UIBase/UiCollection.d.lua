---@meta Internal.UIBase.UiCollection

--- Owns the dynamic rows in one container. Authored preview children are detached when the collection is created.
---@class Internal.UIBase.UiCollection<T: Internal.UIBase.UiController>
---@field container        Engine.Canvas | Engine.ListView
---@field items            T[]
---@field _controllerClass { new: fun(model: any): T }
---@field _disposers       (fun(controller: T))[]
local UiCollection = {}

---@generic T: Internal.UIBase.UiController
---@param container       Engine.Canvas | Engine.ListView
---@param controllerClass { new: fun(model: any): T }
---@return Internal.UIBase.UiCollection<T>
function UiCollection.new(container, controllerClass) end

--- Create, prepare and attach a row. Its Controller is owned by this collection.
---@param model       any
---@param logicalSize sf.Vector2u | nil
---@return T
function UiCollection:add(model, logicalSize) end

--- Recompute ListView positions after a batch of changes.
function UiCollection:layout() end

--- Release row callbacks, subscriptions and Views, then remove their controls.
function UiCollection:clear() end

function UiCollection:dispose() end

return UiCollection
