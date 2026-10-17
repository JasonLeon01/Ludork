---@meta Source.Data

---@class Source.Data.EnemySpecialValues
---@field Poisoning? integer
---@field Weaken?    integer
---@field Hard?      boolean
---@field Magic?     boolean
---@field MultiHit?  integer
---@field Compete?   boolean
---@field Domain?    integer
---@field Flank?     boolean
---@field Blockade?  boolean
---@field Reborn?    string

---@class Source.Data.GeneralMemberData
---@field name   string
---@field desc   string
---@field icon   string
---@field _graph Global.Data.GraphData | nil

---@class Source.Data.GeneralClassData: Source.Data.GeneralMemberData
---@field slot table<string, string>

---@class Source.Data.GeneralEquipData: Source.Data.GeneralMemberData
---@field slot     string
---@field attrPlus table<string, integer>

---@class Source.Data.GeneralEnemyData: Source.Data.GeneralMemberData
---@field MAXHP         integer
---@field ATK           integer
---@field DEF           integer
---@field EXP           integer
---@field GOLD          integer
---@field drops         table<string, sf.Vector2i>
---@field special       Source.Data.EnemySpecialValues
---@field ANIMATION_KEY string

---@class Source.Data.GeneralItemData: Source.Data.GeneralMemberData
---@field usable boolean
---@field price  integer
---@field cost   boolean

---@class Source.Data.GeneralStateData: Source.Data.GeneralMemberData
---@field stackable boolean

---@class Source.Data.GeneralSpecialData: Source.Data.GeneralMemberData

---@class (partial) Source.Data
local Data = {}

---@brief Initialise shared data services with the project's configuration, node search order and Actor preview types.
function Data.InitializeRuntime() end

---@param typeName string
---@param memberID string
---@return GlobalCore.AttributeSet
function Data.CreateGeneralAttributeSet(typeName, memberID) end

---@brief Get class data by its key.
---
--- - @param classKey The class key.
--- - @return Class data dictionary.
---@param key string
---@return Source.Data.GeneralClassData
function Data.GetGeneralClassData(key) end

---@brief Get enemy data by its key.
---
--- - @param enemyKey The enemy key.
--- - @return Enemy data dictionary.
---@param key string
---@return Source.Data.GeneralEnemyData
function Data.GetGeneralEnemyData(key) end

---@brief Get player data by its key.
---
--- - @param playerKey The player key.
--- - @return Player data dictionary.
---@param key string
---@return Source.Data.GeneralMemberData
function Data.GetGeneralPlayerData(key) end

---@brief Get equip data by its key.
---
--- - @return Equip data dictionary.
---@return table<string, Source.Data.GeneralEquipData>
function Data.GetAllGeneralEquipData() end

---@brief Get equip data by its key.
---
--- - @param equipKey The equip key.
--- - @return Equip data dictionary.
---@param key string
---@return Source.Data.GeneralEquipData
function Data.GetGeneralEquipData(key) end

---@brief Get item data by its key.
---
--- - @return Item data dictionary.
---@return table<string, Source.Data.GeneralItemData>
function Data.GetAllGeneralItemData() end

---@brief Get item data by its key.
---
--- - @param itemKey The item key.
--- - @return Item data dictionary.
---@param key string
---@return Source.Data.GeneralItemData
function Data.GetGeneralItemData(key) end

---@brief Get special data by its key.
---
--- - @param specialKey The special key.
--- - @return Special data dictionary.
---@param key string
---@return Source.Data.GeneralSpecialData
function Data.GetGeneralSpecialData(key) end

---@brief Get state data by its key.
---
--- - @param stateKey The state key.
--- - @return State data dictionary.
---@param key string
---@return Source.Data.GeneralStateData
function Data.GetGeneralStateData(key) end

return Data
