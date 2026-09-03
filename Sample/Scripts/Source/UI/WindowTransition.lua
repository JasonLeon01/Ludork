local WindowTransition = {}

WindowTransition.DEFAULT = "Default"
WindowTransition.MENU = "Menu"

function WindowTransition.GetAnimationNames(profile)
    if profile == WindowTransition.MENU then
        return "FadeIn_Menu", "FadeOut_Menu"
    end
    return "FadeIn", "FadeOut"
end

function WindowTransition:init(host, ui, target)
    self._host = host
    self._ui = ui
    self._target = target
    self._phase = "open"
    self._generation = 0
    self._animationName = nil
end

function WindowTransition:_isCurrent(requestGeneration)
    return requestGeneration == self._generation
end

function WindowTransition:show(animationName, onReady)
    self._generation = self._generation + 1
    local requestGeneration = self._generation
    self._phase = "entering"
    self._animationName = animationName
    self._host:setVisible(true)
    self._host:setActive(false)
    self._ui:playAnimation(animationName, self._target, function ()
        if not self:_isCurrent(requestGeneration) then
            return
        end
        self._ui:stopAnimation(animationName, self._target)
        self._animationName = nil
        self._phase = "open"
        if onReady ~= nil then
            onReady()
        end
    end)
end

function WindowTransition:hide(animationName, onHidden)
    if not self._host:getVisible() then
        if onHidden ~= nil then
            onHidden()
        end
        return
    end
    self._generation = self._generation + 1
    local requestGeneration = self._generation
    self._phase = "exiting"
    self._animationName = animationName
    self._host:setActive(false)
    self._ui:playAnimation(animationName, self._target, function ()
        if not self:_isCurrent(requestGeneration) then
            return
        end
        self._ui:stopAnimation(animationName, self._target)
        self._animationName = nil
        self._host:setVisible(false)
        self._phase = "hidden"
        if onHidden ~= nil then
            onHidden()
        end
    end)
end

function WindowTransition:hideImmediate()
    self._generation = self._generation + 1
    if self._animationName ~= nil then
        self._ui:stopAnimation(self._animationName, self._target)
    end
    self._animationName = nil
    self._host:setActive(false)
    self._host:setVisible(false)
    self._phase = "hidden"
end

function WindowTransition:isBlocking()
    return self._phase ~= "hidden"
end

function WindowTransition:isOpen()
    return self._phase == "open"
end

return class(WindowTransition)
