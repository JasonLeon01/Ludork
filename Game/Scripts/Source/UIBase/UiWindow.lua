local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local WindowBase = require("Source.Windows.Base.WindowBase")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")

---@class Source.UIBase.UiWindow
local UiWindow = {}

function UiWindow:init(host, ui, nativeBase, options, nested)
    self.host = host
    self.ui = ui
    self.options = options
    self.nested = nested
    self._selectable = false
    self._position = nil
    self._manager = nil
    local size = ui.root:getSize()
    local position = nested and ui.root:getPosition() or options.position or sf.Vector2f.new(0, 0)
    if options.screen and not nested then
        local gameSize = GlobalCore.System.getGameSize()
        size = sf.Vector2f.new(gameSize.x, gameSize.y)
    elseif options.centered and not nested then
        local gameSize = GlobalCore.System.getGameSize()
        position = sf.Vector2f.new(math.floor((gameSize.x - size.x) / 2), math.floor((gameSize.y - size.y) / 2))
    end
    local rect = Engine.ToIntRect(math.floor(position.x), math.floor(position.y), math.ceil(size.x), math.ceil(size.y))
    if Class.isSubclass(nativeBase, WindowSelectable) then
        local content = assert(ui.controls[options.content or "Content"])
        local itemWidth = options.itemWidth or math.max(1, math.floor(content:getSize().x - 32))
        WindowSelectable.init(
            host, rect, nil, itemWidth, options.itemHeight, nil, nil, options.hitWidth, options.hitHeight, true
        )
        self._selectable = true
    elseif Class.isSubclass(nativeBase, WindowBase) then
        WindowBase.init(host, rect, nil, nil, true)
    else
        nativeBase.init(host, rect)
    end
end

function UiWindow:attach()
    if Class.isInstance(self.host, WindowBase) then
        ---@cast self.host Source.Windows.Base.WindowBase
        local frame = assert(self.ui.controls[self.options.frame or "WindowFrame"])
        local content = assert(self.ui.controls[self.options.content or "Content"])
        ---@cast frame Engine.Window
        ---@cast content Engine.Canvas
        self.ui:attachPreparedWindow(self.host, frame, content, self.nested, self.options.transitionTarget)
        self.transition = self.host:getTransition()
        self.host:setHasReturnBtn(self.options.returnButton == true)
    else
        if not self.nested then
            self.host:addChild(self.ui.root)
        end
        self.transition = self.ui:createTransition(self.host, self.options.transitionTarget)
    end
    if self._selectable then
        ---@cast self.host Source.Windows.Base.WindowSelectable
        if self.options.scroll ~= nil then
            local scroll = assert(self.ui.controls[self.options.scroll])
            ---@cast scroll Engine.ScrollBox
            self.host:setScrollBox(scroll)
        end
        if self.options.list ~= nil then
            local list = assert(self.ui.controls[self.options.list])
            ---@cast list Engine.ListView
            self.host:setListView(list, self.options.directContent)
        end
    end
    if self.options.focusable ~= nil then
        self.host:setCanReceiveFocus(self.options.focusable)
    end
    if self.nested then
        local parent = assert(self.ui.root:getParent(), "Child UI View must already be attached")
        ---@cast parent Engine.Canvas
        parent:addChild(self.host)
        if Class.isInstance(self.ui.root, Engine.Canvas) then
            ---@cast self.ui.root Engine.Canvas
            self.host:setZOrder(self.ui.root:getZOrder())
        end
    end
    self.ui:bindWindow(self)
end

function UiWindow:syncLayout()
    if not self.nested then
        return
    end
    if self._position ~= nil then
        self.ui.root:setPosition(self._position)
    end
    Engine.ControlBase.setPosition(self.host, self.ui.root:getPosition())
    local size = self.ui.root:getSize()
    local hostSize = self.host:getSize()
    if size.x ~= hostSize.x or size.y ~= hostSize.y then
        local logicalSize = sf.Vector2u.new(math.ceil(size.x), math.ceil(size.y))
        ---@cast logicalSize sf.Vector2u
        self.host:resize(logicalSize)
        self.host:setView(self.host:getDefaultView())
    end
end

function UiWindow:setPosition(position)
    if self.nested then
        self._position = position
        self.ui.root:setPosition(position)
    end
    Engine.ControlBase.setPosition(self.host, position)
end

function UiWindow:mount(manager)
    assert(not self.nested, "Nested window is already attached to its owning View")
    assert(self._manager == nil or self._manager == manager, "UI window is already mounted to another manager")
    if self._manager == nil then
        manager:loadUI(self.host)
        self._manager = manager
    end
    return self.host
end

function UiWindow:unmount()
    if self._manager ~= nil then
        self._manager:removeUI(self.host)
        self._manager = nil
    end
end

function UiWindow:dispose()
    self:unmount()
    if self.nested then
        local parent = self.host:getParent()
        if parent ~= nil then
            ---@cast parent Engine.Canvas
            parent:removeChild(self.host)
        end
    end
end

return class(UiWindow)
