local MapAudioState = require("Source.SceneComponents.MapAudioState")

---@class Source.SceneComponents.SceneMapAudioController
local SceneMapAudioController = {}

function SceneMapAudioController:init()
    self._currentBgmMusic = nil
    self._currentBgmFile = ""
    self._currentBgsMusic = nil
    self._currentBgsFile = ""
    self._bgmState = MapAudioState.New("BGM")
    self._bgsState = MapAudioState.New("BGS")
end

function SceneMapAudioController:playBgm(bgm, bgmFilter)
    MapAudioState.Request(self._bgmState, bgm, bgmFilter, 0.0)
    MapAudioState.SyncController(self)
end

function SceneMapAudioController:setBgmFilter(attr, value)
    MapAudioState.SetFilterAttribute(self._currentBgmMusic, attr, value)
end

function SceneMapAudioController:setBgsFilter(attr, value)
    MapAudioState.SetFilterAttribute(self._currentBgsMusic, attr, value)
end

function SceneMapAudioController:playMapAudio(mapData, transitionSeconds)
    transitionSeconds = transitionSeconds or 0.0
    local bgm = mapData.bgm or ""
    local bgmFilter = MapAudioState.BuildFilter(mapData.bgmFilter or {})
    local bgs = mapData.bgs or ""
    local bgsFilter = MapAudioState.BuildFilter(mapData.bgsFilter or {})
    MapAudioState.Request(self._bgmState, bgm, bgmFilter, transitionSeconds)
    MapAudioState.Request(self._bgsState, bgs, bgsFilter, transitionSeconds)
    MapAudioState.SyncController(self)
end

function SceneMapAudioController:onTick(deltaTime)
    MapAudioState.Update(self._bgmState, deltaTime)
    MapAudioState.Update(self._bgsState, deltaTime)
    MapAudioState.SyncController(self)
end

function SceneMapAudioController:stopMapAudio()
    MapAudioState.Stop(self._bgmState)
    MapAudioState.Stop(self._bgsState)
    MapAudioState.SyncController(self)
end

return class(SceneMapAudioController)
