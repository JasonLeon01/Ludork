---@meta Global.Audio.MapAudioState

---@class Global.Audio.MapAudioState.MusicFilterValues
---@field loop?      boolean
---@field offset?    sf.Time
---@field pitch?     number
---@field pan?       number
---@field volume?    number
---@field loopPoint? sf.Music.TimeSpan

---@class Global.Audio.MapAudioState.TransitionBase
---@field elapsed      number
---@field duration     number
---@field startVolume  number
---@field targetVolume number

---@class Global.Audio.MapAudioState.FadeInTransition: Global.Audio.MapAudioState.TransitionBase
---@field phase "in"

---@class Global.Audio.MapAudioState.FadeOutTransition: Global.Audio.MapAudioState.TransitionBase
---@field phase          "out"
---@field switchDuration number

---@alias Global.Audio.MapAudioState.Transition Global.Audio.MapAudioState.FadeInTransition | Global.Audio.MapAudioState.FadeOutTransition

---@class Global.Audio.MapAudioState.State
---@field kind            "BGM" | "BGS"
---@field music           sf.Music | nil
---@field file            string
---@field targetVolume    number | nil
---@field requestedVolume number | nil
---@field transition      Global.Audio.MapAudioState.Transition | nil
---@field pendingFile     string
---@field pendingFilter   Engine.MusicFilter | nil

local MapAudioState = {}

---@param kind string
---@return Global.Audio.MapAudioState.State
function MapAudioState.New(kind) end

---@param state Global.Audio.MapAudioState.State
function MapAudioState.Stop(state) end

---@param state    Global.Audio.MapAudioState.State
---@param file     string
---@param filter   Engine.MusicFilter | nil
---@param duration number
function MapAudioState.Request(state, file, filter, duration) end

---@param state     Global.Audio.MapAudioState.State
---@param deltaTime number
function MapAudioState.Update(state, deltaTime) end

---@param music sf.Music | nil
---@param attr  string
---@param value Global.Audio.MusicFilterValue
function MapAudioState.SetFilterAttribute(music, attr, value) end

---@param data Global.Audio.MusicFilterData
---@return Engine.MusicFilter | nil
function MapAudioState.BuildFilter(data) end

return MapAudioState
