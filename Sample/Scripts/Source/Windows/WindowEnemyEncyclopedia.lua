local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GameSystem = require("Source.System")
local WindowEnemyEncyclopediaUI = require("Source.UI.WindowEnemyEncyclopedia")
local WindowBase = require("Source.Windows.Base.WindowBase")

local Input = Engine.Input
local AudioManager = GlobalCore.AudioManager

local _WINDOW_WIDTH = 640
local _WINDOW_HEIGHT = 480

local WindowEnemyEncyclopedia = {}

WindowEnemyEncyclopedia.uiClass = WindowEnemyEncyclopediaUI

function WindowEnemyEncyclopedia:init(rect, onClose)
    super(WindowEnemyEncyclopedia, self).init(rect, nil, nil, true)
    self:setHasReturnBtn(true)
    self._onCloseCallback = onClose
    self._portrait = nil
    self._nameText = nil
    self._infoTexts = {}
    ---@cast self Source.Windows.WindowEnemyEncyclopedia
    self._ui = self.uiClass.new(self, rect.size)
    self._ui:attach()
    self:setCanReceiveFocus(true)
    self:hideImmediate()
end

function WindowEnemyEncyclopedia:open(entry)
    self._ui:open(entry)
    self:showWithAnimation("FadeIn", function ()
        self:setActive(true)
        self:requestKeyboardFocus()
    end)
end

function WindowEnemyEncyclopedia:close()
    self:setActive(false)
    self:hideWithAnimation("FadeOut", function ()
        if self._onCloseCallback ~= nil then
            self._onCloseCallback()
        end
    end)
end

function WindowEnemyEncyclopedia:refreshLocale()
    self._ui:refreshLocale()
end

function WindowEnemyEncyclopedia:onKeyDown(_kwargs)
    if Input.isActionTriggered(Input.getConfirmKeys(), false) then
        self:onReturn()
        Input.isActionTriggered(Input.getConfirmKeys(), true)
        return
    end
    if Input.isActionTriggered(Input.getCancelKeys(), false) then
        self:onReturn()
        Input.isActionTriggered(Input.getCancelKeys(), true)
    end
end

function WindowEnemyEncyclopedia:onMouseButtonDown(_kwargs)
    if _kwargs.button == sf.Mouse.Button.Right then
        self:onReturn()
        return true
    end
    return false
end

function WindowEnemyEncyclopedia:_clearEnemyControls()
    self._ui:clearEnemyControls()
end

function WindowEnemyEncyclopedia:onReturn()
    AudioManager.playSound(GameSystem.GetCancelSE())
    self:close()
end

return class(WindowEnemyEncyclopedia, WindowBase)
