local GlobalCore = require("GlobalCore")

local Tutorial = {}
---@type table<GlobalCore.SceneBase, Global.Tutorial.Handler | nil>
local handlers = setmetatable({}, { __mode = "k" })

function Tutorial.Register(scene, show)
    assert(handlers[scene] == nil, "Tutorial handler is already registered for this scene")
    local handler = { show = show }
    handlers[scene] = handler
    return function ()
        if handlers[scene] == handler then
            handlers[scene] = nil
        end
    end
end

---@return Global.Tutorial.Handler
local function currentHandler()
    local scene = assert(GlobalCore.SceneManager.getScene(), "Tutorial requires an active scene")
    local handler = handlers[scene]
    assert(handler ~= nil, "Tutorial handler is not registered for the active scene")
    return handler
end

function Tutorial.Show(key)
    return currentHandler().show(key)
end

return Tutorial
