---@meta Source.UIBase.LazyWindow

--- Owns one window, constructed and mounted by its factory on first use.
---@class Source.UIBase.LazyWindow<T: Source.UIBase.Ui.Window>
---@field _factory (fun(): T) | nil
---@field _window  T | nil
local LazyWindow = {}

--- Store the factory without constructing its window. The factory must finish configuration and mounting before returning.
---@generic T: Source.UIBase.Ui.Window
---@param factory fun(): T
---@return Source.UIBase.LazyWindow<T>
function LazyWindow.new(factory) end

--- Create the complete window once and reuse that instance on later calls.
---@return T
function LazyWindow:get() end

--- Return the existing window without invoking its factory.
---@return T | nil
function LazyWindow:peek() end

--- Release the factory and dispose an existing window. Disposal is terminal.
function LazyWindow:dispose() end

return LazyWindow
