---@meta Source.Config.Main

---@class Source.Config.Main.Module
---@field SupportedLanguages string[]
local MainConfig = {}

---@param maximumScale number | nil
---@param configuredScale number
---@return number[] values
---@return number effectiveScale
function MainConfig.GetDisplayScaleOptions(maximumScale, configuredScale) end

---@return string iniFilePath
---@return ConfigParser iniFile
function MainConfig.loadOrCreate() end

return MainConfig
