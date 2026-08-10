---@meta Source.Windows.Base.WindowSelectable
---
--- Supports keyboard and mouse navigation, scrolling, and item selection.
---@class Source.Windows.Base.WindowSelectable: Source.Windows.Base.WindowBase
---@field _oldIndex integer?
---@field index integer?
---@field _listView Engine.ListView?
---@field _rectWidth integer
---@field _rectHeight integer
---@field _hitRectWidth integer?
---@field _hitRectHeight integer?
---@field _rect Engine.Rect
---@field _ensureSelectionVisibleRequested boolean
---@field _selectionScrollIndex integer?
---@field _selectionScrollItemCount integer?
---@field _selectionScrollItem Engine.ControlBase?
---@field _selectionScrollX number?
---@field _selectionScrollY number?
---@field _selectionViewWidth number?
---@field _selectionViewHeight number?
---@field _mousePositionAtCursorPending boolean
---@field _mouseSelectionConfirmedThisFrame boolean
---@field _wheelScrollTargetOriginY number?
---@field _touchCaptured boolean
---@field _touchDragging boolean
---@field _touchStartPosition sf.Vector2f?
---@field _touchStartOriginY number
local WindowSelectable = {}

--- @brief Construct a selectable window.
---
--- - @param rect The window rectangle.
--- - @param listView Optional ListView for selectable items.
--- - @param rectWidth Optional fixed width for the selection rectangle.
--- - @param rectHeight Height of each selection item.
--- - @param windowSkin Optional window skin image.
--- - @param repeated Whether the window skin is repeated.
--- - @param hitRectWidth Override hit detection width; defaults to selection rect width.
--- - @param hitRectHeight Override hit detection height; defaults to selection rect height.
---@param rect          sf.IntRect
---@param listView      Engine.ListView | nil
---@param rectWidth     integer | nil
---@param rectHeight    integer | nil
---@param windowSkin    sf.Image | nil
---@param repeated      boolean | nil
---@param hitRectWidth  integer | nil
---@param hitRectHeight integer | nil
function WindowSelectable:init(rect, listView, rectWidth, rectHeight, windowSkin, repeated, hitRectWidth, hitRectHeight) end

--- @brief Get the current list view.
---
--- - @return The ListView, or nil.
---@return Engine.ListView | nil
function WindowSelectable:getListView() end

--- @brief Set the list view for selectable items.
---
--- - @param listView The ListView to use, or nil to clear.
---@param listView Engine.ListView | nil
function WindowSelectable:setListView(listView) end

---@param active boolean
function WindowSelectable:setActive(active) end

---@param visible boolean
function WindowSelectable:setVisible(visible) end

---@param deltaTime number
function WindowSelectable:update(deltaTime) end

--- @brief Update selection rectangle position and selection state.
---
--- - @param deltaTime Elapsed time in seconds.
---@param deltaTime number
function WindowSelectable:onTick(deltaTime) end

--- @brief Handle mouse wheel scrolling.
---
--- - @param kwargs Event data containing delta.
---@param kwargs table
function WindowSelectable:onMouseWheelScrolled(kwargs) end

--- @brief Handle mouse movement events.
---
--- - @param kwargs Event data.
---@param kwargs table
function WindowSelectable:onMouseMoved(kwargs) end

---@return boolean
function WindowSelectable:requestKeyboardFocusAtCursor() end

--- @brief Handle keyboard navigation and confirmation.
---
--- Direction keys use repeat mode: immediate first press, then
--- after ~0.4 s they fire every ~0.1 s while held.
---
--- - @param kwargs Event data.
---@param kwargs table
function WindowSelectable:onKeyDown(kwargs) end

--- @brief Handle directional cursor movement.
---
--- - @param direction Direction pressed by keyboard or gamepad.
---
--- - @return True if the direction was handled inside this window.
---@param direction string
---@return boolean
function WindowSelectable:onDirectionalKey(direction) end

---@param index integer
---@return sf.Vector2f
function WindowSelectable:_getRectPositionForIndex(index) end

---@return integer
function WindowSelectable:_getRectWidth() end

---@param item Engine.ControlBase
function WindowSelectable:_applyItem(item) end

---@param position sf.Vector2f
---@return boolean
function WindowSelectable:_shouldCaptureTouch(position) end

return WindowSelectable
