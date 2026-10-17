local MapAudioState = require("Global.Audio.MapAudioState")

---@class Global.Audio.MapAudio
local MapAudio = {}

function MapAudio:init()
    self._bgmState = MapAudioState.New("BGM")
    self._bgsState = MapAudioState.New("BGS")
end

function MapAudio:playBgm(bgm, bgmFilter)
    MapAudioState.Request(self._bgmState, bgm, bgmFilter, 0.0)
end

function MapAudio:setBgmFilter(attr, value)
    MapAudioState.SetFilterAttribute(self._bgmState.music, attr, value)
end

function MapAudio:setBgsFilter(attr, value)
    MapAudioState.SetFilterAttribute(self._bgsState.music, attr, value)
end

function MapAudio:playMapAudio(mapData, transitionSeconds)
    transitionSeconds = transitionSeconds or 0.0
    local bgm = mapData.bgm or ""
    local bgmFilter = MapAudioState.BuildFilter(mapData.bgmFilter or {})
    local bgs = mapData.bgs or ""
    local bgsFilter = MapAudioState.BuildFilter(mapData.bgsFilter or {})
    MapAudioState.Request(self._bgmState, bgm, bgmFilter, transitionSeconds)
    MapAudioState.Request(self._bgsState, bgs, bgsFilter, transitionSeconds)
end

function MapAudio:onTick(deltaTime)
    MapAudioState.Update(self._bgmState, deltaTime)
    MapAudioState.Update(self._bgsState, deltaTime)
end

function MapAudio:stopMapAudio()
    MapAudioState.Stop(self._bgmState)
    MapAudioState.Stop(self._bgsState)
end

return class(MapAudio)
