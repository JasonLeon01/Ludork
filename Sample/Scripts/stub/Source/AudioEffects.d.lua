---@meta Source.AudioEffects

---@class Source.AudioEffects.Factories
---@field Echo fun(delay?: number, decay?: number): sf.SoundSource.EffectProcessor
---@field Distortion fun(drive?: number, threshold?: number): sf.SoundSource.EffectProcessor
---@field Underwater fun(depth?: number, bubbleIntensity?: number): sf.SoundSource.EffectProcessor
---@field BehindWall fun(cutoffFrequency?: number, transmission?: number): sf.SoundSource.EffectProcessor

---@class Source.AudioEffects
---@field EFFECTS Source.AudioEffects.Factories
local AudioEffects = {}

return AudioEffects
