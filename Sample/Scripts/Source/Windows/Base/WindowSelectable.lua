local Engine = require("Engine")
local GlobalFunctions = require("GlobalFunctions")
local GameSystem = require("Source.System")
local UiControlFactory = require("Source.UI.UiControlFactory")
local WindowBase = require("Source.Windows.Base.WindowBase")

local Input = Engine.Input
local ControlBase = Engine.ControlBase
local FunctionalBase = Engine.FunctionalBase
local Direction = Engine.FocusDirection
local ManagerFunctions = GlobalFunctions.Manager

local _INACTIVE_SELECTION_RECT_OPACITY_MULTIPLIER = 0.35
local _REPEAT_DELAY = 0.4
local _REPEAT_INTERVAL = 0.1
local _WHEEL_SCROLL_RESPONSE = 18.0
local _WHEEL_SCROLL_EPSILON = 0.01

---@class Source.Windows.Base.WindowSelectable
local WindowSelectable = {}

function WindowSelectable:init(rect, listView, rectWidth, rectHeight, windowSkin, repeated, hitRectWidth, hitRectHeight)
    super(WindowSelectable, self).init(rect, windowSkin, repeated)
    self._oldIndex = nil
    self.index = 0
    self:setCanReceiveFocus(true)
    if listView ~= nil then
        self.content:addChild(listView)
    end
    self._listView = listView
    if rectWidth == nil then
        rectWidth = self:_getRectWidth()
    end
    ---@cast rectWidth integer
    self._rectWidth = rectWidth
    self._rectHeight = rectHeight or 32
    self._hitRectWidth = hitRectWidth
    self._hitRectHeight = hitRectHeight
    self._rect = self:_createSelectionRect()
    self._ensureSelectionVisibleRequested = true
    self._selectionScrollIndex = nil
    self._selectionScrollItemCount = nil
    self._selectionScrollItem = nil
    self._selectionScrollX = nil
    self._selectionScrollY = nil
    self._selectionViewWidth = nil
    self._selectionViewHeight = nil
    self._mousePositionAtCursorPending = false
    self._mouseSelectionConfirmedThisFrame = false
    self._wheelScrollTargetOriginY = nil
    self._touchCaptured = false
    self._touchDragging = false
    self._touchStartPosition = nil
    self._touchStartOriginY = 0.0
end

---@return Engine.Rect
function WindowSelectable:_createSelectionRect()
    local rectWidth = self._rectWidth
    local rectHeight = self._rectHeight
    ---@cast rectWidth integer
    ---@cast rectHeight integer
    local size = sf.Vector2u.new(rectWidth, rectHeight)
    ---@cast size sf.Vector2u
    local rect = UiControlFactory.createSelectionRect(size, self._windowSkin)
    local position = self:_getRectPosition()
    ---@cast position - nil
    rect:setPosition(position)
    return rect
end

function WindowSelectable:getListView()
    return self._listView
end

function WindowSelectable:setListView(listView)
    if self._listView ~= nil then
        self.content:removeChild(self._listView)
    end
    if listView ~= nil then
        self.content:addChild(listView)
    end
    self._listView = listView
    self._ensureSelectionVisibleRequested = true
end

function WindowSelectable:setActive(active)
    local wasActive = self:getActive()
    super(WindowSelectable, self).setActive(active)
    if active and not wasActive then
        self._ensureSelectionVisibleRequested = true
    elseif not active then
        self:_resetTransientInputState()
    end
end

function WindowSelectable:setVisible(visible)
    super(WindowSelectable, self).setVisible(visible)
    if visible then
        self._ensureSelectionVisibleRequested = true
    else
        self:_resetTransientInputState()
    end
end

function WindowSelectable:update(deltaTime)
    self._mouseSelectionConfirmedThisFrame = false
    local pendingTouchConfirm = nil
    if LUDORK_MOBILE then
        pendingTouchConfirm = self:_updateTouchInput()
    end
    super(WindowSelectable, self).update(deltaTime)
    if pendingTouchConfirm ~= nil then
        self:_confirmSelectionIndex(pendingTouchConfirm)
    end
end

function WindowSelectable:onTick(deltaTime)
    local active = self:getActive()
    local focused = self:_hasCursorFocus()
    if self.index ~= nil then
        if self._rectWidth ~= self:_getRectWidth() then
            self._rectWidth = self:_getRectWidth()
            if self._rect:getParent() ~= nil then
                self.content:removeChild(self._rect)
            end
            self._rect = self:_createSelectionRect()
            self._ensureSelectionVisibleRequested = true
        end
        local position = self:_getRectPosition()
        ---@cast position - nil
        self._rect:setPosition(position)
    end
    local selectionVisible = self.index ~= nil and self:_itemCount() > 0 and focused == true
    self._rect:setVisible(selectionVisible)
    if self:_shouldEnsureSelectionVisible() then
        self:_ensureSelectionVisible()
        self:_recordSelectionScrollState()
    else
        self:_updateWheelScroll(deltaTime)
    end
    self._rect:update(deltaTime)
    self._rect:setOpacityMultiplier(focused and 1.0 or _INACTIVE_SELECTION_RECT_OPACITY_MULTIPLIER)
    if self._rect:getParent() == nil then
        self._rect:resize(sf.Vector2f.new(self._rectWidth, self._rectHeight))
        self.content:addChild(self._rect)
    end
    self:_updatePendingMousePosition()
    if active and self._listView ~= nil then
        self:_confirmMouseSelection()
    end
    if LUDORK_DESKTOP and active and self._listView ~= nil and Input.isTouchTap(false) then
        local tapPos = Input.getTouchTapPosition()
        if tapPos ~= nil then
            local touchLocal = Engine.ToVector2f(tapPos)
            if self:_confirmSelectionAt(touchLocal) then
                Input.isTouchTap(true)
            end
        end
    end
    if self._oldIndex == nil then
        self._oldIndex = self.index
    end
    if self.index ~= self._oldIndex then
        self._oldIndex = self.index
        ManagerFunctions.playSE(GameSystem.getCursorSE())
    end
    super(WindowSelectable, self).onTick(deltaTime)
end

function WindowSelectable:onMouseWheelScrolled(kwargs)
    local listView = self._listView
    if listView == nil or not bool(listView:getChildren()) then
        return
    end
    local wheel = Input.getMouseScrolledWheel()
    if wheel ~= nil and wheel ~= sf.Mouse.Wheel.Vertical then
        return
    end
    local delta = kwargs.delta
    if delta == 0 then
        return
    end
    if Input.isMouseWheelPrecise() then
        self._wheelScrollTargetOriginY = nil
        self:_setScrollOriginY(self:_getScrollOriginY() - delta / math.max(Engine.Scale, 0.000001))
        return
    end
    local targetOriginY = self._wheelScrollTargetOriginY or self:_getScrollOriginY()
    self._wheelScrollTargetOriginY = self:_clampScrollOriginY(targetOriginY - delta * self._rectHeight)
end

function WindowSelectable:onMouseMoved(kwargs)
    if self._mouseSelectionConfirmedThisFrame or not self:canReceiveFocus() or self._listView == nil
        or not Input.isMouseInputMode() or not Input.isMouseMoved() then
        return
    end
    local position = sf.Vector2f.new(kwargs.position.x, kwargs.position.y)
    self:requestKeyboardFocus()
    local index = self:_getSelectionAt(position)
    if index ~= nil then
        self:_setPointerIndex(index)
    end
end

function WindowSelectable:requestKeyboardFocusAtCursor()
    local focused = self:requestKeyboardFocus()
    if not focused then
        return false
    end
    self._ensureSelectionVisibleRequested = true
    if LUDORK_DESKTOP and Input.isMouseInputMode() then
        self._mousePositionAtCursorPending = true
    end
    return true
end

---@diagnostic disable-next-line: unused
function WindowSelectable:onKeyDown(kwargs)
    local listView = self._listView
    if listView == nil or not bool(listView:getChildren()) or self.index == nil then
        return
    end
    if Input.isActionTriggered(Input.getConfirmKeys(), false) then
        local children = listView:getChildren()
        if self.index >= 0 and self.index < #children then
            local child = children[self.index + 1]
            if Class.isInstance(child, FunctionalBase) then
                ---@cast child Engine.ControlBase & Engine.FunctionalBase
                child:onConfirm({})
                Input.isActionTriggered(Input.getConfirmKeys(), true)
            end
        end
        return
    end
    if self:_handleDirectionalAction(Direction.UP, Input.getUpKeys()) then
        return
    end
    if self:_handleDirectionalAction(Direction.DOWN, Input.getDownKeys()) then
        return
    end
    if self:_handleDirectionalAction(Direction.LEFT, Input.getLeftKeys()) then
        return
    end
    self:_handleDirectionalAction(Direction.RIGHT, Input.getRightKeys())
end

function WindowSelectable:onDirectionalKey(direction)
    if self.index == nil or self:_itemCount() <= 0 then
        return false
    end
    local columns = self:_getColumns()
    if direction == Direction.UP then
        if columns == 1 then
            local index = (self.index - 1) % self:_itemCount()
            ---@cast index integer
            if self.index ~= index then
                self.index = index
                self:_ensureSelectionVisible()
                self:_synchronizeSelectionScrollState()
            end
            return true
        end
        local index = math.max(0, self.index - columns)
        ---@cast index integer
        return self:_setIndexIfChanged(index)
    end
    if direction == Direction.DOWN then
        if columns == 1 then
            local index = (self.index + 1) % self:_itemCount()
            ---@cast index integer
            if self.index ~= index then
                self.index = index
                self:_ensureSelectionVisible()
                self:_synchronizeSelectionScrollState()
            end
            return true
        end
        return self:_setIndexIfChanged(math.min(self:_itemCount() - 1, self.index + columns))
    end
    if direction == Direction.LEFT then
        if columns == 1 or self.index % columns == 0 then
            return false
        end
        return self:_setIndexIfChanged(self.index - 1)
    end
    if direction == Direction.RIGHT then
        if columns == 1 or self.index % columns == columns - 1 or self.index + 1 >= self:_itemCount() then
            return false
        end
        return self:_setIndexIfChanged(self.index + 1)
    end
    return false
end

-- Compute the selection rectangle position for a given item index.
---
--- - @param index  Zero-based item index.
--- - @return  Top-left position of the selection rectangle in content space.
---@param index integer
---@return sf.Vector2f
function WindowSelectable:_getRectPositionForIndex(index)
    local columns = self:_getColumns()
    local x = index % columns * self._rectWidth + 16
    local y = math.floor(index / columns) * self._rectHeight
    return sf.Vector2f.new(x, y)
end

---@return sf.Vector2f | nil
function WindowSelectable:_getRectPosition()
    if self.index == nil then
        return nil
    end
    return self:_getRectPositionForIndex(self.index)
end

-- Get the hit detection size for each selectable item.
---
--- - @return  Width and height used for mouse/touch hit testing.
---@return sf.Vector2i
function WindowSelectable:_getItemHitSize()
    local width = self._hitRectWidth or self._rectWidth
    local height = self._hitRectHeight or self._rectHeight
    ---@cast width integer
    ---@cast height integer
    local size = sf.Vector2i.new(width, height)
    ---@cast size sf.Vector2i
    return size
end

-- Get the screen-space hit rectangle for a given item index.
---
--- Temporarily repositions the selection rect sprite to the target cell,
--- reads absolute screen bounds through the content canvas transform
--- (including scroll and Scale), then restores the original position.
---
--- - @param index  Zero-based item index.
--- - @return  Absolute screen-space bounds used for hit testing.
---@param index integer
---@return sf.FloatRect
function WindowSelectable:_getItemHitAbsoluteBounds(index)
    local savedPos = self._rect:getPosition()
    self._rect:setPosition(self:_getRectPositionForIndex(index))
    local rectAbs = self._rect:getAbsoluteBounds()
    self._rect:setPosition(savedPos)
    local hitSize = self:_getItemHitSize()
    if hitSize.x == self._rectWidth and hitSize.y == self._rectHeight then
        return rectAbs
    end
    local scaleX = self._rectWidth ~= 0 and rectAbs.size.x / self._rectWidth or 1.0
    local scaleY = self._rectHeight ~= 0 and rectAbs.size.y / self._rectHeight or 1.0
    return sf.FloatRect.new(rectAbs.position, sf.Vector2f.new(hitSize.x * scaleX, hitSize.y * scaleY))
end

---@return integer
function WindowSelectable:_getRectWidth()
    local columns = self:_getColumns()
    return math.floor((self.content:getSize().x - 32) / columns)
end

---@return integer
function WindowSelectable:_itemCount()
    if self._listView == nil then
        return 0
    end
    return #self._listView:getChildren()
end

---@return integer
function WindowSelectable:_getColumns()
    if self._listView == nil then
        return 1
    end
    return self._listView:getColumns()
end

---@return boolean
function WindowSelectable:_hasCursorFocus()
    if self:ownsKeyboardCursorFocus() then
        return true
    end
    return self:getActive() and self:shouldDispatchKeyboardInput()
end

---@param direction  string
---@param actionKeys table
---@return boolean
function WindowSelectable:_handleDirectionalAction(direction, actionKeys)
    if not Input.isActionTriggered(actionKeys, false, _REPEAT_DELAY, _REPEAT_INTERVAL) then
        return false
    end
    local handled = self:onDirectionalKey(direction)
    if not handled then
        handled = self:requestDirectionalFocusMove(direction) == true
    end
    if handled then
        Input.isActionTriggered(actionKeys, true, _REPEAT_DELAY, _REPEAT_INTERVAL)
    end
    return handled
end

---@param index integer
---@return boolean
function WindowSelectable:_setIndexIfChanged(index)
    if self.index == index then
        return false
    end
    self.index = index
    self:_ensureSelectionVisible()
    self:_synchronizeSelectionScrollState()
    return true
end

---@param index integer
function WindowSelectable:_setPointerIndex(index)
    self.index = index
    self:_synchronizeSelectionScrollState()
end

---@param item Engine.ControlBase
---@diagnostic disable-next-line: unused
function WindowSelectable:_applyItem(item)
    if Class.isInstance(item, ControlBase) then
        local bounds = item:getLocalBounds()
        local origin = sf.Vector2f.new(bounds.position.x + bounds.size.x / 2, 0)
        item:setOrigin(origin)
    end
end

function WindowSelectable:_getScrollOriginY()
    local view = self.content:getView()
    return view:getCenter().y - view:getSize().y / 2.0
end

---@return number
function WindowSelectable:_getMaxScrollOriginY()
    local itemCount = self:_itemCount()
    if itemCount <= 0 then
        return 0.0
    end
    local position = self:_getRectPositionForIndex(itemCount - 1)
    local viewHeight = self.content:getView():getSize().y
    return math.max(0.0, position.y + self._rectHeight - viewHeight)
end

---@param originY number
function WindowSelectable:_setScrollOriginY(originY)
    local view = self.content:getView()
    local viewSize = view:getSize()
    local origin = view:getCenter() - viewSize / 2.0
    local clampedOriginY = self:_clampScrollOriginY(originY)
    self.content:setView(sf.View.new(sf.Vector2f.new(origin.x, clampedOriginY) + viewSize / 2.0, viewSize))
end

---@param originY number
---@return number
function WindowSelectable:_clampScrollOriginY(originY)
    return math.min(self:_getMaxScrollOriginY(), math.max(0.0, originY))
end

---@param deltaTime number
function WindowSelectable:_updateWheelScroll(deltaTime)
    if self._wheelScrollTargetOriginY == nil then
        return
    end
    local pendingOriginY = self._wheelScrollTargetOriginY
    local targetOriginY = self:_clampScrollOriginY(pendingOriginY)
    self._wheelScrollTargetOriginY = targetOriginY
    local originY = self:_getScrollOriginY()
    local distance = targetOriginY - originY
    if math.abs(distance) <= _WHEEL_SCROLL_EPSILON then
        self:_setScrollOriginY(targetOriginY)
        self._wheelScrollTargetOriginY = nil
        return
    end
    local factor = 1.0 - math.exp(-_WHEEL_SCROLL_RESPONSE * math.max(0.0, deltaTime))
    self:_setScrollOriginY(originY + distance * factor)
end

function WindowSelectable:_ensureSelectionVisible()
    self._wheelScrollTargetOriginY = nil
    local originY = self:_getScrollOriginY()
    if self.index == nil or self.index < 0 or self.index >= self:_itemCount() then
        self:_setScrollOriginY(originY)
        return
    end
    local view = self.content:getView()
    local viewSize = view:getSize()
    local position = self:_getRectPositionForIndex(self.index)
    if position.y < originY then
        originY = position.y
    elseif position.y + self._rectHeight > originY + viewSize.y then
        originY = position.y + self._rectHeight - viewSize.y
    end
    self:_setScrollOriginY(originY)
end

---@return boolean
function WindowSelectable:_shouldEnsureSelectionVisible()
    local position = self:_getRectPosition()
    local viewSize = self.content:getView():getSize()
    return self._ensureSelectionVisibleRequested or self._selectionScrollIndex ~= self.index
        or self._selectionScrollItemCount ~= self:_itemCount()
        or self._selectionScrollItem ~= self:_getSelectionScrollItem() or self._rect:getParent() == nil
        or position ~= nil and (self._selectionScrollX ~= position.x or self._selectionScrollY ~= position.y)
        or self._selectionViewWidth ~= viewSize.x or self._selectionViewHeight ~= viewSize.y
end

---@return Engine.ControlBase | nil
function WindowSelectable:_getSelectionScrollItem()
    if self._listView == nil or self.index == nil or self.index < 0 then
        return nil
    end
    return self._listView:getChildren()[self.index + 1]
end

function WindowSelectable:_recordSelectionScrollState()
    self._ensureSelectionVisibleRequested = false
    self:_synchronizeSelectionScrollState()
end

function WindowSelectable:_synchronizeSelectionScrollState()
    local position = self:_getRectPosition()
    local viewSize = self.content:getView():getSize()
    self._selectionScrollIndex = self.index
    self._selectionScrollItemCount = self:_itemCount()
    self._selectionScrollItem = self:_getSelectionScrollItem()
    self._selectionScrollX = position ~= nil and position.x or nil
    self._selectionScrollY = position ~= nil and position.y or nil
    self._selectionViewWidth = viewSize.x
    self._selectionViewHeight = viewSize.y
end

function WindowSelectable:_updatePendingMousePosition()
    if not self._mousePositionAtCursorPending then
        return
    end
    self._mousePositionAtCursorPending = false
    if LUDORK_MOBILE or not Input.isMouseInputMode()
        or not self:_hasCursorFocus() or self.index == nil
        or self.index < 0 or self.index >= self:_itemCount() then
        return
    end
    local bounds = self._rect:getAbsoluteBounds()
    local centre = bounds.position + bounds.size / 2.0
    Input.setMousePosition(Engine.ToVector2i(centre))
end

---@return integer | nil
function WindowSelectable:_updateTouchInput()
    if not self:getActive() or self._listView == nil then
        self:_resetTouchCapture(true)
        return nil
    end
    if Input.isTouchBegan(false) then
        local beganPosition = Input.getTouchBeganPosition()
        if beganPosition ~= nil then
            local position = Engine.ToVector2f(beganPosition)
            if sf.FloatRect.contains(self:_getContentViewportAbsoluteBounds(), position) then
                self._wheelScrollTargetOriginY = nil
                if self:_shouldCaptureTouch(position) then
                    self._touchCaptured = true
                    self._touchDragging = false
                    self._touchStartPosition = position
                    self._touchStartOriginY = self:_getScrollOriginY()
                    Input.isTouchBegan(true)
                end
            end
        end
    end
    if not self._touchCaptured then
        return nil
    end
    local touchPosition = Input.getTouchPosition()
    if Input.isTouchMoved() and touchPosition ~= nil then
        local position = Engine.ToVector2f(touchPosition)
        local scale = math.max(Engine.Scale, 0.000001)
        if Input.isTouchDragged() then
            self._touchDragging = true
        end
        if self._touchDragging then
            local touchStartPosition = self._touchStartPosition
            ---@cast touchStartPosition sf.Vector2f
            local originDeltaY = (position.y - touchStartPosition.y) / scale
            self:_setScrollOriginY(self._touchStartOriginY - originDeltaY)
        end
    end
    if not Input.isTouchEnded() then
        if not Input.isTouchActive() then
            self:_resetTouchCapture(true)
        end
        return nil
    end
    local isTap = Input.isTouchTap(false)
    local pendingTouchConfirm = nil
    if isTap and not self._touchDragging then
        Input.isTouchTap(true)
        local tapPosition = Input.getTouchTapPosition()
        if tapPosition ~= nil then
            local index = self:_getSelectionAt(Engine.ToVector2f(tapPosition))
            if index ~= nil then
                if self.index == index then
                    pendingTouchConfirm = index
                else
                    self:_setPointerIndex(index)
                end
            end
        end
    end
    self:_resetTouchCapture()
    return pendingTouchConfirm
end

---@diagnostic disable-next-line: unused
function WindowSelectable:_shouldCaptureTouch(position)
    return true
end

---@return sf.FloatRect
function WindowSelectable:_getContentViewportAbsoluteBounds()
    local view = self.content:getView()
    self.content:setView(self.content:getDefaultView())
    local bounds = self.content:getAbsoluteBounds()
    self.content:setView(view)
    return bounds
end

---@param cancelGesture boolean | nil
function WindowSelectable:_resetTouchCapture(cancelGesture)
    if cancelGesture == true and self._touchCaptured then
        Input.cancelTouchGesture()
    end
    self._touchCaptured = false
    self._touchDragging = false
    self._touchStartPosition = nil
    self._touchStartOriginY = 0.0
end

function WindowSelectable:_resetTransientInputState()
    self._mousePositionAtCursorPending = false
    self._mouseSelectionConfirmedThisFrame = false
    self._wheelScrollTargetOriginY = nil
    self:_resetTouchCapture(true)
end

---@return boolean
function WindowSelectable:_confirmMouseSelection()
    if not Input.isMouseInputMode() then
        return false
    end
    if not Input.isMouseButtonTriggered(sf.Mouse.Button.Left, false) then
        return false
    end
    if self:_confirmSelectionAt(Engine.ToVector2f(Input.getMousePosition())) then
        self._mouseSelectionConfirmedThisFrame = true
        Input.isMouseButtonTriggered(sf.Mouse.Button.Left, true)
        return true
    end
    return false
end

---@param position sf.Vector2f
---@return boolean
function WindowSelectable:_confirmSelectionAt(position)
    local index = self:_getSelectionAt(position)
    if index == nil then
        return false
    end
    self:_setPointerIndex(index)
    return self:_confirmSelectionIndex(index)
end

---@param position sf.Vector2f
---@return integer | nil
function WindowSelectable:_getSelectionAt(position)
    if self._listView == nil or not sf.FloatRect.contains(self:_getContentViewportAbsoluteBounds(), position) then
        return nil
    end
    for luaIndex, child in ipairs(self._listView:getChildren()) do
        if Class.isInstance(child, ControlBase)
            and sf.FloatRect.contains(self:_getItemHitAbsoluteBounds(luaIndex - 1), position) then
            return luaIndex - 1
        end
    end
    return nil
end

---@param index integer
---@return boolean
function WindowSelectable:_confirmSelectionIndex(index)
    if self._listView == nil then
        return false
    end
    local children = self._listView:getChildren()
    if index < 0 or index >= #children then
        return false
    end
    local child = children[index + 1]
    if not Class.isInstance(child, FunctionalBase) then
        return false
    end
    ---@cast child Engine.ControlBase & Engine.FunctionalBase
    child:onConfirm({})
    return true
end

return class(WindowSelectable, WindowBase)
