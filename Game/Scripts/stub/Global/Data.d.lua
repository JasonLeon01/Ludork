---@meta Global.Data

---@alias Global.Data.ClassVarValue nil|boolean|number|string|table|userdata
---@alias Global.Data.JsonValue nil|boolean|number|string|table
---@alias Global.Data.GeneralValue boolean|number|string|table|userdata
---@alias Global.Data.CurveValue Engine.Curve | Engine.Vector2Curve | Engine.Vector3Curve | Engine.Vector4Curve

---@class Global.Data.TextStyleValues
---@field characterSize?    integer
---@field bold?             boolean
---@field italic?           boolean
---@field underlined?       boolean
---@field strikeThrough?    boolean
---@field fillColor?        sf.Color
---@field letterSpacing?    number
---@field lineSpacing?      number
---@field outlineColor?     sf.Color
---@field outlineThickness? number

---@class Global.Data.InitialLoadStage
---@field animationData       table<string, Engine.AnimationData>
---@field curveData           table<string, Global.Data.CurveValue>
---@field curveTypes          table<string, string>
---@field textConfigData      table<string, table>
---@field commonFunctionsData table
---@field tilesetData         table<string, Engine.Tileset>
---@field autoTileData        table<string, Engine.AutoTile>
---@field generalData         table<string, table<string, Global.Data.GeneralValue>>
---@field aborted             boolean
---@field committed           boolean

---@class Global.Data.GeneratedActor: Engine.Actor
---@field classVarChanges table<string, Global.Data.ClassVarValue> | nil

---@class Global.Data.GraphData
---@field parent      string | nil
---@field nodeGraph   table<string, { nodes: table[], links: table[] }>
---@field eventParams table<string, string[]> | nil
---@field startNodes  table | nil

---@class Global.Data.SerializedActorData
---@field bp       string
---@field tag      string | nil
---@field position integer[] | nil

---@class Global.Data.ActorData
---@field bp       string
---@field tag      string | nil
---@field position sf.Vector2u

---@class Global.Data.Cache
---@field animationData           table<string, Engine.AnimationData>
---@field curveData               table<string, Global.Data.CurveValue>
---@field curveTypes              table<string, string>
---@field textConfigData          table<string, table>
---@field plainTextConfigs        table<string, Engine.PlainTextConfig>
---@field richTextConfigs         table<string, Engine.RichTextConfig>
---@field commonFunctionsData     table<string, Global.Data.GraphData>
---@field tilesetData             table<string, Engine.Tileset>
---@field autoTileData            table<string, Engine.AutoTile>
---@field generalData             table<string, table<string, Global.Data.GeneralValue>>
---@field blueprintClassPaths     string[] | nil
---@field blueprintClassPathIndex table<string, string> | nil
---@field blueprintClassData      table<string, string | table>

---@class Global.Data.RuntimeOptions
---@field configResolver          fun(configName: string, settingName: string): string
---@field nodeModuleCandidates    fun(prefix: string): string[]
---@field conditionalActorBase    Class.ClassType<Engine.Actor>
---@field childActorComponentBase Class.ClassType<Engine.Component>

---@class (partial) Global.Data
---@field dataKinds integer
local Data = {}

---@brief Create session data services and install runtime providers once before the first scene.
---@param options Global.Data.RuntimeOptions
function Data.InitializeRuntime(options) end

---@return Global.Data.InitialLoadStage
function Data.BeginInitialLoad() end

---@param stage Global.Data.InitialLoadStage
---@param item  FileBatchItem
---@return string
function Data.ApplyInitialLoadItem(stage, item) end

---@param stage Global.Data.InitialLoadStage
function Data.CommitInitialLoad(stage) end

---@param stage Global.Data.InitialLoadStage
function Data.AbortInitialLoad(stage) end

---@brief Get the number of data kind categories.
---
--- - @return The number of data kinds.
---@return integer
function Data.GetDataKinds() end

---@brief Count loadable JSON data files under a directory.
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
function Data.CountLoadableFiles(dataRoot, needExt, defaultType, recursive) end

---@brief Load all animation caches from Engine.getAnimationCacheRoot().
---
--- - @param onFileLoaded Optional callback invoked after each file is loaded.
---@param onFileLoaded function | nil
function Data.LoadAnimations(onFileLoaded) end

---@brief Load all common function data from the Data/CommonFunctions directory.
---
--- - @param onFileLoaded Optional callback invoked after each file is loaded.
---@param onFileLoaded function | nil
function Data.LoadCommonFunctions(onFileLoaded) end

---@brief Load all tileset data from the Data/Tilesets directory.
---
--- - @param onFileLoaded Optional callback invoked after each file is loaded.
---@param onFileLoaded function | nil
function Data.LoadTilesets(onFileLoaded) end

---@brief Load all autotile data from the Data/AutoTiles directory.
---
--- - @param onFileLoaded Optional callback invoked after each file is loaded.
---@param onFileLoaded function | nil
function Data.LoadAutoTiles(onFileLoaded) end

---@brief Load all general data from the Data/General directory.
---
--- - @param onFileLoaded Optional callback invoked after each file is loaded.
---@param onFileLoaded function | nil
function Data.LoadGeneralData(onFileLoaded) end

---@brief Load all curve data from the Data/Curves directory.
---
--- - @param onFileLoaded Optional callback invoked after each file is loaded.
---@param onFileLoaded function | nil
function Data.LoadCurves(onFileLoaded) end

---@brief Load all text configuration data from the Data/TextConfigs directory.
---
--- - @param onFileLoaded Optional callback invoked after each file is loaded.
---@param onFileLoaded function | nil
function Data.LoadTextConfigs(onFileLoaded) end

---@brief Get animation data by name.
---
--- - @param name The animation name.
--- - @return Animation configuration dictionary.
---@param name string
---@return Engine.AnimationData
function Data.GetAnimation(name) end

---@brief Get a curve by name.
---
--- - @param name The curve name.
--- - @return The Curve object.
---@param name string
---@return Engine.Curve
function Data.GetCurve(name) end

---@param name string
---@return Engine.Vector2Curve
function Data.GetVector2Curve(name) end

---@param name string
---@return Engine.Vector3Curve
function Data.GetVector3Curve(name) end

---@param name string
---@return Engine.Vector4Curve
function Data.GetVector4Curve(name) end

---@param name string
---@return Engine.PlainTextConfig
function Data.GetPlainTextConfig(name) end

---@param name string
---@return Engine.RichTextConfig
function Data.GetRichTextConfig(name) end

---@brief Get a tileset by name.
---
--- - @param name The tileset name.
--- - @return The Tileset object.
---@param name string
---@return Engine.Tileset
function Data.GetTileset(name) end

---@brief Get an autotile by name.
---
--- - @param name The autotile name.
--- - @return The AutoTile object.
---@param name string
---@return Engine.AutoTile
function Data.GetAutoTile(name) end

---@brief Check whether an autotile is registered.
---
--- - @param name The autotile name.
--- - @return True if the autotile exists.
---@param name string
---@return boolean
function Data.HasAutoTile(name) end

---@brief Get general data by name.
---
--- - @param name The data name.
--- - @return General data dictionary whose member fields have already been canonicalised from their schema.
---@param name string
---@return table<string, Global.Data.GeneralValue>
function Data.GetGeneralData(name) end

---@brief Get a class by its blueprint path.
---
--- - @param classPath The class path.
--- - @return The resolved class type. Unknown paths raise.
---@param classPath string
---@return Class.ClassType<any> | nil
function Data.GetClass(classPath) end

---@brief Resolve a class and return its session definition table.
---
--- The same VM and path share one table, not a snapshot. Changes do not
--- invalidate generated classes, instance defaults or graph templates.
--- Use copy/deepcopy when an independent table is needed. Unknown paths raise.
--- - @param classPath The class path.
--- - @return The shared JSON definition, or nil for a module class without one.
---@param classPath string
---@return table<string, Global.Data.JsonValue> | nil
function Data.GetClassData(classPath) end

---@brief Resolve a class path from a path, class name, or generated blueprint class name.
---
--- - @param className Class path or generated class name.
--- - @return Resolved class path, or the original value when no mapping is found.
---@param className string
---@return string
function Data.ResolveClassPath(className) end

---@brief Get a common function graph by name.
---
--- - @param name The common function name.
--- - @return The function Graph.
---@param name string
---@return Engine.Graph
function Data.GetCommonFunction(name) end

---@brief Generate a node graph from data.
---
--- - @param data The graph data dictionary.
--- - @param parent Optional parent object.
--- - @param parentClass Optional parent class.
--- - @return The generated Graph.
---@generic T
---@param data        Global.Data.GraphData
---@param parent      T | nil
---@param parentClass Class.ClassType<any> | nil
---@return Engine.Graph
function Data.GenGraphFromData(data, parent, parentClass) end

---@brief Generate an actor from a resolved class path.
---
--- - @param classPath Actor class path.
--- - @param tag Optional actor tag.
--- - @param classVarChanges Optional blueprint instance variable overrides.
--- - @return The generated Actor, or nil.
---@param classPath       string
---@param tag             string | nil
---@param classVarChanges table<string, Global.Data.ClassVarValue> | nil
---@return Engine.Actor | nil
function Data.GenActorFromClassPath(classPath, tag, classVarChanges) end

---@brief Generate an actor from a class path or generated blueprint class name.
---
--- - @param className Actor class path or generated blueprint class name.
--- - @param tag Optional actor tag.
--- - @return The generated Actor, or nil.
---@param className string
---@param tag       string | nil
---@return Engine.Actor | nil
function Data.GenActorFromClassName(className, tag) end

---@brief Generate an actor from data.
---
--- - @param actorData The actor data dictionary.
--- - @param layerName The layer to spawn the actor on.
--- - @param classVarChanges Optional blueprint instance variable overrides.
--- - @return The generated Actor, or nil.
---@param actorData       Global.Data.ActorData
---@param layerName       string
---@param classVarChanges table<string, Global.Data.ClassVarValue> | nil
---@return Engine.Actor | nil
function Data.GenActorFromData(actorData, layerName, classVarChanges) end

--- Read static first-frame appearance without creating an Actor or instantiating its graph.
---@param classPath       string
---@param classVarChanges table<string, Global.Data.ClassVarValue> | nil
---@return Global.Data.ActorPreviewData | nil
function Data.GetActorPreviewData(classPath, classVarChanges) end

--- Release class appearance templates at the end of a preview cache lifetime.
function Data.ClearActorPreviewCache() end

return Data
