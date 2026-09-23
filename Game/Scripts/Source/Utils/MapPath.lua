local Path = require("Global.Utils.Path")

local MapPath = {}

function MapPath.Normalise(mapPath)
    local path = Path.NormaliseSeparators(tostring(mapPath or ""))
    while path:sub(1, 2) == "./" do
        path = path:sub(3)
    end
    local markerStart = path:find("Data/Maps/", 1, true)
    if markerStart ~= nil then
        path = path:sub(markerStart + #"Data/Maps/")
    end
    return path
end

function MapPath.WithoutExtension(mapPath)
    local path = MapPath.Normalise(mapPath)
    return path:gsub("%.[^%.]+$", "")
end

function MapPath.BasenameWithoutExtension(mapPath)
    local path = MapPath.Normalise(mapPath)
    path = path:match("([^/]+)$") or path
    return path:gsub("%.[^%.]+$", "")
end

return MapPath
