local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GameSystem = require("Source.System")
local WindowBase = require("Internal.UIBase.WindowBase")

local Input = Engine.Input
local FunctionalBase = Engine.FunctionalBase
local Direction = Engine.FocusDirection
local AudioManager = GlobalCore.AudioManager

local _INACTIVE_SELECTION_RECT_OPACITY_MULTIPLIER = 0.35
local _REPEAT_DELAY = 0.4
local _REPEAT_INTERVAL = 0.1

---@class Internal.UIBase.WindowSelectable
local WindowSelectable = {}

function WindowSelectable:init(rect)
    super(WindowSelectable, self).init(rect)
    self._oldIndex = nil
    self.index = 0
    self:setCanReceiveFocus(true)
    self._scrollBox = nil
    self._listView = nil
    self._rectWidth = 1
    self._rectHeight = 1
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
    self._selectionInputPaused = false
    self._touchCaptured = false
    self._touchDragging = false
    self._touchStartPosition = nil
    self._touchStartScrollOffset = sf.Vector2f.new(0.0, 0.0)
end

---@return Engine.Rect
function WindowSelectable:_createSelectionRect()
    return Engine.Rect.new(Engine.ToIntRect(0, 0, 1, 1), self._windowSkin, Engine.Rect.SelectionRectOpacityCurveKey)
end

function WindowSelectable:detachSelectionRect()
    local parent = self._rect:getParent()
    if parent ~= nil then
        ---@cast parent Engine.Canvas
        parent:removeChild(self._rect)
    end
end

function WindowSelectable:getListView()
    return self._listView
end

function WindowSelectable:getScrollBox()
    return self._scrollBox
end

---@param scrollBox Engine.ScrollBox
function WindowSelectable:setScrollBox(scrollBox)
    if self._scrollBox == scrollBox then
        return
    end
    self:detachSelectionRect()
    self._scrollBox = scrollBox
    scrollBox:setScrollingEnabled(not self._selectionInputPaused)
end

function WindowSelectable:setListView(listView)
    self:detachSelectionRect()
    self._listView = listView
    self._ensureSelectionVisibleRequested = true
end

function WindowSelectable:resetSelection()
    self.index = self:_itemCount() > 0 and 0 or nil
    self._oldIndex = self.index
    if self._scrollBox ~= nil then
        self._scrollBox:setScrollOffset(sf.Vector2f.new(0.0, 0.0))
    end
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
    local active = self:canReceiveFocus()
    local focused = self:_hasCursorFocus()
    if self.index ~= nil and self.index >= 0 and self.index < self:_itemCount() then
        local bounds = self:getSelectionLayoutRect(self.index)
        bounds.size = bounds.size:componentWiseMul(assert(self._listView):getScale())
        if self._rectWidth ~= bounds.size.x or self._rectHeight ~= bounds.size.y then
            self._rectWidth = bounds.size.x
            self._rectHeight = bounds.size.y
            self._rect:resize(bounds.size)
            self._ensureSelectionVisibleRequested = true
        end
        self._rect:setPosition(self:_getRectPositionForIndex(self.index))
    end
    local selectionVisible = not self._selectionInputPaused and self.index ~= nil
        and self:_itemCount() > 0 and focused == true
    self._rect:setVisible(selectionVisible)
    if self:_shouldEnsureSelectionVisible() then
        self:_ensureSelectionVisible()
        self:_recordSelectionScrollState()
    end
    self._rect:update(deltaTime)
    self._rect:setOpacityMultiplier(focused and 1.0 or _INACTIVE_SELECTION_RECT_OPACITY_MULTIPLIER)
    if self._rect:getParent() == nil then
        self._rect:resize(sf.Vector2f.new(self._rectWidth, self._rectHeight))
        local target = self._scrollBox or self.content
        target:addChild(self._rect)
    end
    self:_updatePendingMousePosition()
    if active and not self._selectionInputPaused and self._listView ~= nil then
        self:_confirmMouseSelection()
    end
    if LUDORK_DESKTOP and active
        and not self._selectionInputPaused and self._listView ~= nil
        and Input.isTouchTap(false) then
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
        AudioManager.playSound(GameSystem.GetCursorSE())
    end
    super(WindowSelectable, self).onTick(deltaTime)
end

function WindowSelectable:onMouseMoved(kwargs)
    if self._selectionInputPaused or self._mouseSelectionConfirmedThisFrame or not self:canReceiveFocus()
        or self._listView == nil or not Input.isMouseInputMode() or not Input.isMouseMoved() then
        return
    end
    if kwargs.position == nil then
        return
    end
    local position = kwargs.position
    self:requestKeyboardFocus()
    local index = self:_getSelectionAt(position)
    if index ~= nil then
        self:_setPointerIndex(index)
    end
end

function WindowSelectable:requestKeyboardFocusAtCursor()
    if self._selectionInputPaused then
        return false
    end
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
    if not self:canReceiveFocus() then
        return
    end
    if Input.isActionTriggered(Input.getCancelKeys(), false) then
        self:onReturn()
        Input.isActionTriggered(Input.getCancelKeys(), true)
        return
    end
    if self._selectionInputPaused then
        return
    end
    if self._listView == nil or not bool(self._listView:getChildren()) or self.index == nil then
        return
    end
    if Input.isActionTriggered(Input.getConfirmKeys(), false) then
        local children = self._listView:getChildren()
        if self.index >= 0 and self.index < #children then
            local child = children[self.index + 1]
            if Class.isInstance(child, FunctionalBase) then
                ---@cast child Engine.ControlBase & Engine.FunctionalBase
                child:onConfirm(Engine.UiInputEventArguments.new({}))
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

function WindowSelectable:onMouseButtonDown(kwargs)
    if not self:canReceiveFocus() then
        return false
    end
    if kwargs.button == sf.Mouse.Button.Right then
        self:onReturn()
        return true
    end
    return false
end

function WindowSelectable:onDirectionalKey(direction)
    if self._selectionInputPaused or not self:canReceiveFocus() or self.index == nil or self:_itemCount() <= 0 then
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

---@param index integer
---@return sf.Vector2f
function WindowSelectable:_getRectPositionForIndex(index)
    local list = assert(self._listView)
    local bounds = self:getSelectionLayoutRect(index)
    return list:getPosition() + (bounds.position - list:getOrigin()):componentWiseMul(list:getScale())
end

function WindowSelectable:getSelectionLayoutRect(index)
    return assert(self._listView):getItemLayoutRect(index)
end

---@return sf.Vector2f | nil
function WindowSelectable:_getRectPosition()
    if self.index == nil or self.index < 0 or self.index >= self:_itemCount() then
        return nil
    end
    return self:_getRectPositionForIndex(self.index)
end

---@param index integer
---@return sf.FloatRect
function WindowSelectable:_getItemHitAbsoluteBounds(index)
    local savedPos = self._rect:getPosition()
    local savedSize = sf.Vector2f.new(self._rectWidth, self._rectHeight)
    self._rect:setPosition(self:_getRectPositionForIndex(index))
    self._rect:resize(self:getSelectionLayoutRect(index).size:componentWiseMul(assert(self._listView):getScale()))
    local bounds = self._rect:getAbsoluteBounds()
    self._rect:setPosition(savedPos)
    self._rect:resize(savedSize)
    return bounds
end

---@return integer
function WindowSelectable:getItemWidth()
    return math.floor(assert(self._listView):getDefaultItemSize().x)
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
    if self._selectionInputPaused or not self:canReceiveFocus() then
        return false
    end
    if self:ownsKeyboardCursorFocus() then
        return true
    end
    return self:shouldDispatchKeyboardInput()
end

---@param direction  string
---@param actionKeys table
---@return boolean
function WindowSelectable:_handleDirectionalAction(direction, actionKeys)
    if self._selectionInputPaused or not self:canReceiveFocus() then
        return false
    end
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

function WindowSelectable:_getScrollOriginY()
    return self._scrollBox ~= nil and self._scrollBox:getScrollOffset().y or 0.0
end

---@return number
function WindowSelectable:_getMaxScrollOriginY()
    return self._scrollBox ~= nil and self._scrollBox:getMaxScrollOffset().y or 0.0
end

---@param originY number
function WindowSelectable:_setScrollOriginY(originY)
    if self._scrollBox == nil then
        return
    end
    local offset = self._scrollBox:getScrollOffset()
    self._scrollBox:setScrollOffset(sf.Vector2f.new(offset.x, originY))
end

---@param originY number
---@return number
function WindowSelectable:_clampScrollOriginY(originY)
    return math.clamp(originY, 0.0, self:_getMaxScrollOriginY())
end

function WindowSelectable:_ensureSelectionVisible()
    if self._scrollBox == nil or self.index == nil or self.index < 0 or self.index >= self:_itemCount() then
        return
    end
    local item = self:_getSelectionScrollItem()
    if item ~= nil then
        self._scrollBox:scrollDescendantIntoView(item)
    end
end

---@return boolean
function WindowSelectable:_shouldEnsureSelectionVisible()
    local position = self:_getRectPosition()
    local viewSize = self._scrollBox ~= nil and self._scrollBox:getSize() or self.content:getSize()
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
    local viewSize = self._scrollBox ~= nil and self._scrollBox:getSize() or self.content:getSize()
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
    if self._selectionInputPaused or LUDORK_MOBILE or not Input.isMouseInputMode() or not self:_hasCursorFocus()
        or self.index == nil or self.index < 0 or self.index >= self:_itemCount() then
        return
    end
    local bounds = self._rect:getAbsoluteBounds()
    local centre = bounds.position + bounds.size / 2.0
    Input.setMousePosition(Engine.ToVector2i(centre))
end

---@return integer | nil
function WindowSelectable:_updateTouchInput()
    if not self:canReceiveFocus() or self._selectionInputPaused or self._listView == nil then
        self:_resetTouchCapture(true)
        return nil
    end
    if Input.isTouchBegan(false) then
        local beganPosition = Input.getTouchBeganPosition()
        if beganPosition ~= nil then
            local position = Engine.ToVector2f(beganPosition)
            if sf.FloatRect.contains(self:_getContentViewportAbsoluteBounds(), position) then
                if self:_shouldCaptureTouch(position) then
                    self._touchCaptured = true
                    self._touchDragging = false
                    self._touchStartPosition = position
                    self._touchStartScrollOffset = self._scrollBox ~= nil and self._scrollBox:getScrollOffset()
                        or sf.Vector2f.new(0.0, 0.0)
                    self:_onCapturedTouchBegan(position)
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
        local scale = math.max(Engine.GetScale(), 0.000001)
        if Input.isTouchDragged() then
            self._touchDragging = true
        end
        if self._touchDragging then
            if not self:_handleCapturedTouchDrag(position) then
                ---@cast self._touchStartPosition sf.Vector2f
                if self._scrollBox ~= nil then
                    local delta = (position - self._touchStartPosition) / scale
                    self._scrollBox:setScrollOffset(self._touchStartScrollOffset - delta)
                end
            end
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
            local position = Engine.ToVector2f(tapPosition)
            if not self:_handleCapturedTouchTap(position) then
                local index = self:_getSelectionAt(position)
                if index ~= nil then
                    if self.index == index then
                        pendingTouchConfirm = index
                    else
                        self:_setPointerIndex(index)
                    end
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

---@param position sf.Vector2f
---@diagnostic disable-next-line: unused
function WindowSelectable:_onCapturedTouchBegan(position)
end

---@param position sf.Vector2f
---@return boolean
---@diagnostic disable-next-line: unused
function WindowSelectable:_handleCapturedTouchDrag(position)
    return false
end

---@param position sf.Vector2f
---@return boolean
---@diagnostic disable-next-line: unused
function WindowSelectable:_handleCapturedTouchTap(position)
    return false
end

---@diagnostic disable-next-line: unused
function WindowSelectable:_onCapturedTouchReset()
end

function WindowSelectable:onPointerInteractionReset()
    self:_resetTouchCapture(true)
    super(WindowSelectable, self).onPointerInteractionReset()
end

---@return sf.FloatRect
function WindowSelectable:_getContentViewportAbsoluteBounds()
    return self._scrollBox ~= nil and self._scrollBox:getAbsoluteBounds() or self.content:getAbsoluteBounds()
end

---@param cancelGesture boolean | nil
function WindowSelectable:_resetTouchCapture(cancelGesture)
    if cancelGesture == true and self._touchCaptured then
        Input.cancelTouchGesture()
    end
    self._touchCaptured = false
    self._touchDragging = false
    self._touchStartPosition = nil
    self._touchStartScrollOffset = sf.Vector2f.new(0.0, 0.0)
    self:_onCapturedTouchReset()
end

---@param paused boolean
function WindowSelectable:_setSelectionInputPaused(paused)
    if self._selectionInputPaused == paused then
        return
    end
    self._selectionInputPaused = paused
    if self._scrollBox ~= nil then
        self._scrollBox:setScrollingEnabled(not paused)
    end
    if paused then
        self:_resetTransientInputState()
    else
        self._ensureSelectionVisibleRequested = true
    end
end

function WindowSelectable:_resetTransientInputState()
    self._mousePositionAtCursorPending = false
    self._mouseSelectionConfirmedThisFrame = false
    self:_resetTouchCapture(true)
end

---@return boolean
function WindowSelectable:_confirmMouseSelection()
    if self._selectionInputPaused or not self:canReceiveFocus() or not Input.isMouseInputMode() then
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
    if self._selectionInputPaused or not self:canReceiveFocus() then
        return false
    end
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
        if Class.isInstance(child, Engine.ControlBase)
            and sf.FloatRect.contains(self:_getItemHitAbsoluteBounds(luaIndex - 1), position) then
            return luaIndex - 1
        end
    end
    return nil
end

---@param index integer
---@return boolean
function WindowSelectable:_confirmSelectionIndex(index)
    if self._selectionInputPaused or not self:canReceiveFocus() or self._listView == nil then
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
    child:onConfirm(Engine.UiInputEventArguments.new({}))
    return true
end

function WindowSelectable:hideSelectionCursor()
    self._rect:setVisible(false)
end

function WindowSelectable:selectIndex(index, ensureVisible)
    self.index = index
    self._oldIndex = index
    if ensureVisible ~= nil then
        self._ensureSelectionVisibleRequested = ensureVisible
    end
end

function WindowSelectable:setSelectionInputPaused(paused)
    self:_setSelectionInputPaused(paused)
end

function WindowSelectable:isSelectionInputPaused()
    return self._selectionInputPaused
end

function WindowSelectable:getSelectionRowHeight()
    return math.ceil(assert(self._listView):getDefaultItemSize().y)
end

function WindowSelectable:setPointerIndex(index)
    WindowSelectable._setPointerIndex(self, index)
end

function WindowSelectable:shouldCaptureTouch(position)
    return WindowSelectable._shouldCaptureTouch(self, position)
end

function WindowSelectable:changeSelection(index)
    return self:_setIndexIfChanged(index)
end

return class(WindowSelectable, WindowBase)
