local Engine = require("Engine")
local GlobalCore = require("GlobalCore")

local AudioManager = GlobalCore.AudioManager
local MUSIC_FILTER_FIELDS = { "loop", "pitch", "pan", "volume" }

local MapAudioState = {}

local function requestedVolume(filter)
    return filter ~= nil and filter.volume or 100.0
end

local function startMusic(state, file, filter, fadeIn)
    if not bool(file) then
        return
    end
    local music = AudioManager.playMusic(state.kind, file, filter)
    if music == nil then
        return
    end
    music:setLooping(true)
    state.music = music
    state.file = file
    local targetVolume = music:getVolume()
    state.targetVolume = targetVolume
    state.requestedVolume = requestedVolume(filter)
    if fadeIn > 0.0 then
        music:setVolume(0.0)
        state.transition = {
            phase = "in",
            elapsed = 0.0,
            duration = fadeIn,
            startVolume = 0.0,
            targetVolume = targetVolume
        }
    end
end

local function replaceMusicFilter(music, filter)
    local loopPoint = sf.Music.TimeSpan.new()
    loopPoint.offset = sf.seconds(0.0)
    loopPoint.length = music:getDuration()
    local values = {
        pitch = filter ~= nil and filter.pitch or 1.0,
        pan = filter ~= nil and filter.pan or 0.0,
        volume = filter ~= nil and filter.volume or 100.0,
        loopPoint = filter ~= nil and filter.loopPoint or loopPoint
    }
    if filter ~= nil and filter.loop ~= nil then
        values.loop = filter.loop
    end
    if filter ~= nil and filter.offset ~= nil then
        values.offset = filter.offset
    end
    AudioManager.setMusicFilter(music, Engine.MusicFilter.new(values))
end

local function buildLoopPoint(data)
    local start = data.start
    local finish = data["end"]
    local span = sf.Music.TimeSpan.new()
    span.offset = sf.seconds(start)
    if finish > start then
        span.length = sf.seconds(finish - start)
    end
    return span
end

function MapAudioState.New(kind)
    return { kind = kind, music = nil, file = "", pendingFile = "", pendingFilter = nil }
end

function MapAudioState.Stop(state)
    if state.music ~= nil then
        AudioManager.stopMusic(state.kind)
    end
    state.music = nil
    state.file = ""
    state.transition = nil
    state.targetVolume = nil
    state.requestedVolume = nil
    state.pendingFile = ""
    state.pendingFilter = nil
end

function MapAudioState.Request(state, file, filter, duration)
    if state.music ~= nil and state.file == file and bool(file) then
        local volume = requestedVolume(filter)
        local targetVolume = state.targetVolume or state.music:getVolume()
        replaceMusicFilter(state.music, filter)
        if state.requestedVolume ~= volume then
            targetVolume = state.music:getVolume()
        end
        state.targetVolume = targetVolume
        state.requestedVolume = volume
        state.music:setLooping(true)
        state.music:setVolume(targetVolume)
        state.transition = nil
        state.pendingFile = ""
        state.pendingFilter = nil
        return
    end
    if duration <= 0.0 then
        MapAudioState.Stop(state)
        startMusic(state, file, filter, 0.0)
        return
    end
    state.pendingFile = file
    state.pendingFilter = filter
    if state.music == nil then
        local pendingFile = state.pendingFile
        local pendingFilter = state.pendingFilter
        state.pendingFile = ""
        state.pendingFilter = nil
        startMusic(state, pendingFile, pendingFilter, duration)
        return
    end
    state.transition = {
        phase = "out",
        elapsed = 0.0,
        duration = bool(file) and duration * 0.5 or duration,
        startVolume = state.music:getVolume(),
        targetVolume = 0.0,
        switchDuration = bool(file) and duration * 0.5 or 0.0
    }
end

function MapAudioState.Update(state, deltaTime)
    local transition = state.transition
    if state.music == nil then
        return
    end
    if transition == nil then
        state.targetVolume = state.music:getVolume()
        return
    end
    transition.elapsed = Engine.Clamp(transition.elapsed + deltaTime, 0.0, transition.duration)
    local alpha = transition.duration > 0.0 and transition.elapsed / transition.duration or 1.0
    state.music:setVolume(Engine.Lerp(transition.startVolume, transition.targetVolume, alpha))
    if alpha < 1.0 then
        return
    end
    if transition.phase == "in" then
        state.transition = nil
        return
    end
    ---@cast transition Source.SceneComponents.SceneMapMusicFadeOutTransition
    local pendingFile = state.pendingFile
    local pendingFilter = state.pendingFilter
    local switchDuration = transition.switchDuration
    MapAudioState.Stop(state)
    state.pendingFile = ""
    state.pendingFilter = nil
    startMusic(state, pendingFile, pendingFilter, switchDuration)
end

function MapAudioState.SetFilterAttribute(music, attr, value)
    if music == nil then
        return
    end
    AudioManager.setMusicFilter(music, Engine.MusicFilter.new({ [attr] = value }))
end

function MapAudioState.BuildFilter(data)
    if not bool(data) then
        return nil
    end
    ---@type Source.SceneComponents.MusicFilterValues
    local values = {}
    for _, key in ipairs(MUSIC_FILTER_FIELDS) do
        if data[key] ~= nil then
            values[key] = data[key]
        end
    end
    if data.offset ~= nil then
        values.offset = sf.seconds(data.offset)
    end
    if data.loopPoint ~= nil then
        values.loopPoint = buildLoopPoint(data.loopPoint)
        if values.offset == nil then
            local start = data.loopPoint.start
            if start > 0.0 then
                values.offset = sf.seconds(start)
            end
        end
    end
    if not bool(values) then
        return nil
    end
    return Engine.MusicFilter.new(values)
end

function MapAudioState.SyncController(controller)
    controller._currentBgmMusic = controller._bgmState.music
    controller._currentBgmFile = controller._bgmState.file
    controller._currentBgsMusic = controller._bgsState.music
    controller._currentBgsFile = controller._bgsState.file
end

return MapAudioState
