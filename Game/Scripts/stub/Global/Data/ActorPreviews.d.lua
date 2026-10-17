---@meta Global.Data.ActorPreviews

---@class Global.Data.ActorPreviewData
---@field texturePath        string
---@field rect               sf.IntRect
---@field translation        sf.Vector2f
---@field rotation           number
---@field scale              sf.Vector2f
---@field origin             sf.Vector2f
---@field visible            boolean
---@field material           string
---@field materialParameters table<string, any>
---@field conditionVariable  string
---@field conditionOperator  string
---@field conditionValue     number | boolean | string
---@field child              Global.Data.ActorPreviewData | nil
---@field childOffset        sf.Vector2f

---@class Global.Data.ActorPreviewTemplate
---@field classModel     Class.ClassType<any>
---@field conditional    boolean
---@field data           Global.Data.ActorPreviewData
---@field childClassName string
---@field componentType  Class.ClassType<Engine.Component> | nil

---@class Global.Data.ActorPreviews
---@field private _classDict               Engine.ClassDict
---@field private _resolveClassPath        fun(classPath: string): string
---@field private _configResolver          fun(configName: string, settingName: string): string
---@field private _conditionalActorBase    Class.ClassType<Engine.Actor>
---@field private _childActorComponentBase Class.ClassType<Engine.Component>
---@field private _templates               table<string, Global.Data.ActorPreviewTemplate>
---@field new                              fun(classDict: Engine.ClassDict, resolveClassPath: (fun(classPath: string): string), options: Global.Data.RuntimeOptions): Global.Data.ActorPreviews
local ActorPreviews = {}

---@param classDict        Engine.ClassDict
---@param resolveClassPath fun(classPath: string): string
---@param options          Global.Data.RuntimeOptions
function ActorPreviews:init(classDict, resolveClassPath, options) end

function ActorPreviews:clear() end

---@private
---@param classPath string
---@return Global.Data.ActorPreviewTemplate | nil
function ActorPreviews:_getTemplate(classPath) end

---@private
---@param classPath       string
---@param classVarChanges table<string, Global.Data.ClassVarValue> | nil
---@param ancestors       table<string, boolean>
---@return Global.Data.ActorPreviewData | nil
function ActorPreviews:_get(classPath, classVarChanges, ancestors) end

---@param classPath       string
---@param classVarChanges table<string, Global.Data.ClassVarValue> | nil
---@return Global.Data.ActorPreviewData | nil
function ActorPreviews:get(classPath, classVarChanges) end

return ActorPreviews
