---@meta Source.Scenes.SceneMap.Dialogue

---@class Source.Scenes.SceneMap.Dialogue.LocaleContext
---@field name         string
---@field localeArgs   table<string, any>
---@field localVars    table<string, any>
---@field instanceVars table<string, any>

---@class Source.Scenes.SceneMap.Dialogue.MessageLocaleSource
---@field kind    "message"
---@field context Source.Scenes.SceneMap.Dialogue.LocaleContext
---@field content string

---@class Source.Scenes.SceneMap.Dialogue.SelectionLocaleSource
---@field kind    "selection"
---@field context Source.Scenes.SceneMap.Dialogue.LocaleContext
---@field content string[]

---@class Source.Scenes.SceneMap.Dialogue
---@field new              fun(scene: Source.Scenes.SceneMap.SceneMap): Source.Scenes.SceneMap.Dialogue
---@field _scene           Source.Scenes.SceneMap.SceneMap
---@field _window          Internal.UIBase.LazyWindow<Source.Windows.WindowMessage>
---@field _operation       Engine.AsyncOperation | nil
---@field _voice           sf.Sound | nil
---@field _restoreMovement fun() | nil
---@field _localeSource    Source.Scenes.SceneMap.Dialogue.MessageLocaleSource | Source.Scenes.SceneMap.Dialogue.SelectionLocaleSource | nil
local Dialogue = {}

---@param scene Source.Scenes.SceneMap.SceneMap
function Dialogue:init(scene) end

--- Snapshot user variables belonging to the exact active Blueprint entry function.
--- Ordinary Lua calls return an empty table.
---@param nodeFunction function
---@return table<string, any>
function Dialogue.CaptureLocalVars(nodeFunction) end

--- Replace the active dialogue and retain its text and variable snapshots until completion or cancellation.
---@param name       string
---@param message    string
---@param refActor   Engine.Actor | nil
---@param localeArgs table<string, any> | nil
---@param localVars  table<string, any>
---@return Engine.AsyncOperation
function Dialogue:showMessage(name, message, refActor, localeArgs, localVars) end

--- Stop the exact voice started by this dialogue when it finishes or is cancelled.
---@param name          string
---@param message       string
---@param voiceFileName string
---@param refActor      Engine.Actor | nil
---@param minDistance   number | nil
---@param spatial       boolean | nil
---@param localVars     table<string, any>
---@return Engine.AsyncOperation
function Dialogue:showVoiceMessage(name, message, voiceFileName, refActor, minDistance, spatial, localVars) end

---@param name        string
---@param options     string[]
---@param refActor    Engine.Actor | nil
---@param allowCancel boolean | nil
---@param localeArgs  table<string, any> | nil
---@param localVars   table<string, any>
---@return Engine.AsyncOperation
function Dialogue:showSelection(name, options, refActor, allowCancel, localeArgs, localVars) end

---@return boolean
function Dialogue:isBlocking() end

function Dialogue:update() end

--- Reformat the active dialogue in place without replacing its callbacks or variable snapshots.
function Dialogue:refreshLocale() end

--- Cancel the operation without completion outputs, stop its voice and restore its original player once.
function Dialogue:cancel() end

function Dialogue:dispose() end

return Dialogue
