local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local LOC = require("Source.Locale.Core").ApplyStringLocaleFormat
local Context = require("Source.NodeFunctions.Context")

local ManagerFunctions = GlobalFunctions.Manager
local GlobalSystem = GlobalCore.System

local System = {}

System.SoundFilter = Engine.SoundFilter.new()
System.MusicFilter = Engine.MusicFilter.new()

local TransitionCondition = {}

function TransitionCondition:init()
    ---@type boolean
    self._started = false
end

function TransitionCondition:poll()
    if GlobalSystem.isTransitionPending() or GlobalSystem.isInTransition() then
        self._started = true
        return false
    end
    return self._started
end

function TransitionCondition:isFinished()
    return self._started and not GlobalSystem.isTransitionPending() and not GlobalSystem.isInTransition()
end

local FinalTransitionCondition = class(TransitionCondition)
FinalTransitionCondition.__call = TransitionCondition.poll

local FrozenCondition = {}

function FrozenCondition.poll()
    return not GlobalSystem.isTransitionBackgroundFreezePending() and GlobalSystem.isTransitionBackgroundFrozen()
end

function FrozenCondition:isFinished()
    return self:poll()
end

local FinalFrozenCondition = class(FrozenCondition)
FinalFrozenCondition.__call = FrozenCondition.poll

function System.EditSoundFilter(attr, value)
    System.SoundFilter[attr] = value
end

function System.EditMusicFilter(attr, value)
    System.MusicFilter[attr] = value
end

function System.PlaySound(soundFileName, applyFilter)
    applyFilter = applyFilter == nil and false or applyFilter
    if applyFilter then
        ManagerFunctions.playSE(soundFileName, System.SoundFilter)
    else
        ManagerFunctions.playSE(soundFileName)
    end
end

function System.PlayMusic(musicFileName, applyFilter)
    applyFilter = applyFilter == nil and false or applyFilter
    local scene = GlobalSystem.getScene()
    local musicFilter = applyFilter and System.MusicFilter or nil
    if bool(scene) then
        ---@cast scene Source.Scenes.SceneMap.SceneMap
        scene:playBgm(musicFileName, musicFilter)
        return
    end
    local music = ManagerFunctions.playMusic("BGM", musicFileName, musicFilter)
    if music ~= nil then
        music:setLooping(true)
    end
end

function System.PlayVideo(videoFileName, mute, skipable)
    mute = mute == nil and false or mute
    skipable = skipable == nil and true or skipable
    local videoPath = os.path.join(os.getcwd(), "Assets", "Videos", videoFileName)
    _G.playVideo(videoPath, mute, skipable)
end

function System.FreezeTransitionBackground()
    GlobalSystem.freezeTransitionBackground()
    return FinalFrozenCondition.new()
end

function System.RequestTransition(transitionName, transitionTime)
    transitionName = transitionName == nil and "" or transitionName
    transitionTime = transitionTime == nil and 1.0 or transitionTime
    GlobalSystem.requestTransition(transitionName, tonumber(transitionTime))
    return FinalTransitionCondition.new()
end

function System.SetBgmFilter(attr, value)
    Context.requireSceneMap():setBgmFilter(attr, value)
end

function System.SetBgsFilter(attr, value)
    Context.requireSceneMap():setBgsFilter(attr, value)
end

function System.FlashScreen(red, green, blue, alpha, duration)
    red = red == nil and 255 or red
    green = green == nil and 255 or green
    blue = blue == nil and 255 or blue
    alpha = alpha == nil and 255 or alpha
    duration = duration == nil and 0.5 or duration
    GlobalSystem.flashScreen(
        sf.Color.new((math.modf(red)), (math.modf(green)), (math.modf(blue)), (math.modf(alpha))), tonumber(duration)
    )
end

function System.StopFlashScreen()
    GlobalSystem.stopFlash()
end

function System.ChangeScreenTone(red, green, blue, gray, duration)
    red = red == nil and 0 or red
    green = green == nil and 0 or green
    blue = blue == nil and 0 or blue
    gray = gray == nil and 0 or gray
    duration = duration == nil and 0.0 or duration
    GlobalSystem.changeScreenTone(tonumber(red), tonumber(green), tonumber(blue), tonumber(gray), tonumber(duration))
end

function System.ClearScreenTone(duration)
    duration = duration == nil and 0.0 or duration
    GlobalSystem.clearScreenTone(tonumber(duration))
end

function System.ScreenShake(power, speed, duration)
    power = power == nil and 4.0 or power
    speed = speed == nil and 10.0 or speed
    duration = duration == nil and 0.5 or duration
    GlobalSystem.startShake(tonumber(power), tonumber(speed), tonumber(duration))
end

function System.StopScreenShake()
    GlobalSystem.stopShake()
end

local function coerceWeatherType(weatherType)
    local names = { "NONE", "RAIN", "STORM", "SNOW" }
    for _, name in ipairs(names) do
        if weatherType == LOC("WEATHER_TYPE_" .. name) then
            return GlobalCore[name]
        end
    end
    return GlobalCore.coerce(weatherType)
end

function System.SetWeather(weatherType, power, maxCount)
    power = power == nil and 40 or power
    maxCount = maxCount == nil and 80 or maxCount
    GlobalSystem.setWeather(coerceWeatherType(weatherType), (math.modf(power)), (math.modf(maxCount)))
end

function System.ClearWeather()
    GlobalSystem.clearWeather()
end

return System
