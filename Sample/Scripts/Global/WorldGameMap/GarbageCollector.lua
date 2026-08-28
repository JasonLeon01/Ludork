local GarbageCollector = {}

local users = 0
---@type boolean | nil
local wasRunning = nil

function GarbageCollector.Acquire()
    if users == 0 then
        wasRunning = collectgarbage("isrunning")
        collectgarbage("stop")
    end
    users = users + 1
end

function GarbageCollector.Release()
    assert(users > 0, "World garbage collector registration is unbalanced")
    if users == 1 then
        if bool(wasRunning) then
            collectgarbage("restart")
        else
            collectgarbage("stop")
        end
        users = 0
        wasRunning = nil
    else
        users = users - 1
    end
end

function GarbageCollector.Step()
    if users > 0 and bool(wasRunning) then
        collectgarbage("step", 0)
    end
end

return GarbageCollector
