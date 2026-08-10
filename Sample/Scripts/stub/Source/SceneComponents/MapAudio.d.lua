---@meta Source.SceneComponents.MapAudio
---@alias Source.SceneComponents.MusicFilterValue boolean | number | sf.Time | sf.Music.TimeSpan

---@class Source.SceneComponents.MusicLoopPointData
---@field start number
---@field end number

---@class Source.SceneComponents.MusicFilterData
---@field loop? boolean
---@field offset? number
---@field pitch? number
---@field pan? number
---@field volume? number
---@field loopPoint? Source.SceneComponents.MusicLoopPointData

--- @brief Manage map BGM/BGS playback and music filters.
---@class Source.SceneComponents.SceneMapAudioController
---@field _currentBgmMusic sf.Music | nil
---@field _currentBgmFile string
---@field _currentBgsMusic sf.Music | nil
---@field _currentBgsFile string
local SceneMapAudioController = {}

---@return Source.SceneComponents.SceneMapAudioController
function SceneMapAudioController.new(...) end

function SceneMapAudioController:init() end

--- @brief Replace the current BGM with a new track.
---
--- - @param bgm Music filename under Assets/Musics.
--- - @param bgmFilter Optional music filter to apply.
---@param bgm       string
---@param bgmFilter Engine.MusicFilter | nil
function SceneMapAudioController:playBgm(bgm, bgmFilter) end

--- @brief Set a filter attribute on the current BGM music.
---
--- - @param attr The filter attribute name.
--- - @param value The filter attribute value.
---@param attr  string
---@param value Source.SceneComponents.MusicFilterValue
function SceneMapAudioController:setBgmFilter(attr, value) end

--- @brief Set a filter attribute on the current BGS music.
---
--- - @param attr The filter attribute name.
--- - @param value The filter attribute value.
---@param attr  string
---@param value Source.SceneComponents.MusicFilterValue
function SceneMapAudioController:setBgsFilter(attr, value) end

--- @brief Play or reuse BGM/BGS described by map data.
---
--- - @param mapData Current map data.
---@param mapData Source.SceneComponents.MapData
function SceneMapAudioController:playMapAudio(mapData) end

--- @brief Stop current BGM/BGS playback.
function SceneMapAudioController:stopMapAudio() end

return SceneMapAudioController
