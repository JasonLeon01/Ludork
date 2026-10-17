---@meta Global.Audio.MapAudio
---@alias Global.Audio.SoundFilterValue boolean | number | sf.Time | sf.Vector3f | sf.SoundSource.Cone | nil
---@alias Global.Audio.MusicFilterValue Global.Audio.SoundFilterValue | sf.Music.TimeSpan

---@class Global.Audio.MusicLoopPointData
---@field start number
---@field end   number

---@class Global.Audio.MusicFilterData
---@field loop?      boolean
---@field offset?    number
---@field pitch?     number
---@field pan?       number
---@field volume?    number
---@field loopPoint? Global.Audio.MusicLoopPointData

---@class Global.Audio.MapAudioData
---@field bgm       string | nil
---@field bgs       string | nil
---@field bgmFilter Global.Audio.MusicFilterData | nil
---@field bgsFilter Global.Audio.MusicFilterData | nil

---@brief Manage map BGM/BGS playback and music filters.
---@class Global.Audio.MapAudio
---@field _bgmState Global.Audio.MapAudioState.State
---@field _bgsState Global.Audio.MapAudioState.State
local MapAudio = {}

---@return Global.Audio.MapAudio
function MapAudio.new(...) end

function MapAudio:init() end

---@brief Replace the current BGM with a new track.
---
--- - @param bgm Canonical music asset path under /Game/Assets/Musics.
--- - @param bgmFilter Optional music filter to apply.
---@param bgm       string
---@param bgmFilter Engine.MusicFilter | nil
function MapAudio:playBgm(bgm, bgmFilter) end

---@brief Set a filter attribute on the current BGM music.
---
--- - @param attr The filter attribute name.
--- - @param value The filter attribute value.
---@param attr  string
---@param value Global.Audio.MusicFilterValue
function MapAudio:setBgmFilter(attr, value) end

---@brief Set a filter attribute on the current BGS music.
---
--- - @param attr The filter attribute name.
--- - @param value The filter attribute value.
---@param attr  string
---@param value Global.Audio.MusicFilterValue
function MapAudio:setBgsFilter(attr, value) end

---@brief Play or reuse BGM/BGS described by map data.
---
--- - @param mapData Current map data.
--- - @param transitionSeconds Optional fade duration for a map-environment change.
--- Re-requesting the current file during fade-out keeps its source and restores its stored full target volume.
---@param mapData            Global.Audio.MapAudioData
---@param transitionSeconds? number
function MapAudio:playMapAudio(mapData, transitionSeconds) end

---@param deltaTime number
function MapAudio:onTick(deltaTime) end

---@brief Stop current BGM/BGS playback.
function MapAudio:stopMapAudio() end

return MapAudio
