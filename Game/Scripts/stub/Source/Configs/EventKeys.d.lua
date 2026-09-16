---@meta Source.Configs.EventKeys
---@brief Shared EventBus key constants.
---

---@brief Kind values for AbilitySystemChanged payloads.
---@class Source.Configs.EventKeys.AbilitySystemChangeKind
---@field Attribute string
---@field State     string

---@brief Kind values for PlayerChanged payloads.
---@class Source.Configs.EventKeys.PlayerChangeKind
---@field Inventory string
---@field Name      string
---@field Map       string

---@brief EventBus payload for player and ability-system changes.
---@class Source.Configs.EventKeys.ChangePayload
---@field owner any | nil
---@field kind  string
---@field name  string | nil

---@brief EventBus event names used by runtime systems.
---@class Source.Configs.EventKeys
---@field LocaleChanged             string
---@field AbilitySystemChanged      string
---@field PlayerChanged             string
---@field AbilitySystemChangeKind   Source.Configs.EventKeys.AbilitySystemChangeKind
---@field PlayerChangeKind          Source.Configs.EventKeys.PlayerChangeKind
local EventKeys = {}

return EventKeys
