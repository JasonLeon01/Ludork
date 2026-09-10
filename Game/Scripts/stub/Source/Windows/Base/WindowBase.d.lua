---@meta Source.Windows.Base.WindowBase
---
--- Provides a window skin, content area, and nested canvas hierarchy.
---@class Source.Windows.Base.WindowBase: Engine.Canvas
---@field _windowSkin                sf.Image
---@field _repeated                  boolean
---@field _windowBaseUI              Source.Windows.Base.WindowBase.Controller | nil
---@field _window                    Engine.Window
---@field content                    Engine.Canvas
---@field _visualRoot                Engine.ControlBase | nil
---@field _hasReturnBtn              boolean
---@field _returnButtonSuppressed    boolean
---@field _returnButton              Engine.Button
---@field _pauseMarkShowRequested    boolean
---@field _pauseMarkEnabled          boolean
---@field _pauseMarkVisiblePredicate function | nil
---@field _pauseMarkFrameIndex       integer
---@field _pauseMarkFrameTimer       number
---@field _pauseMark                 Engine.Image
---@field _pauseMarkTexture          sf.Texture
---@field _uiController              Source.UIBase.UiController | Source.UIBase.UiView | nil
---@field _uiDispose                 function | nil
---@field _transition                Source.UIBase.WindowTransition | nil
local WindowBase = {}

---@brief Construct a window with a skin and content area.
---
--- - @param rect The window rectangle.
--- - @param windowSkin Optional window skin image; defaults to the system windowskin.
--- - @param repeated Whether the window skin is repeated.
--- - @param deferView Whether a declarative Controller will provide the window frame and content.
---@param rect       sf.IntRect
---@param windowSkin sf.Image | nil
---@param repeated   boolean | nil
---@param deferView  boolean | nil
function WindowBase:init(rect, windowSkin, repeated, deferView) end

---@brief Return whether this window exposes its return button.
---
--- - @return True when the return button is enabled.
---@return boolean
function WindowBase:getHasReturnBtn() end

---@brief Enable or disable this window's return button.
---
--- - @param hasReturnBtn Whether the window exposes its return button.
---@param hasReturnBtn boolean
function WindowBase:setHasReturnBtn(hasReturnBtn) end

---@brief Handle a return request from the window button or another input source.
function WindowBase:onReturn() end

---@param active boolean
function WindowBase:setActive(active) end

---@brief Show or hide this window and its declarative visual root.
---
--- Nested panes created through `createChild` or `FromView` keep their visual root in the parent asset. `setVisible` applies to that root as well as the input host.
--- - @param visible Whether the window and its visual root are shown.
---@param visible boolean
function WindowBase:setVisible(visible) end

---@class Source.Windows.Base.WindowBase.PreparedView
---@field root             Engine.ControlBase
---@field windowFrame      Engine.Window
---@field content          Engine.Canvas
---@field chromeRoot       Engine.Canvas
---@field nested           boolean
---@field transitionTarget string | nil
---@field returnButton     Engine.Button | nil
---@field pauseMark        Engine.Image | nil
---@field pauseMarkTexture sf.Texture | nil

---@brief Attach a prepared controller view while retaining ownership of host chrome and transitions.
---@param controller Source.UIBase.UiController | Source.UIBase.UiView
---@param viewParts  Source.Windows.Base.WindowBase.PreparedView
function WindowBase:attachPreparedView(controller, viewParts) end

---@param animationName string | nil
---@param onReady       function | nil
function WindowBase:showWithAnimation(animationName, onReady) end

---@param animationName string | nil
---@param onHidden      function | nil
function WindowBase:hideWithAnimation(animationName, onHidden) end

function WindowBase:hideImmediate() end

---@return boolean
function WindowBase:isTransitionBlocking() end

---@return boolean
function WindowBase:isTransitionOpen() end

---@param suppressed boolean
function WindowBase:setReturnButtonSuppressed(suppressed) end

---@brief Enable or disable the pause mark display.
---
--- - @param enabled Whether the pause mark is allowed to show.
---@param enabled boolean
function WindowBase:setPauseMarkEnabled(enabled) end

---@brief Set an optional predicate that gates pause mark visibility.
---
--- - @param predicate Callable returning True when the pause mark may show, or nil to clear.
---@param predicate function | nil
function WindowBase:setPauseMarkVisiblePredicate(predicate) end

---@brief Request the pause mark to be shown (subject to enabled state and predicate).
function WindowBase:showPauseMark() end

---@brief Hide the pause mark.
function WindowBase:hidePauseMark() end

---@brief Position the pause mark at the bottom-centre of the content area.
function WindowBase:refreshPauseMarkLayout() end

---@brief Update pause mark animation.
---
--- - @param deltaTime Elapsed time in seconds.
---@param deltaTime number
function WindowBase:onTick(deltaTime) end

---@return boolean
function WindowBase:isReturnButtonEnabled() end

---@param windowFrame Engine.Window
function WindowBase:applyWindowSkin(windowFrame) end

---@return integer
function WindowBase:getPauseMarkSize() end

function WindowBase:dispose() end

---@return boolean
function WindowBase:isReturnButtonSuppressed() end

---@return Source.UIBase.WindowTransition
function WindowBase:getTransition() end

return WindowBase
