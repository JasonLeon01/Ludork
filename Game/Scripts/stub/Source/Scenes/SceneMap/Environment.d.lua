---@meta Source.Scenes.SceneMap.Environment

---@class Source.Scenes.SceneMap.Environment
---@field new                                fun(): Source.Scenes.SceneMap.Environment
---@field private _audio                     Global.Audio.MapAudio
---@field private _mapAudioData              Global.Audio.MapAudioData | nil
---@field private _mapPanorama               string | nil
---@field private _needsRestore              boolean
---@field private _worldEnvironmentKey       string | nil
---@field private _ambientStart              sf.Color | nil
---@field private _ambientTarget             sf.Color | nil
---@field private _ambientElapsed            number
---@field private _MAP_AUDIO_TRANSITION_TIME number
---@field private _ENVIRONMENT_AVAILABILITY  { LOADED: Source.Scenes.SceneMap.Environment.Availability, EMPTY: Source.Scenes.SceneMap.Environment.Availability }
local Environment = {}

---@enum Source.Scenes.SceneMap.Environment.Availability
Environment._ENVIRONMENT_AVAILABILITY = {
    LOADED = "loaded",
    EMPTY = "empty"
}

function Environment:init() end

---@param gameMap GameMap
---@param mapData Global.GameMap.MapData | Global.WorldGameMap.WorldMapData
function Environment:applyMap(gameMap, mapData) end

---@param gameMap GameMap
---@param player  Source.MapActors.Player.Player
function Environment:enter(gameMap, player) end

function Environment:quit() end

---@param deltaTime number
function Environment:updateAudio(deltaTime) end

---@param gameMap   GameMap
---@param player    Source.MapActors.Player.Player
---@param deltaTime number
---@param force?    boolean
function Environment:updateWorld(gameMap, player, deltaTime, force) end

---@param bgm       string
---@param bgmFilter Engine.MusicFilter | nil
function Environment:playBgm(bgm, bgmFilter) end

---@param attr  string
---@param value Global.Audio.MusicFilterValue
function Environment:setBgmFilter(attr, value) end

---@param attr  string
---@param value Global.Audio.MusicFilterValue
function Environment:setBgsFilter(attr, value) end

function Environment:dispose() end

return Environment
