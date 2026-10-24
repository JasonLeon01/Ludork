local Engine = require("Engine")
local GlobalCore = require("GlobalCore")

local DEFAULT_MUSIC_VOLUME = 100.0

---@enum Global.Audio.MapAudioState.TransitionPhase
local TransitionPhase = {
    IN = "in",
    OUT = "out"
}

local AudioManager = GlobalCore.AudioManager
local MUSIC_FILTER_FIELDS = { "loop", "pitch", "pan", "volume" }

local MapAudioState = {}

local function requestedVolume(filter)
    return filter ~= nil and filter.volume or DEFAULT_MUSIC_VOLUME
end

local function applyVolume(state)
    AudioManager.setMusicFilter(state.music, Engine.MusicFilter.new({ volume = state.requestedVolume * state.gain }))
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
    state.requestedVolume = requestedVolume(filter)
    state.gain = fadeIn > 0.0 and 0.0 or 1.0
    if fadeIn > 0.0 then
        applyVolume(state)
        state.transition = {
            phase = TransitionPhase.IN,
            elapsed = 0.0,
            duration = fadeIn,
            startGain = 0.0,
            targetGain = 1.0
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
        volume = filter ~= nil and filter.volume or DEFAULT_MUSIC_VOLUME,
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
    return {
        kind = kind,
        music = nil,
        file = "",
        requestedVolume = DEFAULT_MUSIC_VOLUME,
        gain = 1.0,
        pendingFile = "",
        pendingFilter = nil
    }
end

function MapAudioState.Stop(state)
    if state.music ~= nil then
        AudioManager.stopMusic(state.kind)
    end
    state.music = nil
    state.file = ""
    state.transition = nil
    state.requestedVolume = DEFAULT_MUSIC_VOLUME
    state.gain = 1.0
    state.pendingFile = ""
    state.pendingFilter = nil
end

function MapAudioState.Request(state, file, filter, duration)
    if state.music ~= nil and state.file == file and bool(file) then
        state.requestedVolume = requestedVolume(filter)
        state.gain = 1.0
        replaceMusicFilter(state.music, filter)
        state.music:setLooping(true)
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
        phase = TransitionPhase.OUT,
        elapsed = 0.0,
        duration = bool(file) and duration * 0.5 or duration,
        startGain = state.gain,
        targetGain = 0.0,
        switchDuration = bool(file) and duration * 0.5 or 0.0
    }
end

function MapAudioState.Update(state, deltaTime)
    local transition = state.transition
    if state.music == nil then
        return
    end
    if transition == nil then
        return
    end
    transition.elapsed = math.clamp(transition.elapsed + deltaTime, 0.0, transition.duration)
    local alpha = transition.duration > 0.0 and transition.elapsed / transition.duration or 1.0
    state.gain = math.lerp(transition.startGain, transition.targetGain, alpha)
    applyVolume(state)
    if alpha < 1.0 then
        return
    end
    if transition.phase == TransitionPhase.IN then
        state.transition = nil
        return
    end
    ---@cast transition Global.Audio.MapAudioState.FadeOutTransition
    local pendingFile = state.pendingFile
    local pendingFilter = state.pendingFilter
    local switchDuration = transition.switchDuration
    MapAudioState.Stop(state)
    state.pendingFile = ""
    state.pendingFilter = nil
    startMusic(state, pendingFile, pendingFilter, switchDuration)
end

function MapAudioState.SetFilterAttribute(state, attr, value)
    if state.music == nil then
        return
    end
    local filter = Engine.MusicFilter.new({ [attr] = value })
    if filter.volume ~= nil then
        state.requestedVolume = filter.volume
        filter.volume = state.requestedVolume * state.gain
    end
    AudioManager.setMusicFilter(state.music, filter)
end

function MapAudioState.BuildFilter(data)
    if not bool(data) then
        return nil
    end
    ---@type Global.Audio.MapAudioState.MusicFilterValues
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

return MapAudioState
