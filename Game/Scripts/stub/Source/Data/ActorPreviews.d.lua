---@meta Source.Data.ActorPreviews

---@class Source.Data.ActorPreviewData
---@field texturePath       string
---@field rect              sf.IntRect
---@field translation       sf.Vector2f
---@field rotation          number
---@field scale             sf.Vector2f
---@field origin            sf.Vector2f
---@field visible           boolean
---@field hue               number
---@field shaderPath        string
---@field conditionVariable string
---@field conditionOperator string
---@field conditionValue    number | boolean | string
---@field child             Source.Data.ActorPreviewData | nil
---@field childOffset       sf.Vector2f

---@class Source.Data.ActorPreviewTemplate
---@field classModel     Class.ClassType<any>
---@field conditional    boolean
---@field data           Source.Data.ActorPreviewData
---@field childClassName string
---@field componentType  Class.ClassType<Source.Components.ChildActorComponent> | nil

---@class Source.Data.ActorPreviews
---@field private _classDict        Engine.ClassDict
---@field private _resolveClassPath fun(classPath: string): string
---@field private _templates        table<string, Source.Data.ActorPreviewTemplate>
---@field new                       fun(classDict: Engine.ClassDict, resolveClassPath: fun(classPath: string): string): Source.Data.ActorPreviews
local ActorPreviews = {}

---@param classDict        Engine.ClassDict
---@param resolveClassPath fun(classPath: string): string
function ActorPreviews:init(classDict, resolveClassPath) end

function ActorPreviews:clear() end

---@private
---@param classPath string
---@return Source.Data.ActorPreviewTemplate | nil
function ActorPreviews:_getTemplate(classPath) end

---@private
---@param classPath       string
---@param classVarChanges table<string, Source.Data.ClassVarValue> | nil
---@param ancestors       table<string, boolean>
---@return Source.Data.ActorPreviewData | nil
function ActorPreviews:_get(classPath, classVarChanges, ancestors) end

---@param classPath       string
---@param classVarChanges table<string, Source.Data.ClassVarValue> | nil
---@return Source.Data.ActorPreviewData | nil
function ActorPreviews:get(classPath, classVarChanges) end

return ActorPreviews
