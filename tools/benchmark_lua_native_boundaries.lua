local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local Lighting = require("Global.GameMap.Lighting")
local Logging = require("Global.Utils.Logging")
local Data = require("Source.Data")
local TextLayout = require("Source.TextLayout")

local ManagerFunctions = GlobalFunctions.Manager
local Light = GlobalCore.Light

local function benchmark(name, iterations, callback)
    for _ = 1, math.max(1, math.floor(iterations / 10)) do
        callback()
    end
    collectgarbage("collect")
    local start = os.clock()
    for _ = 1, iterations do
        callback()
    end
    local elapsed = os.clock() - start
    Logging.info(
        "BENCHMARK name=%s iterations=%d total_ms=%.3f mean_ms=%.6f",
        name, iterations, elapsed * 1000.0, elapsed * 1000.0 / iterations
    )
end

local function appendVertex(vertices, vertex, x, y, textureCoordinate)
    vertex.position = sf.Vector2f.new(x, y)
    vertex.texCoords = textureCoordinate
    vertices:append(vertex)
end

local function buildPixelGridVerticesLua(origin, size)
    local vertices = sf.VertexArray.new(sf.PrimitiveType.Triangles)
    local vertex = sf.Vertex.new()
    for y = 0, size.y - 1 do
        for x = 0, size.x - 1 do
            local left = origin.x + x
            local top = origin.y + y
            local right = left + 1
            local bottom = top + 1
            local textureCoordinate = sf.Vector2f.new(x + 0.5, y + 0.5)
            appendVertex(vertices, vertex, left, top, textureCoordinate)
            appendVertex(vertices, vertex, right, top, textureCoordinate)
            appendVertex(vertices, vertex, right, bottom, textureCoordinate)
            appendVertex(vertices, vertex, left, top, textureCoordinate)
            appendVertex(vertices, vertex, right, bottom, textureCoordinate)
            appendVertex(vertices, vertex, left, bottom, textureCoordinate)
        end
    end
    return vertices
end

local function assertVerticesEqual(left, right)
    assert(left:getPrimitiveType() == right:getPrimitiveType())
    assert(left:getVertexCount() == right:getVertexCount())
    for index = 0, left:getVertexCount() - 1 do
        local leftVertex = left[index]
        local rightVertex = right[index]
        assert(leftVertex.position == rightVertex.position)
        assert(leftVertex.texCoords == rightVertex.texCoords)
        assert(leftVertex.color == rightVertex.color)
    end
end

local function benchmarkPixelGrid()
    local origin = sf.Vector2f.new(-13.0, 27.0)
    for _, sizeValue in ipairs({ 32, 64, 128 }) do
        local size = sf.Vector2u.new(sizeValue, sizeValue)
        local luaVertices = buildPixelGridVerticesLua(origin, size)
        local nativeVertices = Engine.BuildPixelGridVertices(origin, size)
        assert(luaVertices:getVertexCount() == sizeValue * sizeValue * 6)
        assertVerticesEqual(nativeVertices, luaVertices)
        local iterations = math.max(4, math.floor(32768 / (sizeValue * sizeValue)))
        benchmark("pixel_grid_lua_" .. sizeValue, iterations, function ()
            buildPixelGridVerticesLua(origin, size)
        end)
        benchmark("pixel_grid_native_" .. sizeValue, iterations * 100, function ()
            Engine.BuildPixelGridVertices(origin, size)
        end)
    end
end

local function benchmarkLighting()
    local map = setmetatable({}, { __index = Lighting })
    map._camera = nil
    map._lights = {}
    for index = 1, 8 do
        map._lights[index] = Light.new(
            sf.Vector2f.new(index * 48.0, index * 32.0), sf.Color.White, 96.0 + index, 1.0
        )
    end
    local actors = {}
    for index = 1, 2048 do
        actors[index] = { lightComp = nil }
    end
    function map:getAllActors()
        return actors
    end
    benchmark("lighting_active_filter_8_lights_2048_actors", 500, function ()
        map:_getActiveLights()
    end)

    local visibleActors = {}
    for index = 1, 512 do
        local actor = {}
        function actor:isDestroyed()
            return false
        end
        function actor:getLightBlock()
            return 1.0
        end
        function actor:getGlobalBounds()
            return {
                position = sf.Vector2f.new(10000.0 + index, 10000.0 + index),
                size = sf.Vector2f.new(32.0, 32.0)
            }
        end
        visibleActors[actor] = true
    end
    local activeLights = map:_getActiveLights()
    benchmark("lighting_occlusion_scan_8x512", 200, function ()
        assert(not map:_hasRelevantLightBlockingActors(activeLights, visibleActors))
    end)

    local occupancy = {}
    for y = 1, 128 do
        local row = {}
        for x = 1, 128 do
            row[x] = (x + y) % 7 == 0 and 1.0 or 0.0
        end
        occupancy[y] = row
    end
    function map:generateDataFromMap(_, rows)
        return rows
    end
    benchmark("lighting_occupancy_prefix_128x128", 100, function ()
        map:_rebuildStaticOccupancy(sf.Vector2u.new(128, 128), occupancy)
    end)
end

local function benchmarkTextLayout()
    Engine.DefaultFont = ManagerFunctions.loadFont("HarmonyOS_SansSC_Medium.ttf")
    Data.LoadTextConfigs()
    local plainText = string.rep("Ludork 长文本换行 benchmark with UTF-8 words and spaces. ", 20)
    local richText = string.rep("#Red#Ludork 红色#Red# and #Cyan#cyan 中文文本#Cyan# with spaces. ", 20)
    local plainMeasurements = 0
    local richMeasurements = 0
    local measurePlainText = TextLayout.MeasurePlainText
    local measureRichText = TextLayout.MeasureRichText
    TextLayout.MeasurePlainText = function (...)
        plainMeasurements = plainMeasurements + 1
        return measurePlainText(...)
    end
    TextLayout.MeasureRichText = function (...)
        richMeasurements = richMeasurements + 1
        return measureRichText(...)
    end
    benchmark("text_wrap_plain_utf8_1120_chars", 20, function ()
        TextLayout.WrapPlainText(plainText, 360.0, "UI/Text14")
    end)
    Logging.info("BENCHMARK_CALLS name=text_wrap_plain_measure count=%d", plainMeasurements)
    benchmark("text_wrap_rich_utf8_markers_1220_chars", 10, function ()
        TextLayout.WrapRichText(richText, 360.0, "UI/Message")
    end)
    Logging.info("BENCHMARK_CALLS name=text_wrap_rich_measure count=%d", richMeasurements)
end

local mode = arg[1] or "all"
if mode == "all" or mode == "pixel" then
    benchmarkPixelGrid()
end
if mode == "all" or mode == "lighting" then
    benchmarkLighting()
end
if mode == "all" or mode == "text" then
    benchmarkTextLayout()
end
