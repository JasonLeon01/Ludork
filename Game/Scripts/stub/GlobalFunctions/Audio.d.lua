---@meta GlobalFunctions.Audio

---@alias GlobalFunctions.Audio.SoundFilterValue boolean | number | sf.Time | sf.Vector3f | sf.SoundSource.Cone | nil
---@alias GlobalFunctions.Audio.MusicFilterValue GlobalFunctions.Audio.SoundFilterValue | sf.Music.TimeSpan

local Audio = {}

---@param attr  string
---@param value GlobalFunctions.Audio.SoundFilterValue
function Audio.EditSoundFilter(attr, value) end

---@param attr  string
---@param value GlobalFunctions.Audio.MusicFilterValue
function Audio.EditMusicFilter(attr, value) end

--- Select a Lua audio-effect preset for future playback in one category.
--- Existing sources keep their current processor. Unknown preset names fail immediately.
--- Each future source receives a fresh processor closure in its own isolated audio Lua state.
--- Use the literal string `nil` to clear the category preset.
---@param audioType string
---@param effect    string
function Audio.SetEffect(audioType, effect) end

---@param soundFileName string
---@param applyFilter   boolean
function Audio.PlaySound(soundFileName, applyFilter) end

---@param musicFileName string
---@param applyFilter   boolean
function Audio.PlayMusic(musicFileName, applyFilter) end

---@param attr  string
---@param value GlobalFunctions.Audio.MusicFilterValue
function Audio.SetBgmFilter(attr, value) end

---@param attr  string
---@param value GlobalFunctions.Audio.MusicFilterValue
function Audio.SetBgsFilter(attr, value) end

return Audio
