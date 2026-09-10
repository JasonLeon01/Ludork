---@meta Global.Utils.NodeCompiler

---@alias NodeCompiler.Callable function|table|userdata
---@alias NodeCompiler.MetadataType string|string[]

---@class NodeCompiler.Parameter
---@field name string
---@field type NodeCompiler.MetadataType

---@class NodeCompiler.OrderedEntry
---@field name   string
---@field values table

---@class NodeCompiler.MemberMetadata
---@field parameters     NodeCompiler.Parameter[]
---@field parameterTypes table<string, NodeCompiler.MetadataType>
---@field defaults       table
---@field returns        NodeCompiler.Parameter[]
---@field execSplit      NodeCompiler.OrderedEntry[]
---@field latentStates   NodeCompiler.OrderedEntry[]
---@field latent         boolean
---@field loop           boolean
---@field pure           boolean
---@field loopNode       string
---@field kind           string
---@field needsRefLocal  boolean

---@class NodeCompiler.ContextRoot
---@field name  string
---@field value table

---@class NodeCompiler.Context
---@field roots?            NodeCompiler.ContextRoot[]
---@field moduleCandidates? fun(prefix: string): string[]

---@class NodeCompiler.Definition
---@field callable        NodeCompiler.Callable
---@field memberMeta      NodeCompiler.MemberMetadata
---@field paramNames      string[]
---@field isSelf          boolean
---@field displayName     string
---@field declaringModule string

---@param functionName string
---@param parentClass  Class.ClassType<any> | nil
---@param context      NodeCompiler.Context | nil
---@return NodeCompiler.Definition | nil
function NodeCompiler.Compile(functionName, parentClass, context) end

return NodeCompiler
