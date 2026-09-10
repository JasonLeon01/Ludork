---@meta Source.AudioEffects

---@alias Source.AudioEffects.Attacher fun(source: sf.SoundSource, control: GlobalCore.AudioEffectControl, sampleRate: integer)

---@brief Lua audio-effect presets used by the gameplay audio Manager.
local AudioEffects = {}

---@brief Resolve an audio-effect name for future per-source attachment.
---
--- Each attacher delegates to the native control, which creates an isolated Lua
--- state and installs exactly one fresh processor for that source. Processor
--- state is therefore never shared between sources. Missing or duplicate
--- processor installation fails synchronously.
--- The literal string `nil` returns nil and clears the selected Manager category.
--- An unknown name raises an error immediately.
---@param name string
---@return Source.AudioEffects.Attacher | nil
function AudioEffects.Get(name) end

return AudioEffects
