---@meta Source.MapActors.ConditionalActor

---@class Source.MapActors.ConditionalActor: Engine.Actor
---@field conditionVariable            string
---@field conditionOperator            "==" | "~=" | ">" | ">=" | "<" | "<="
---@field conditionValue               number | boolean | string
---@field private _conditionTarget     table<string, Source.GameInstance.RecordValue> | nil
---@field private _conditionName       string | nil
---@field private _conditionIdentifier string | nil
---@field private _conditionMap        GameMap | nil
---@field new                          fun(texture?: sf.Texture, rect?: sf.IntRect, tag?: string): Source.MapActors.ConditionalActor
local ConditionalActor = {}

--- Apply the current condition without subscribing or dispatching lifecycle events. An empty condition preserves visibility.
---@param variables table<string, Source.GameInstance.RecordValue>
function ConditionalActor:applyConditionVisibility(variables) end

--- Release this Actor's variable subscription without changing visibility or destruction records.
function ConditionalActor:releaseConditionMonitor() end

--- Replace the live variable subscription and immediately evaluate the current condition without running lifecycle events.
function ConditionalActor:refreshConditionMonitor() end

--- Release all condition subscriptions owned by a departing map, including removed Actors.
---@param gameMap GameMap
function ConditionalActor.ReleaseMapMonitors(gameMap) end

--- Subscribe to the configured variable and immediately evaluate the visibility condition.
function ConditionalActor:onCreate() end

function ConditionalActor:onDestroy() end

function ConditionalActor:onWorldSleep() end

---@param elapsedSeconds number
function ConditionalActor:onWorldWake(elapsedSeconds) end

---@private
function ConditionalActor:_registerConditionMonitor() end

---@private
function ConditionalActor:_updateConditionVisibility() end

return ConditionalActor
