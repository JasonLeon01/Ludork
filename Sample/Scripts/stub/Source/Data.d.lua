---@meta Source.Data
---@alias Source.Data.ClassVarValue nil|boolean|number|string|table|userdata
---@alias Source.Data.JsonValue nil|boolean|number|string|table
---@alias Source.Data.GeneralValue boolean|number|string|table|userdata
---@alias Source.Data.CurveValue Engine.Curve | Engine.Vector2Curve | Engine.Vector3Curve | Engine.Vector4Curve

---@class Source.Data.TextStyleValues
---@field characterSize? integer
---@field bold? boolean
---@field italic? boolean
---@field underlined? boolean
---@field strikeThrough? boolean
---@field fillColor? sf.Color
---@field letterSpacing? number
---@field lineSpacing? number
---@field outlineColor? sf.Color
---@field outlineThickness? number

---@class Source.Data.EnemySpecialValues
---@field Poisoning? integer
---@field Weaken? integer
---@field Hard? boolean
---@field Magic? boolean
---@field MultiHit? integer
---@field Compete? boolean
---@field Domain? integer
---@field Flank? boolean
---@field Blockade? boolean
---@field Reborn? string

---@class Source.Data.InitialLoadStage
---@field _animationData       table<string, Engine.AnimationData>
---@field _curveData           table<string, Source.Data.CurveValue>
---@field _curveTypes          table<string, string>
---@field _textConfigData      table<string, table>
---@field _commonFunctionsData table
---@field _tilesetData         table<string, Engine.Tileset>
---@field _autoTileData        table<string, Engine.AutoTile>
---@field _generalData         table<string, table<string, Source.Data.GeneralValue>>
---@field _aborted             boolean
---@field _committed           boolean

---@class Source.Data.GeneratedActor : Engine.Actor
---@field _classVarChanges table<string, Source.Data.ClassVarValue> | nil

---@class Source.Data.GraphData
---@field parent      string | nil
---@field nodeGraph   table<string, { nodes: table[], links: table[] }>
---@field eventParams table<string, string[]> | nil
---@field startNodes  table | nil

---@class Source.Data.SerializedActorData
---@field bp       string
---@field tag      string | nil
---@field position integer[] | nil

---@class Source.Data.ActorData
---@field bp       string
---@field tag      string | nil
---@field position sf.Vector2u

---@class Source.Data.GeneralMemberData
---@field name string
---@field desc string
---@field icon string
---@field _graph Source.Data.GraphData | nil

---@class Source.Data.GeneralClassData : Source.Data.GeneralMemberData
---@field slot table<string, string>

---@class Source.Data.GeneralEquipData : Source.Data.GeneralMemberData
---@field slot string
---@field attrPlus table<string, integer>

---@class Source.Data.GeneralEnemyData : Source.Data.GeneralMemberData
---@field MAXHP         integer
---@field ATK           integer
---@field DEF           integer
---@field EXP           integer
---@field GOLD          integer
---@field drops         table<string, sf.Vector2i>
---@field special       Source.Data.EnemySpecialValues
---@field ANIMATION_KEY string

---@class Source.Data.GeneralItemData : Source.Data.GeneralMemberData
---@field usable boolean
---@field price integer
---@field cost boolean

---@class Source.Data.GeneralStateData : Source.Data.GeneralMemberData
---@field stackable boolean

---@class Source.Data.GeneralSpecialData : Source.Data.GeneralMemberData
local Data = {}

---@return Source.Data.InitialLoadStage
function Data.beginInitialLoad() end

---@param stage Source.Data.InitialLoadStage
---@param item  FileBatchItem
---@return string
function Data.applyInitialLoadItem(stage, item) end

---@param stage Source.Data.InitialLoadStage
function Data.commitInitialLoad(stage) end

---@param stage Source.Data.InitialLoadStage
function Data.abortInitialLoad(stage) end

--- @brief Get the number of data kind categories.
---
--- - @return The number of data kinds.
---@return integer
function Data.getDataKinds() end

--- @brief Count loadable JSON data files under a directory.
---
--- - @param dataRoot Root directory to scan.
--- - @param needExt Optional required file extension filter.
--- - @param defaultType Retained for source-call compatibility.
--- - @param recursive Whether to scan subdirectories recursively.
--- - @return Number of loadable files.
---@param dataRoot    string
---@param needExt     string | nil
---@param defaultType table<string, function>
---@param recursive   boolean
---@return integer
function Data.countLoadableFiles(dataRoot, needExt, defaultType, recursive) end

--- @brief Load all animation data from the Data/Animations directory.
---
--- - @param onFileLoaded Optional callback invoked after each file is loaded.
---@param onFileLoaded function | nil
function Data.loadAnimations(onFileLoaded) end

--- @brief Load all common function data from the Data/CommonFunctions directory.
---
--- - @param onFileLoaded Optional callback invoked after each file is loaded.
---@param onFileLoaded function | nil
function Data.loadCommonFunctions(onFileLoaded) end

--- @brief Load all tileset data from the Data/Tilesets directory.
---
--- - @param onFileLoaded Optional callback invoked after each file is loaded.
---@param onFileLoaded function | nil
function Data.loadTilesets(onFileLoaded) end

--- @brief Load all autotile data from the Data/AutoTiles directory.
---
--- - @param onFileLoaded Optional callback invoked after each file is loaded.
---@param onFileLoaded function | nil
function Data.loadAutoTiles(onFileLoaded) end

--- @brief Load all general data from the Data/General directory.
---
--- - @param onFileLoaded Optional callback invoked after each file is loaded.
---@param onFileLoaded function | nil
function Data.loadGeneralData(onFileLoaded) end

--- @brief Load all curve data from the Data/Curves directory.
---
--- - @param onFileLoaded Optional callback invoked after each file is loaded.
---@param onFileLoaded function | nil
function Data.loadCurves(onFileLoaded) end

--- @brief Load all text configuration data from the Data/TextConfigs directory.
---
--- - @param onFileLoaded Optional callback invoked after each file is loaded.
---@param onFileLoaded function | nil
function Data.loadTextConfigs(onFileLoaded) end

---@param fileName string
---@return string, string
function Data.splitCompound(fileName) end

--- @brief Get animation data by name.
---
--- - @param name The animation name.
--- - @return Animation configuration dictionary.
---@param name string
---@return Engine.AnimationData
function Data.getAnimation(name) end

--- @brief Get a curve by name.
---
--- - @param name The curve name.
--- - @return The Curve object.
---@param name string
---@return Engine.Curve
function Data.getCurve(name) end

---@param name string
---@return Engine.Vector2Curve
function Data.getVector2Curve(name) end

---@param name string
---@return Engine.Vector3Curve
function Data.getVector3Curve(name) end

---@param name string
---@return Engine.Vector4Curve
function Data.getVector4Curve(name) end

---@param name string
---@return Engine.PlainTextConfig
function Data.getPlainTextConfig(name) end

---@param name string
---@return Engine.RichTextConfig
function Data.getRichTextConfig(name) end

--- @brief Get a tileset by name.
---
--- - @param name The tileset name.
--- - @return The Tileset object.
---@param name string
---@return Engine.Tileset
function Data.getTileset(name) end

--- @brief Get an autotile by name.
---
--- - @param name The autotile name.
--- - @return The AutoTile object.
---@param name string
---@return Engine.AutoTile
function Data.getAutoTile(name) end

--- @brief Check whether an autotile is registered.
---
--- - @param name The autotile name.
--- - @return True if the autotile exists.
---@param name string
---@return boolean
function Data.hasAutoTile(name) end

--- @brief Get general data by name.
---
--- - @param name The data name.
--- - @return General data dictionary whose member fields have already been canonicalised from their schema.
---@param name string
---@return table<string, Source.Data.GeneralValue>
function Data.getGeneralData(name) end

--- @brief Get class data by its key.
---
--- - @param classKey The class key.
--- - @return Class data dictionary.
---@param key string
---@return Source.Data.GeneralClassData
function Data.getGeneralClassData(key) end

--- @brief Get enemy data by its key.
---
--- - @param enemyKey The enemy key.
--- - @return Enemy data dictionary.
---@param key string
---@return Source.Data.GeneralEnemyData
function Data.getGeneralEnemyData(key) end

--- @brief Get player data by its key.
---
--- - @param playerKey The player key.
--- - @return Player data dictionary.
---@param key string
---@return Source.Data.GeneralMemberData
function Data.getGeneralPlayerData(key) end

--- @brief Get equip data by its key.
---
--- - @return Equip data dictionary.
---@return table<string, Source.Data.GeneralEquipData>
function Data.getAllGeneralEquipData() end

--- @brief Get equip data by its key.
---
--- - @param equipKey The equip key.
--- - @return Equip data dictionary.
---@param key string
---@return Source.Data.GeneralEquipData
function Data.getGeneralEquipData(key) end

--- @brief Get item data by its key.
---
--- - @return Item data dictionary.
---@return table<string, Source.Data.GeneralItemData>
function Data.getAllGeneralItemData() end

--- @brief Get item data by its key.
---
--- - @param itemKey The item key.
--- - @return Item data dictionary.
---@param key string
---@return Source.Data.GeneralItemData
function Data.getGeneralItemData(key) end

--- @brief Get special data by its key.
---
--- - @param specialKey The special key.
--- - @return Special data dictionary.
---@param key string
---@return Source.Data.GeneralSpecialData
function Data.getGeneralSpecialData(key) end

--- @brief Get state data by its key.
---
--- - @param stateKey The state key.
--- - @return State data dictionary.
---@param key string
---@return Source.Data.GeneralStateData
function Data.getGeneralStateData(key) end

--- @brief Get a class by its blueprint path.
---
--- - @param classPath The class path.
--- - @return The class type, or nil when the path is not registered.
---@param classPath string
---@return Class.ClassType<any> | nil
function Data.getClass(classPath) end

--- @brief Get class data by its blueprint path.
---
--- - @param classPath The class path.
--- - @return Class data dictionary.
---@param classPath string
---@return table<string, Source.Data.JsonValue>
function Data.getClassData(classPath) end

--- @brief Resolve a class path from a path, class name, or generated blueprint class name.
---
--- - @param className Class path or generated class name.
--- - @return Resolved class path, or the original value when no mapping is found.
---@param className string
---@return string
function Data.resolveClassPath(className) end

--- @brief Get a common function graph by name.
---
--- - @param name The common function name.
--- - @return The function Graph.
---@param name string
---@return Engine.Graph
function Data.getCommonFunction(name) end

--- @brief Generate a node graph from data.
---
--- - @param data The graph data dictionary.
--- - @param parent Optional parent object.
--- - @param parentClass Optional parent class.
--- - @return The generated Graph.
---@generic T
---@param data        Source.Data.GraphData
---@param parent      T | nil
---@param parentClass Class.ClassType<any> | nil
---@return Engine.Graph
function Data.genGraphFromData(data, parent, parentClass) end

--- @brief Generate an actor from a resolved class path.
---
--- - @param classPath Actor class path.
--- - @param tag Optional actor tag.
--- - @param classVarChanges Optional blueprint instance variable overrides.
--- - @return The generated Actor, or nil.
---@param classPath       string
---@param tag             string | nil
---@param classVarChanges table<string, Source.Data.ClassVarValue> | nil
---@return Engine.Actor | nil
function Data.genActorFromClassPath(classPath, tag, classVarChanges) end

--- @brief Generate an actor from a class path or generated blueprint class name.
---
--- - @param className Actor class path or generated blueprint class name.
--- - @param tag Optional actor tag.
--- - @return The generated Actor, or nil.
---@param className string
---@param tag       string | nil
---@return Engine.Actor | nil
function Data.genActorFromClassName(className, tag) end

--- @brief Generate an actor from data.
---
--- - @param actorData The actor data dictionary.
--- - @param layerName The layer to spawn the actor on.
--- - @param classVarChanges Optional blueprint instance variable overrides.
--- - @return The generated Actor, or nil.
---@param actorData       Source.Data.ActorData
---@param layerName       string
---@param classVarChanges table<string, Source.Data.ClassVarValue> | nil
---@return Engine.Actor | nil
function Data.genActorFromData(actorData, layerName, classVarChanges) end

return Data
