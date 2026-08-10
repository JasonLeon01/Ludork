---@meta Source.Locale.Core
---
--- Loads translation dictionaries and exposes localization functions.

local Core = {}

--- @brief Get loaded locale identifiers.
---
--- @return List of locale identifiers.
---@return string[]
function Core.GetLocaleKeys() end

--- @brief Load all locale data files from the locale source directory.
function Core.init() end

--- @brief Get localized content for a specific locale.
---
--- - localeKey: Locale identifier (e.g., "en_GB", "zh_CN").
--- - key: The translation key to look up.
---
--- @return The localized string, or the key itself if not found.
---@param localeKey string
---@param key string
---@return string
function Core.GetLocaleContent(localeKey, key) end

--- @brief Get localized content for the current language.
---
--- - key: The translation key to look up.
---
--- @return The localized string, or the key itself if not found.
---@param key string
---@return string
function Core.GetContent(key) end

--- @brief Get the locale dictionary for the current language.
---
--- @return The locale dictionary for the current language.
---@return table<string, string>
function Core.GetLocaleDict() end

--- @brief Resolve a locale key or replace localized placeholders in a string.
---
--- - value: A locale key or source value containing `{ID}` placeholders.
---
--- @return The localized or formatted value.
---@param value string
---@return string
function Core.ApplyStringLocaleFormat(value) end

---@param language string
function Core.setLanguage(language) end

---@return string
function Core.getLanguage() end

---@param language string
---@return boolean
function Core.hasLanguage(language) end

---@param key string
---@return boolean
function Core.hasKey(key) end

---@param language string | nil
---@return string
function Core.ResolveLanguage(language) end

return Core
