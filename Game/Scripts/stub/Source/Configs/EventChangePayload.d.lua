---@meta

---@brief EventBus payload for player and ability-system changes.
---@class Source.Configs.EventChangePayload
---@field owner any | nil
---@field kind  Enums.AbilitySystemChangeKind | Enums.PlayerChangeKind
---@field name  string | nil

--- Published only after a shop record is actually added or removed.
---@class Source.Configs.ShopRecordsChangePayload
---@field instance Source.GameInstance.GameInstance
