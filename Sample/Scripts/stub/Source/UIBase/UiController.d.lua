---@meta Source.UIBase.UiController

---@class Source.UIBase.UiController
---@field windowOptions         Source.UIBase.Ui.WindowOptions | nil
---@field _transition           Source.UIBase.WindowTransition
---@field host                  Source.UIBase.Ui.Window | nil
---@field _hostDispose          function | nil
---@field assetKey              string
---@field viewClass             Class.ClassType<Source.UIBase.UiView> | nil
---@field ui                    Source.UIBase.UiView
---@field viewUpdateEvent       string
---@field refreshEvents         string[]
---@field model                 any
---@field view                  Engine.AssetInstance
---@field root                  Engine.ControlBase
---@field _eventSubscriptions   integer[]
---@field _observer             Source.UIBase.UiObserver<Source.UIBase.UiController>
---@field _observerDispose      function
---@field _callbackTarget       Source.UIBase.UiController[] | nil
---@field _viewLogicalSize      sf.Vector2u | nil
---@field _viewUpdateUnregister function | nil
---@field _bound                boolean
---@field _disposed             boolean
local UiController = {}

---@param model any
---@param ui    Source.UIBase.UiView | nil
function UiController:init(model, ui) end

--- Attach a child window to its existing generated asset View.
---@generic T: { FromView: function }
---@param name        string
---@param windowClass T
---@param ...         any
---@return T
function UiController:createChild(name, windowClass, ...) end

--- Runs after init, bind and the first refresh.
function UiController:ready() end

function UiController:bind() end

function UiController:refresh() end

function UiController:onViewUpdate(_) end

function UiController:refreshFromEvent(payload) end

function UiController:subscribe(eventName, handler, priority) end

--- Observe one model field, immediately applying its current value by default.
--- Later writes use native Class.monitor; all subscriptions stop on disposal.
---@generic T: Source.UIBase.UiController, V
---@param self       T
---@param target     table | userdata
---@param field      string
---@param method     fun(controller: T, newValue: V | nil, oldValue: V | Class.MissingValue)
---@param immediate? boolean
---@return fun()
function UiController:watch(target, field, method, immediate) end

--- Bind an unbound Controller method without retaining the Controller.
--- Arguments and return values are preserved; disposal or collection makes the callback inert.
---@generic T: Source.UIBase.UiController
---@param self   T
---@param method fun(controller: T, ...: any): any
---@return function
function UiController:bindCallback(method) end

---@return Engine.ControlBase
function UiController:prepare(logicalSize) end

---@param parent      Engine.Canvas | Engine.ListView
---@param logicalSize sf.Vector2u | nil
---@return Engine.ControlBase
function UiController:attachTo(parent, logicalSize) end

---@param host             Source.Windows.Base.WindowBase
---@param logicalSize      sf.Vector2u | nil
---@param transitionTarget string | nil
---@return Engine.ControlBase
function UiController:attachWindowView(host, logicalSize, transitionTarget) end

---@param host        Source.Windows.Base.WindowBase
---@param logicalSize sf.Vector2u | nil
---@return Engine.ControlBase
function UiController:attachNestedWindowView(host, logicalSize) end

---@param host   Engine.Canvas
---@param target string | nil
---@return Source.UIBase.WindowTransition
function UiController:createTransition(host, target) end

---@generic T: Source.UIBase.UiController
---@param container       Engine.Canvas | Engine.ListView
---@param controllerClass { new: fun(model: any): T }
---@return Source.UIBase.UiCollection<T>
function UiController:createCollection(container, controllerClass) end

---@param name   string
---@param target string | nil
---@return boolean
function UiController:hasAnimation(name, target) end

--- Natural completion holds the final frame and invokes onFinished once; no stopAnimation call is needed.
--- The native layer replaces an animation on the same target and cancels its old callback; different targets run independently.
--- A missing animation returns false and invokes onFinished synchronously.
---@param name       string
---@param target     string | nil
---@param onFinished function | nil
---@return boolean
function UiController:playAnimation(name, target, onFinished) end

--- Clears the matching animation's presentation effect, including a held final frame, and cancels its pending callback.
--- An old or non-current name leaves the target's current animation and callback intact.
---@param name   string
---@param target string | nil
function UiController:stopAnimation(name, target) end

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

---@return Source.UIBase.UiView
function UiController:requireAsset(name) end

---@param name       string
---@param propertyId string
---@param value      nil | boolean | number | string | sf.Vector2f | sf.Vector2u | sf.IntRect | sf.Color | string[]
function UiController:setProperty(name, propertyId, value) end

function UiController:setText(name, text) end

function UiController:dispose() end

function UiController:registerUiInstance() end

---@param unregister fun(controller: Source.UIBase.UiController)
function UiController:setViewUpdateUnregister(unregister) end

return UiController
