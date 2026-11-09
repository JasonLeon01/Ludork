---@class Global.WorldMapConstants.Module
---@field SPATIAL_CHUNK_SIZE            integer
---@field REGION_BUILD_CHUNK_SIZE       integer
---@field REGION_FIXED_CACHE_BYTES      integer
---@field REGION_LAYER_CELL_CACHE_BYTES integer
---@field IDLE_BUILD_STAGE              string
local WorldMapConstants = {}

WorldMapConstants.SPATIAL_CHUNK_SIZE = 32
WorldMapConstants.REGION_BUILD_CHUNK_SIZE = 8
WorldMapConstants.REGION_FIXED_CACHE_BYTES = 64 * 1024
WorldMapConstants.REGION_LAYER_CELL_CACHE_BYTES = 1280

WorldMapConstants.IDLE_BUILD_STAGE = "idle"

return WorldMapConstants
