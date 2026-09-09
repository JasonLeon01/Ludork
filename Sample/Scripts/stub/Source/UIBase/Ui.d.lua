---@meta Source.UIBase.Ui

---@class Source.UIBase.Ui.Module
local Ui = {}

---@param assetKey string
---@return string
function Ui.GetEventName(assetKey) end

---@param assetKey string
---@param payload  any
function Ui.Publish(assetKey, payload) end

--- Define a controller class; its update channel is created when the first instance is prepared.
---@generic T: table
---@param viewClass  Class.ClassType<Source.UIBase.UiView> | Source.UIBase.UiView
---@param definition T
---@param baseClass  Class.ClassType<any> | table | nil
---@return T & Class.ClassType<T>
function Ui.Define(viewClass, definition, baseClass) end

--- Native UI facade; its business Controller is a separate private object.
---@class Source.UIBase.Ui.Window: Engine.Canvas
---@field ui                    Source.UIBase.UiView
---@field _controller           Source.UIBase.UiController
---@field _controllerDispose    function | nil
---@field _windowBindingDispose function | nil
---@field _windowBinding        Source.UIBase.UiWindow
---@field _controllerReady      boolean
local Window = {}

function Window:dispose() end

---@param payload any
function Window.Publish(payload) end

---@param manager GlobalCore.UIManager
---@return Source.UIBase.Ui.Window
function Window:mount(manager) end

function Window:unmount() end

--- Return a native UI class composed with a private Controller and generated View.
--- Instance methods delegate to that Controller; static methods and named constants remain on the UI class.
---@generic T: table
---@param viewClass  Class.ClassType<Source.UIBase.UiView> | Source.UIBase.UiView
---@param definition T
---@param nativeBase Class.ClassType<any> | table | nil
---@return Class.ClassType<Source.UIBase.Ui.Window> & Source.UIBase.Ui.Window
function Ui.DefineWindow(viewClass, definition, nativeBase) end

return Ui
