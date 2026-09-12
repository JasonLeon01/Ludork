local Engine = require("Engine")
local MapConstants = require("Source.Configs.MapConstants")

local ResourceFileConstants = Engine.ResourceFileConstants

local WorldDirectoryValidator = {}

---@param mapsRoot     string
---@param manifestPath string
function WorldDirectoryValidator.Validate(mapsRoot, manifestPath)
    assert(
        os.path.basename(manifestPath) == MapConstants.WORLD_MANIFEST_FILE,
        "World manifest must use the fixed _world.json filename: " .. manifestPath
    )
    local worldDirectory = os.path.dirname(manifestPath)
    local directoryName = os.path.basename(worldDirectory)
    assert(
        bool(directoryName) and os.path.dirname(worldDirectory) == "",
        "World must be one direct directory under Data/Maps: " .. manifestPath
    )
    local directoryPath = os.path.join(mapsRoot, directoryName)
    if not os.path.isdir(directoryPath) then
        local packedManifestPath = os.path.join(directoryPath, MapConstants.WORLD_MANIFEST_FILE)
        assert(Engine.jsonExists(packedManifestPath), "World directory does not exist: " .. worldDirectory)
        return
    end
    local manifestCount = 0
    for _, entry in ipairs(os.listdir(directoryPath)) do
        local entryPath = os.path.join(directoryPath, entry)
        assert(not os.path.isdir(entryPath), "World directories cannot contain nested directories: " .. entry)
        assert(os.path.isfile(entryPath), "World directory entry must be a regular file: " .. entry)
        local _, extension = os.path.splitext(entry)
        if extension:lower() == ResourceFileConstants.DATA_EXTENSION and entry:lower() == MapConstants.WORLD_MANIFEST_FILE then
            manifestCount = manifestCount + 1
            assert(
                entry == MapConstants.WORLD_MANIFEST_FILE,
                "World manifest filename is case-sensitive and must be _world.json: " .. entry
            )
        end
    end
    assert(manifestCount == 1, "World directory must contain exactly one _world.json manifest: " .. worldDirectory)
end

return WorldDirectoryValidator
