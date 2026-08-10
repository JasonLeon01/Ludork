local Pool = {}

Pool.stashedObjects = {}

local MAX_STASHED_OBJECTS = 64

---@generic T: table
function Pool.Get(className, classType, objectTable)
    ---@cast classType T & { new: fun(...: any): T }
    local objects = Pool.stashedObjects[className]
    if objects == nil then
        objects = {}
        Pool.stashedObjects[className] = objects
    end
    local object
    local objectCount = #objects
    if objectCount == 0 then
        object = classType.new()
    else
        object = objects[objectCount]
        objects[objectCount] = nil
    end
    if objectTable ~= nil then
        for key, value in pairs(objectTable) do
            object[key] = value
        end
    end
    return object
end

function Pool.Put(className, object)
    local objects = Pool.stashedObjects[className]
    if objects == nil then
        objects = {}
        Pool.stashedObjects[className] = objects
    end
    local objectCount = #objects
    if objectCount < MAX_STASHED_OBJECTS then
        objects[objectCount + 1] = object
    end
end

function Pool.Release(className)
    Pool.stashedObjects[className] = nil
end

return Pool
