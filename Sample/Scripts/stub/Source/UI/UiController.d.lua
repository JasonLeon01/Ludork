---@meta Source.UI.UiController

---@class Source.UI.UiController
---@field assetKey              string
---@field viewUpdateEvent       string
---@field refreshEvents         string[]
---@field model                 any
---@field view                  Engine.AssetInstance
---@field root                  Engine.ControlBase
---@field _uiManager            GlobalCore.UIManager | nil
---@field _eventSubscriptions   integer[]
---@field _viewLogicalSize      sf.Vector2u | nil
---@field _viewUpdateUnregister function | nil
---@field _bound                boolean
---@field _mounted              boolean
---@field _disposed             boolean
local UiController = {}

function UiController:init(model, instance) end

function UiController:bind() end

function UiController:refresh() end

function UiController:onViewUpdate(_) end

function UiController:_refreshFromEvent(payload) end

function UiController:subscribe(eventName, handler, priority) end

---@return Engine.ControlBase
function UiController:prepare(logicalSize) end

---@param parent      Engine.Canvas
---@param logicalSize sf.Vector2u | nil
---@return Engine.ControlBase
function UiController:attachTo(parent, logicalSize) end

---@param host        Source.Windows.Base.WindowBase
---@param logicalSize sf.Vector2u | nil
---@return Engine.ControlBase
function UiController:attachWindowView(host, logicalSize) end

---@param host        Source.Windows.Base.WindowBase
---@param logicalSize sf.Vector2u | nil
---@return Engine.ControlBase
function UiController:attachNestedWindowView(host, logicalSize) end

---@return Engine.Window
function UiController:getWindowFrame() end

---@return Engine.Canvas
function UiController:getContent() end

---@param control Engine.ControlBase
function UiController:detachControl(control) end

function UiController:mount(uiManager, logicalSize) end

function UiController:unmount() end

---@return Engine.AssetInstance
function UiController:getView() end

---@return Engine.ControlBase
function UiController:getRoot() end

---@return Engine.ControlBase
function UiController:requireControl(name) end

---@return Engine.ControlBase
function UiController:getNodeByName(name) end

---@return Engine.AssetInstance
function UiController:requireAsset(name) end

function UiController:setProperty(name, propertyId, value) end

function UiController:setText(name, text) end

function UiController:dispose() end

return UiController
