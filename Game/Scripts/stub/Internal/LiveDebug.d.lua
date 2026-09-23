---@meta Internal.LiveDebug

---@class Internal.LiveDebug.TileEdit
---@field x      integer
---@field y      integer
---@field tileId integer | string | lightuserdata | nil

---@class Internal.LiveDebug.TileChange
---@field layer    string
---@field x        integer
---@field y        integer
---@field tile     integer | lightuserdata
---@field autoTile string | lightuserdata

---@class Internal.LiveDebug.Request
---@field action      string
---@field context     string | nil
---@field actorId     string | nil
---@field includeInfo boolean | nil
---@field layer       string | nil
---@field tiles       Internal.LiveDebug.TileEdit[] | nil
---@field x           integer | nil
---@field y           integer | nil
---@field name        string | nil
---@field value       any

---@class Internal.LiveDebug.Visual
---@field texturePath string
---@field textureRect number[]
---@field translation number[]
---@field scale       number[]
---@field origin      number[]
---@field rotation    number
---@field hue         number
---@field visible     boolean
---@field shaderPath  string

---@class Internal.LiveDebug.Actor
---@field bp              string
---@field type            string
---@field tag             string
---@field runtimeId       string
---@field parentRuntimeId string | nil              Direct live parent identity, retained even when that parent lies outside the current snapshot.
---@field position        integer[]
---@field visual          Internal.LiveDebug.Visual

---@class Internal.LiveDebug.Info
---@field actorId string
---@field values  table<string, any>
---@field schema  table<string, any>

---@class Internal.LiveDebug.Response
---@field success     boolean
---@field context     string
---@field editable    boolean
---@field mapKey      string
---@field status      string
---@field error       string | nil
---@field map         Internal.LiveDebug.Map | nil
---@field actors      table<string, Internal.LiveDebug.Actor[]> | nil
---@field variables   table<string, table<string, any>> | nil
---@field info        Internal.LiveDebug.Info | nil
---@field tileUpdated boolean | nil
---@field tileChanges Internal.LiveDebug.TileChange[] | nil

---@class Internal.LiveDebug.Map
---@field mapName           string
---@field width             integer
---@field height            integer
---@field layerOrder        string[]
---@field layers            table<string, Source.SceneComponents.MapLayerData>
---@field actors            table<string, Internal.LiveDebug.Actor[]>
---@field BPClassVarChanged table<string, table<string, any>>

---@class Internal.LiveDebug.View
---@field gameMap GameMap
---@field tilemap Engine.Tilemap
---@field player  Engine.Actor
---@field region  Source.SceneComponents.WorldRegionData | nil
---@field x       integer
---@field y       integer
---@field width   integer
---@field height  integer
---@field mapKey  string

---@class Internal.LiveDebug.Module
local LiveDebug = {}

--- Register the handler only for an editor run with LUDORK_LIVE_DEBUG=1. Collection starts after a live-debug start request.
function LiveDebug.Install() end

--- Release the request handler and scene references when the game host exits.
function LiveDebug.Uninstall() end

---@param scene Source.Scenes.SceneMap.SceneMap
function LiveDebug.BindScene(scene) end

---@param scene Source.Scenes.SceneMap.SceneMap
function LiveDebug.UnbindScene(scene) end

return LiveDebug
