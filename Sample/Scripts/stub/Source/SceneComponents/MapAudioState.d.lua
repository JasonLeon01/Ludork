---@meta Source.SceneComponents.MapAudioState

local MapAudioState = {}

---@param kind string
---@return Source.SceneComponents.SceneMapMusicState
function MapAudioState.New(kind) end

---@param state Source.SceneComponents.SceneMapMusicState
function MapAudioState.Stop(state) end

---@param state    Source.SceneComponents.SceneMapMusicState
---@param file     string
---@param filter   Engine.MusicFilter | nil
---@param duration number
function MapAudioState.Request(state, file, filter, duration) end

---@param state     Source.SceneComponents.SceneMapMusicState
---@param deltaTime number
function MapAudioState.Update(state, deltaTime) end

---@param music sf.Music | nil
---@param attr  string
---@param value Source.SceneComponents.MusicFilterValue
function MapAudioState.SetFilterAttribute(music, attr, value) end

---@param data Source.SceneComponents.MusicFilterData
---@return Engine.MusicFilter | nil
function MapAudioState.BuildFilter(data) end

---@param controller Source.SceneComponents.SceneMapAudioController
function MapAudioState.SyncController(controller) end

return MapAudioState
