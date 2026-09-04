---@meta Source.SceneComponents.MapAudio
---@alias Source.SceneComponents.SoundFilterValue boolean | number | sf.Time | sf.Vector3f | sf.SoundSource.Cone | nil
---@alias Source.SceneComponents.MusicFilterValue Source.SceneComponents.SoundFilterValue | sf.Music.TimeSpan

---@class Source.SceneComponents.MusicFilterValues
---@field loop?      boolean
---@field offset?    sf.Time
---@field pitch?     number
---@field pan?       number
---@field volume?    number
---@field loopPoint? sf.Music.TimeSpan

---@class Source.SceneComponents.MusicLoopPointData
---@field start number
---@field end   number

---@class Source.SceneComponents.MusicFilterData
---@field loop?      boolean
---@field offset?    number
---@field pitch?     number
---@field pan?       number
---@field volume?    number
---@field loopPoint? Source.SceneComponents.MusicLoopPointData

---@class Source.SceneComponents.SceneMapMusicTransitionBase
---@field elapsed      number
---@field duration     number
---@field startVolume  number
---@field targetVolume number

---@class Source.SceneComponents.SceneMapMusicFadeInTransition: Source.SceneComponents.SceneMapMusicTransitionBase
---@field phase "in"

---@class Source.SceneComponents.SceneMapMusicFadeOutTransition: Source.SceneComponents.SceneMapMusicTransitionBase
---@field phase          "out"
---@field switchDuration number

---@alias Source.SceneComponents.SceneMapMusicTransition Source.SceneComponents.SceneMapMusicFadeInTransition | Source.SceneComponents.SceneMapMusicFadeOutTransition

---@class Source.SceneComponents.SceneMapMusicState
---@field kind            "BGM" | "BGS"
---@field music           sf.Music | nil
---@field file            string
---@field targetVolume    number | nil
---@field requestedVolume number | nil
---@field transition      Source.SceneComponents.SceneMapMusicTransition | nil
---@field pendingFile     string
---@field pendingFilter   Engine.MusicFilter | nil

---@brief Manage map BGM/BGS playback and music filters.
---@class Source.SceneComponents.SceneMapAudioController
---@field _currentBgmMusic sf.Music | nil
---@field _currentBgmFile  string
---@field _currentBgsMusic sf.Music | nil
---@field _currentBgsFile  string
---@field _bgmState        Source.SceneComponents.SceneMapMusicState
---@field _bgsState        Source.SceneComponents.SceneMapMusicState
local SceneMapAudioController = {}

---@return Source.SceneComponents.SceneMapAudioController
function SceneMapAudioController.new(...) end

function SceneMapAudioController:init() end

---@brief Replace the current BGM with a new track.
---
--- - @param bgm Canonical music asset path under /Game/Assets/Musics.
--- - @param bgmFilter Optional music filter to apply.
---@param bgm       string
---@param bgmFilter Engine.MusicFilter | nil
function SceneMapAudioController:playBgm(bgm, bgmFilter) end

---@brief Set a filter attribute on the current BGM music.
---
--- - @param attr The filter attribute name.
--- - @param value The filter attribute value.
---@param attr  string
---@param value Source.SceneComponents.MusicFilterValue
function SceneMapAudioController:setBgmFilter(attr, value) end

---@brief Set a filter attribute on the current BGS music.
---
--- - @param attr The filter attribute name.
--- - @param value The filter attribute value.
---@param attr  string
---@param value Source.SceneComponents.MusicFilterValue
function SceneMapAudioController:setBgsFilter(attr, value) end

---@brief Play or reuse BGM/BGS described by map data.
---
--- - @param mapData Current map data.
--- - @param transitionSeconds Optional fade duration for a map-environment change.
--- Re-requesting the current file during fade-out keeps its source and restores its stored full target volume.
---@param mapData            Source.SceneComponents.MapAudioData
---@param transitionSeconds? number
function SceneMapAudioController:playMapAudio(mapData, transitionSeconds) end

---@param deltaTime number
function SceneMapAudioController:onTick(deltaTime) end

---@brief Stop current BGM/BGS playback.
function SceneMapAudioController:stopMapAudio() end

return SceneMapAudioController
