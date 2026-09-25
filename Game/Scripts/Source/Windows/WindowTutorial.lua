local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local Ui = require("Internal.UIBase.Ui")
local View = require("Internal.UI.WindowTutorial")

---@class Source.Windows.WindowTutorial.Controller
local Controller = {}

Controller.windowOptions = { screen = true, hidden = true, focusable = false }
Controller.SHRINK_DURATION = 0.25

function Controller:init()
    self._capture = nil
    self._rect = nil
    self._elapsed = 0
    self._ready = false
    self._finished = true
end

function Controller:open(rect, text)
    assert(self._finished, "Tutorial window is already showing a tutorial")
    local size = GlobalCore.Display.getGameSize()
    assert(rect.size.x > 0 and rect.size.y > 0, "Tutorial rectangle must have positive dimensions")
    assert(rect.position.x >= 0 and rect.position.y >= 0
            and rect.position.x + rect.size.x <= size.x and rect.position.y + rect.size.y <= size.y,
        "Tutorial rectangle must fit within the logical screen")
    self._rect = rect:copy()
    self._elapsed = 0
    self._ready = false
    self.host:resize(size)
    self:prepare(size)
    self:refreshText(text)
    self.ui.controls["Explanation"]:setVisible(false)
    self.ui.controls["Border"]:setVisible(false)
    self:_layoutOpening(0)
    self._capture = Engine.Input.captureInput()
    self._finished = false
    self.host:setVisible(true)
end

function Controller:refreshText(text)
    if self._rect == nil then
        return
    end
    local size = GlobalCore.Display.getGameSize()
    local wrapped = Engine.TextLayout.wrapPlainText(text, size.x - 2, self.ui.controls["Explanation"])
    self.ui.controls["Explanation"]:setString(wrapped)
    local bounds = self.ui.controls["Explanation"]:getLocalBounds()
    local x = math.max(0, math.min(self._rect.position.x + self._rect.size.x / 2, size.x - bounds.size.x))
    local y = self._rect.position.y + self._rect.size.y + 0.0
    if y + bounds.size.y > size.y then
        y = self._rect.position.y - bounds.size.y
    end
    self.ui.controls["Explanation"]:setOrigin(bounds.position)
    self.ui.controls["Explanation"]:setPosition(sf.Vector2f.new(x, math.max(0, y)))
end

function Controller:_layoutOpening(alpha)
    local rect = assert(self._rect)
    local size = GlobalCore.Display.getGameSize()
    local left = rect.position.x * alpha
    local top = rect.position.y * alpha
    local right = math.lerp(size.x, rect.position.x + rect.size.x, alpha)
    local bottom = math.lerp(size.y, rect.position.y + rect.size.y, alpha)
    self.ui.controls["TopCurtain"]:setSize(sf.Vector2f.new(size.x, top))
    self.ui.controls["BottomCurtain"]:setPosition(sf.Vector2f.new(0, bottom))
    self.ui.controls["BottomCurtain"]:setSize(sf.Vector2f.new(size.x, size.y - bottom))
    self.ui.controls["LeftCurtain"]:setPosition(sf.Vector2f.new(0, top))
    self.ui.controls["LeftCurtain"]:setSize(sf.Vector2f.new(left, bottom - top))
    self.ui.controls["RightCurtain"]:setPosition(sf.Vector2f.new(right, top))
    self.ui.controls["RightCurtain"]:setSize(sf.Vector2f.new(size.x - right, bottom - top))
    self.ui.controls["Border"]:setPosition(sf.Vector2f.new(left, top))
    self.ui.controls["Border"]:setSize(sf.Vector2f.new(right - left, bottom - top))
end

function Controller:onTick(deltaTime)
    if self._finished then
        return
    end
    local capture = assert(self._capture)
    if not self._ready then
        self._elapsed = self._elapsed + deltaTime
        self:_layoutOpening(math.min(self._elapsed / Controller.SHRINK_DURATION, 1))
        if self._elapsed >= Controller.SHRINK_DURATION then
            self._ready = true
            self.ui.controls["Explanation"]:setVisible(true)
            self.ui.controls["Border"]:setVisible(true)
            self:playAnimation("Flash", "Border")
            capture:setConfirmEnabled(true)
        end
        return
    end
    if capture:consumeConfirm() then
        self:close()
    end
end

function Controller:isFinished()
    return self._finished
end

function Controller:close()
    if self._capture ~= nil then
        self._capture:release()
        self._capture = nil
    end
    self:stopAnimation("Flash", "Border")
    self._finished = true
    self._ready = false
    self.host:setVisible(false)
end

function Controller:dispose()
    self:close()
    super(Controller, self).dispose()
end

return Ui.DefineWindow(View, Controller)
