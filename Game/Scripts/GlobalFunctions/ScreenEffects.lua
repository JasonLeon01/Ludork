local GlobalCore
local function loadGlobalCore()
    if GlobalCore == nil then
        GlobalCore = require("GlobalCore")
    end
    return GlobalCore
end

local ScreenEffects = {}

function ScreenEffects.FlashScreen(red, green, blue, alpha, duration)
    red = red == nil and 255 or red
    green = green == nil and 255 or green
    blue = blue == nil and 255 or blue
    alpha = alpha == nil and 255 or alpha
    duration = duration == nil and 0.5 or duration
    loadGlobalCore().ScreenEffects.flashScreen(sf.Color.new(red, green, blue, alpha), duration)
end

function ScreenEffects.StopFlashScreen()
    loadGlobalCore().ScreenEffects.stopFlash()
end

function ScreenEffects.ChangeScreenTone(red, green, blue, gray, duration)
    red = red == nil and 0 or red
    green = green == nil and 0 or green
    blue = blue == nil and 0 or blue
    gray = gray == nil and 0 or gray
    duration = duration == nil and 0.0 or duration
    loadGlobalCore().ScreenEffects.changeScreenTone(red, green, blue, gray, duration)
end

function ScreenEffects.ClearScreenTone(duration)
    duration = duration == nil and 0.0 or duration
    loadGlobalCore().ScreenEffects.clearScreenTone(duration)
end

function ScreenEffects.ScreenShake(power, speed, duration)
    power = power == nil and 4.0 or power
    speed = speed == nil and 10.0 or speed
    duration = duration == nil and 0.5 or duration
    loadGlobalCore().ScreenEffects.startShake(power, speed, duration)
end

function ScreenEffects.StopScreenShake()
    loadGlobalCore().ScreenEffects.stopShake()
end

return ScreenEffects
