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
    local path = os.path.splitext(MapPath.Normalise(mapPath))
    return Path.NormaliseSeparators(path)
end

function MapPath.BasenameWithoutExtension(mapPath)
    return os.path.basename(MapPath.WithoutExtension(mapPath))
end

return MapPath
