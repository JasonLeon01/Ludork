local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local WindowBase = require("Source.Windows.Base.WindowBase")
local LocaleCore = require("Source.Locale.Core")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.WindowPlayerName")
local GameSystem = require("Source.System")
local Player = require("Source.Player")

---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat
local Input = Engine.Input
local AudioManager = GlobalCore.AudioManager

---@class Source.Windows.WindowPlayerName.Controller
local Controller = {}

Controller.windowOptions = { centered = true, hidden = true, returnButton = true, focusable = true }

function Controller:init(player, onClose)
    self._player = player
    self._onClose = onClose
    self._errorKey = ""
    self:watch(self, "_errorKey", Controller.refreshError)
end

function Controller:getPlayer()
    return self._player
end

function Controller:setPlayer(player)
    self._player = player
end

function Controller:open()
    self:setDraft(self:getPlayer():getDisplayName())
    self:validateDraft(self:getPlayer():getDisplayName())
    self.host:showWithAnimation("FadeIn", function ()
        self.host:setActive(true)
        self.host:requestKeyboardFocus()
    end)
end

function Controller:close()
    if not self.host:getActive() then
        return
    end
    self:cancelEditing()
    self.host:setActive(false)
    self.host:hideWithAnimation("FadeOut", function ()
        self._onClose()
    end)
end

function Controller:onReturn()
    self:cancel()
end

function Controller:onKeyDown(_kwargs)
    self:handleKeyDown(self.host)
end

function Controller:onMouseButtonDown(kwargs)
    if kwargs.button == sf.Mouse.Button.Right then
        self:onReturn()
        return true
    end
    return false
end

function Controller:dispose()
    self.host:hideImmediate()
    self.ui.controls["NameInput"]:setOnTextChanged(nil)
    self.ui.controls["NameInput"]:setOnEditingChanged(nil)
    self.ui.controls["NameInput"]:cancelEdit()
    super(Controller, self).dispose()
end

function Controller:bind()
    local confirm = self:bindCallback(Controller.confirm)
    local cancel = self:bindCallback(Controller.cancel)
    local handleKeyDown = self:bindCallback(Controller.handleKeyDown)
    self.ui.controls["ConfirmButton"]:addClickCallback(confirm)
    self.ui.controls["ConfirmButton"]:addConfirmCallback(confirm)
    self.ui.controls["ConfirmButton"]:addCancelCallback(cancel)
    self.ui.controls["NameInput"]:addCancelCallback(cancel)
    self.ui.controls["NameInput"]:setOnTextChanged(self:bindCallback(Controller.validateDraft))
    self.ui.controls["NameInput"]:addKeyDownCallback(handleKeyDown)
    self.ui.controls["ConfirmButton"]:addKeyDownCallback(handleKeyDown)
    self.ui.controls["NameInput"]:setCanReceiveFocus(true)
    self.ui.controls["ConfirmButton"]:setCanReceiveFocus(true)
end

function Controller:refresh()
    self:refreshLocale()
end

function Controller:getFocusControls()
    return { self.host, self.ui.controls["NameInput"], self.ui.controls["ConfirmButton"] }
end

function Controller:refreshLocale()
    self:setText("Prompt", LOC("PLAYER_NAME_PROMPT"))
    self:setText("ConfirmLabel", LOC("PLAYER_NAME_CONFIRM"))
    self:refreshError(self._errorKey)
    self.ui.controls["NameInput"]:setInputDialogLabels(
        LOC("PLAYER_NAME_PROMPT"), LOC("PLAYER_NAME_DONE"), LOC("PLAYER_NAME_CANCEL")
    )
end

function Controller:setError(key)
    self._errorKey = key
end

function Controller:refreshError(key)
    key = key or ""
    self:setText("Error", key == "" and "" or LOC(key))
end

function Controller:setConfirmEnabled(enabled)
    self.ui.controls["ConfirmButton"]:setActive(enabled)
    self.ui.controls["ConfirmButton"]:setColour(
        enabled and sf.Color.new(255, 255, 255, 255) or sf.Color.new(110, 110, 110, 170)
    )
    self.ui.controls["ConfirmLabel"]:setColour(
        enabled and sf.Color.new(255, 255, 255, 255) or sf.Color.new(150, 150, 150, 255)
    )
end

function Controller:moveFocus(delta)
    local controls = { self.ui.controls["NameInput"], self.ui.controls["ConfirmButton"] }
    local index = delta > 0 and 0 or 1
    for current, control in ipairs(controls) do
        if control:getFocused() then
            index = current
            break
        end
    end
    for _ = 1, #controls do
        index = (index - 1 + delta) % #controls + 1
        local target = controls[index]
        if target ~= nil and target:canReceiveFocus() then
            target:requestKeyboardFocus()
            return
        end
    end
end

function Controller:handleKeyDown(control)
    if Input.getKeyPressed(sf.Keyboard.Key.Tab, true, false, false, true, false) then
        self:moveFocus(-1)
    elseif Input.getKeyPressed(sf.Keyboard.Key.Tab, true) then
        self:moveFocus(1)
    elseif Input.isActionTriggered(Input.getDownKeys(), true) or Input.isActionTriggered(Input.getRightKeys(), true) then
        self:moveFocus(1)
    elseif Input.isActionTriggered(Input.getUpKeys(), true) or Input.isActionTriggered(Input.getLeftKeys(), true) then
        self:moveFocus(-1)
    elseif Input.isActionTriggered(Input.getCancelKeys(), true) then
        self:cancel()
    elseif Input.isAnyJoystickButtonTriggered(Engine.JoystickButton.getA(), true) then
        self:beginEditing()
    elseif Input.isActionTriggered(Input.getConfirmKeys(), true) then
        if control == self.ui.controls["ConfirmButton"] then
            self:confirm()
        else
            self:beginEditing()
        end
    end
end

function Controller:setDraft(text)
    self.ui.controls["NameInput"]:cancelEdit()
    self.ui.controls["NameInput"]:setString(text)
end

function Controller:beginEditing()
    self.ui.controls["NameInput"]:beginEdit()
end

function Controller:finishEditing()
    self.ui.controls["NameInput"]:finishEdit()
    return self.ui.controls["NameInput"]:getString()
end

function Controller:cancelEditing()
    self.ui.controls["NameInput"]:cancelEdit()
end

function Controller:validateDraft(text)
    local _, reason = Player.ValidateName(text)
    local errorKey = reason == "empty" and "PLAYER_NAME_EMPTY" or (reason == "tooLong" and "PLAYER_NAME_TOO_LONG" or "")
    self:setError(errorKey)
    self:setConfirmEnabled(reason == nil)
    return reason == nil
end

function Controller:confirm()
    if not self.host:getActive() then
        return
    end
    local draft = self:finishEditing()
    if not self:validateDraft(draft) then
        return
    end
    assert(self:getPlayer():setName(draft), "Validated player name was rejected")
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self:close()
end

function Controller:cancel()
    if not self.host:getActive() then
        return
    end
    AudioManager.playSound(GameSystem.GetCancelSE())
    self:close()
end

return Ui.DefineWindow(View, Controller, WindowBase)
