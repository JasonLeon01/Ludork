---@meta Source.SceneComponents.Tutorial

---@class Source.SceneComponents.Tutorial.Request
---@field key       string
---@field finished  boolean
---@field condition fun(): boolean

---@class Source.SceneComponents.Tutorial
---@field new               fun(scene: Source.Scenes.SceneMap.SceneMap): Source.SceneComponents.Tutorial
---@field _scene            Source.Scenes.SceneMap.SceneMap
---@field _queue            Source.SceneComponents.Tutorial.Request[]
---@field _requests         table<string, Source.SceneComponents.Tutorial.Request>
---@field _recordedRequests table<string, Source.SceneComponents.Tutorial.Request>
---@field _current          Source.SceneComponents.Tutorial.Request | nil
---@field _window           Internal.UIBase.LazyWindow<Source.Windows.WindowTutorial>
---@field _player           Source.MapActors.Player.Player | nil
---@field _moveEnabled      boolean
---@field _disposed         boolean
---@field _unregister       fun()
local TutorialController = {}

---@param scene Source.Scenes.SceneMap.SceneMap
function TutorialController:init(scene) end

---@return Source.Scenes.SceneMap.SceneMap
function TutorialController:getScene() end

--- Queue a configured key; already-triggered keys immediately complete.
---@param key string
---@return fun(): boolean
function TutorialController:request(key) end

--- Advance the queue after scene transitions; record a key only when its contraction starts.
function TutorialController:update() end

function TutorialController:refreshLocale() end

--- Update the overlay on the render thread while ordinary UI input remains captured.
---@param deltaTime number
function TutorialController:updateUI(deltaTime) end

--- Draw above windows, common tips and the region title.
function TutorialController:draw() end

---@return boolean
function TutorialController:isBlocking() end

---@private
function TutorialController:_restoreMovement() end

--- Cancel the originating graph events without resuming loops or execution outputs, then release capture.
--- Conditions held by direct Lua callers complete; queued keys remain unrecorded.
function TutorialController:cancel() end

function TutorialController:dispose() end

return TutorialController
