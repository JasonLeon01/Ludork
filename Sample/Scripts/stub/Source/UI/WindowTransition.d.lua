---@meta Source.UI.WindowTransition

---@class Source.UI.WindowTransition
---@field DEFAULT        string
---@field MENU           string
---@field _host          Engine.Canvas
---@field _ui            Source.UI.UiController
---@field _target        string | nil
---@field _phase         "open" | "hidden" | "entering" | "exiting"
---@field _generation    integer
---@field _animationName string | nil
local WindowTransition = {}

---@param profile? string
---@return string fadeIn
---@return string fadeOut
function WindowTransition.GetAnimationNames(profile) end

---@param host    Engine.Canvas
---@param ui      Source.UI.UiController
---@param target? string
---@return Source.UI.WindowTransition
function WindowTransition.new(host, ui, target) end

---@param requestGeneration integer
---@return boolean
function WindowTransition:_isCurrent(requestGeneration) end

---@param animationName string
---@param onReady?      function
function WindowTransition:show(animationName, onReady) end

---@param animationName string
---@param onHidden?     function
function WindowTransition:hide(animationName, onHidden) end

function WindowTransition:hideImmediate() end

---@return boolean
function WindowTransition:isBlocking() end

---@return boolean
function WindowTransition:isOpen() end

return WindowTransition
