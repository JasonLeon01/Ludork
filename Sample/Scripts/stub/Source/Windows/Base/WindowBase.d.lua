---@meta Source.Windows.Base.WindowBase
---
--- Provides a window skin, content area, and nested canvas hierarchy.
---@class Source.Windows.Base.WindowBase: Engine.Canvas
---@field _windowSkin sf.Image
---@field _repeated boolean
---@field _windowBaseUI Source.UI.Parts.Shared.WindowBase
---@field _window Engine.Window
---@field content Engine.Canvas
---@field _pauseMarkShowRequested boolean
---@field _pauseMarkEnabled boolean
---@field _pauseMarkVisiblePredicate function | nil
---@field _pauseMarkFrameIndex integer
---@field _pauseMarkFrameTimer number
---@field _pauseMark Engine.Image
---@field _pauseMarkTexture sf.Texture
local WindowBase = {}

--- @brief Construct a window with a skin and content area.
---
--- - @param rect The window rectangle.
--- - @param windowSkin Optional window skin image; defaults to the system windowskin.
--- - @param repeated Whether the window skin is repeated.
---@param rect       sf.IntRect
---@param windowSkin sf.Image | nil
---@param repeated   boolean | nil
function WindowBase:init(rect, windowSkin, repeated) end

--- @brief Enable or disable the pause mark display.
---
--- - @param enabled Whether the pause mark is allowed to show.
---@param enabled boolean
function WindowBase:setPauseMarkEnabled(enabled) end

--- @brief Set an optional predicate that gates pause mark visibility.
---
--- - @param predicate Callable returning True when the pause mark may show, or nil to clear.
---@param predicate function | nil
function WindowBase:setPauseMarkVisiblePredicate(predicate) end

--- @brief Request the pause mark to be shown (subject to enabled state and predicate).
function WindowBase:showPauseMark() end

--- @brief Hide the pause mark.
function WindowBase:hidePauseMark() end

--- @brief Position the pause mark at the bottom-centre of the content area.
function WindowBase:refreshPauseMarkLayout() end

--- @brief Update pause mark animation.
---
--- - @param deltaTime Elapsed time in seconds.
---@param deltaTime number
function WindowBase:onTick(deltaTime) end

return WindowBase
