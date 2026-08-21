local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")

local ManagerFunctions = GlobalFunctions.Manager
local AudioManager = GlobalCore.AudioManager

---@class Source.SceneComponents.SceneMapAudioController
local SceneMapAudioController = {}

function SceneMapAudioController:init()
    self._currentBgmMusic = nil
    self._currentBgmFile = ""
    self._currentBgsMusic = nil
    self._currentBgsFile = ""
end

function SceneMapAudioController:playBgm(bgm, bgmFilter)
    if self._currentBgmMusic ~= nil then
        ManagerFunctions.stopMusic("BGM")
        self._currentBgmMusic = nil
    end
    self._currentBgmFile = ""
    if not bool(bgm) then
        return
    end
    self._currentBgmMusic = ManagerFunctions.playMusic("BGM", bgm, bgmFilter)
    if self._currentBgmMusic ~= nil then
        self._currentBgmMusic:setLooping(true)
        self._currentBgmFile = bgm
    end
end

function SceneMapAudioController:setBgmFilter(attr, value)
    if self._currentBgmMusic == nil then
        return
    end
    local filterObj = Engine.MusicFilter.new({
        [attr] = value
    })
    AudioManager.setMusicFilter(self._currentBgmMusic, filterObj)
end

function SceneMapAudioController:setBgsFilter(attr, value)
    if self._currentBgsMusic == nil then
        return
    end
    local filterObj = Engine.MusicFilter.new({
        [attr] = value
    })
    AudioManager.setMusicFilter(self._currentBgsMusic, filterObj)
end

function SceneMapAudioController:playMapAudio(mapData)
    local bgm = mapData.bgm or ""
    local hasBgm = bool(bgm)
    local bgmFilter = SceneMapAudioController._buildMusicFilter(mapData.bgmFilter or {})
    local reuseBgm = hasBgm and self._currentBgmMusic ~= nil and self._currentBgmFile == bgm
    if reuseBgm then
        ---@cast self._currentBgmMusic sf.Music
        if bgmFilter ~= nil then
            AudioManager.setMusicFilter(self._currentBgmMusic, bgmFilter)
        end
        self._currentBgmMusic:setLooping(true)
    else
        if self._currentBgmMusic ~= nil then
            ManagerFunctions.stopMusic("BGM")
            self._currentBgmMusic = nil
        end
        self._currentBgmFile = ""
    end
    if hasBgm and not reuseBgm then
        self._currentBgmMusic = ManagerFunctions.playMusic("BGM", bgm, bgmFilter)
        if self._currentBgmMusic ~= nil then
            self._currentBgmMusic:setLooping(true)
            self._currentBgmFile = bgm
        end
    end

    local bgs = mapData.bgs or ""
    local hasBgs = bool(bgs)
    local bgsFilter = SceneMapAudioController._buildMusicFilter(mapData.bgsFilter or {})
    local reuseBgs = hasBgs and self._currentBgsMusic ~= nil and self._currentBgsFile == bgs
    if reuseBgs then
        ---@cast self._currentBgsMusic sf.Music
        if bgsFilter ~= nil then
            AudioManager.setMusicFilter(self._currentBgsMusic, bgsFilter)
        end
        self._currentBgsMusic:setLooping(true)
    else
        if self._currentBgsMusic ~= nil then
            ManagerFunctions.stopMusic("BGS")
            self._currentBgsMusic = nil
        end
        self._currentBgsFile = ""
    end
    if hasBgs and not reuseBgs then
        self._currentBgsMusic = ManagerFunctions.playMusic("BGS", bgs, bgsFilter)
        if self._currentBgsMusic ~= nil then
            self._currentBgsMusic:setLooping(true)
            self._currentBgsFile = bgs
        end
    end
end

function SceneMapAudioController:stopMapAudio()
    if self._currentBgmMusic ~= nil then
        ManagerFunctions.stopMusic("BGM")
        self._currentBgmMusic = nil
    end
    self._currentBgmFile = ""
    if self._currentBgsMusic ~= nil then
        ManagerFunctions.stopMusic("BGS")
        self._currentBgsMusic = nil
    end
    self._currentBgsFile = ""
end

---@param data Source.SceneComponents.MusicLoopPointData
---@return sf.Music.TimeSpan
function SceneMapAudioController._buildLoopPoint(data)
    local start = data.start
    local finish = data["end"]
    local span = sf.Music.TimeSpan.new()
    span.offset = sf.seconds(start)
    if finish > start then
        span.length = sf.seconds(finish - start)
    end
    return span
end

---@param data Source.SceneComponents.MusicFilterData
---@return Engine.MusicFilter | nil
function SceneMapAudioController._buildMusicFilter(data)
    if not bool(data) then
        return nil
    end
    ---@type Source.SceneComponents.MusicFilterValues
    local values = {}
    local fields = { "loop", "pitch", "pan", "volume" }
    for _, key in ipairs(fields) do
        if data[key] ~= nil then
            values[key] = data[key]
        end
    end
    if data.offset ~= nil then
        values.offset = sf.seconds(data.offset)
    end
    if data.loopPoint ~= nil then
        values.loopPoint = SceneMapAudioController._buildLoopPoint(data.loopPoint)
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

return class(SceneMapAudioController)
