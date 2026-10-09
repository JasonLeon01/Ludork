---@meta Internal.UIBase.UiView

--- Generated Views recursively create the complete typed asset tree without requiring business Controllers.
--- The View owns its nested Views, dynamic collections and the Controller bound to it.
---@class Internal.UIBase.UiView
---@field _windowBinding        Internal.UIBase.UiWindow | nil
---@field assetKey              string
---@field designSize            sf.Vector2f
---@field instance              Engine.AssetInstance
---@field root                  Engine.ControlBase
---@field controls              table<string, Engine.ControlBase | nil>
---@field assets                table<string, Internal.UIBase.UiView | nil>
---@field _owned                table<table, function>
---@field _collections          table<Engine.ControlBase, Internal.UIBase.UiCollection>
---@field _controller           Internal.UIBase.UiController | nil
---@field _controllerDispose    function | nil
---@field _uiManager            GlobalCore.UIManager | nil
---@field _mounted              boolean
---@field _disposed             boolean
---@field _released             boolean
---@field _reuseVisible         boolean
---@field _reuseActive          boolean | nil
---@field _logicalSize          sf.Vector2u | nil
---@field _animationBindings    table<string, { name: string, target: string | nil }>
---@field _animationGenerations table<string, integer>
---@field new                   fun(instance?: Engine.AssetInstance): Internal.UIBase.UiView
local UiView = {}

---@param instance Engine.AssetInstance | nil
function UiView:init(instance) end

--- Adopt an object with a terminal dispose method; duplicate disposal uses the captured class dispatcher.
---@generic T: table
---@param child T
---@return T
function UiView:own(child) end

---@param controller Internal.UIBase.UiController
function UiView:bindController(controller) end

---@param logicalSize sf.Vector2u | nil
---@return Engine.ControlBase
function UiView:prepare(logicalSize) end

--- Attach or move the existing root; attaching to its current parent does not duplicate it or reset focus.
---@param parent      Engine.Canvas | Engine.ListView
---@param logicalSize sf.Vector2u | nil
---@return Engine.ControlBase
function UiView:attachTo(parent, logicalSize) end

--- Attach the prepared root without reflowing or replacing Controller layout adjustments.
---@param parent Engine.Canvas | Engine.ListView
---@return Engine.ControlBase
function UiView:attachPreparedTo(parent) end

---@return Engine.Window
function UiView:getWindowFrame() end

---@return Engine.Canvas
function UiView:getContent() end

---@param host             Internal.UIBase.WindowBase
---@param logicalSize      sf.Vector2u | nil
---@param transitionTarget string | nil
---@return Engine.ControlBase
function UiView:attachWindowView(host, logicalSize, transitionTarget) end

---@param host        Internal.UIBase.WindowBase
---@param logicalSize sf.Vector2u | nil
---@return Engine.ControlBase
function UiView:attachNestedWindowView(host, logicalSize) end

---@param host             Internal.UIBase.WindowBase
---@param frame            Engine.Window
---@param content          Engine.Canvas
---@param nested           boolean
---@param transitionTarget string | nil
---@return Engine.ControlBase
function UiView:attachPreparedWindow(host, frame, content, nested, transitionTarget) end

---@param host   Engine.Canvas
---@param target string | nil
---@return Internal.UIBase.WindowTransition
function UiView:createTransition(host, target) end

---@generic T: Internal.UIBase.UiController
---@param container       Engine.Canvas | Engine.ListView
---@param controllerClass { new: fun(model: any): T }
---@return Internal.UIBase.UiCollection<T>
function UiView:createCollection(container, controllerClass) end

---@param name   string
---@param target string | nil
---@return boolean
function UiView:hasAnimation(name, target) end

--- Natural completion holds the final frame and calls onFinished once. A missing animation returns false and calls it synchronously.
---@param name       string
---@param target     string | nil
---@param onFinished function | nil
---@return boolean
function UiView:playAnimation(name, target, onFinished) end

---@param name   string
---@param target string | nil
function UiView:stopAnimation(name, target) end

---@param control Engine.ControlBase
function UiView:detachControl(control) end

---@param uiManager   GlobalCore.UIManager
---@param logicalSize sf.Vector2u | nil
function UiView:mount(uiManager, logicalSize) end

--- Register the prepared root without reflowing or replacing Controller layout adjustments.
---@param uiManager GlobalCore.UIManager
function UiView:mountPrepared(uiManager) end

function UiView:unmount() end

function UiView:_stopAnimations() end

--- Clear callbacks, animations and dynamic rows while retaining the authored control tree.
--- Nested Views keep their authored parent when detachRoot is false.
---@param detachRoot?        boolean
---@param releaseController? boolean
function UiView:releaseForReuse(detachRoot, releaseController) end

--- Restore the root state of a released View and its nested Views before refreshing the new model.
--- Nested Controllers resume without a model; their owner binds and prepares them for the new row model.
---@param reuseController? boolean
function UiView:reuse(reuseController) end

--- Terminally release the complete owned tree and its business callbacks, and detach the root.
function UiView:dispose() end

---@param binding Internal.UIBase.UiWindow
function UiView:bindWindow(binding) end

function UiView:syncWindowHosts() end

return UiView
