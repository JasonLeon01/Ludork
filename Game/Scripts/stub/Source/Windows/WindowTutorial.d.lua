---@meta

---@class Source.Windows.WindowTutorial.Controller: Internal.UIBase.UiController
---@field host            Source.Windows.WindowTutorial
---@field ui              Internal.UI.WindowTutorial
---@field root            Engine.Canvas
---@field SHRINK_DURATION number
---@field _capture        Engine.InputCapture | nil
---@field _rect           sf.IntRect | nil
---@field _elapsed        number
---@field _ready          boolean
---@field _finished       boolean
local Controller = {}

function Controller:init() end

--- Start a 0.25-second contraction. All input stays captured until confirmation or close.
---@param rect sf.IntRect
---@param text string
function Controller:open(rect, text) end

--- Replace already-localised text and reposition it without restarting the tutorial.
---@param text string
function Controller:refreshText(text) end

---@private
---@param alpha number
function Controller:_layoutOpening(alpha) end

---@param deltaTime number
function Controller:onTick(deltaTime) end

---@return boolean
function Controller:isFinished() end

--- Immediately hide the tutorial and release only its own input capture.
function Controller:close() end

function Controller:dispose() end
