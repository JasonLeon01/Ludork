local Engine = require("Engine")
local Data = require("Source.Data")

local RichText = Engine.RichText

local TextLayout = {}
---@class Source.TextLayout.TextUnit
---@field value  string
---@field marker boolean

---@class Source.TextLayout.RichMeasurementEntry
---@field config      Engine.RichTextConfig
---@field control     Engine.RichText
---@field markerFlags table<string, boolean>

local richMeasurementControls = {}
---@cast richMeasurementControls table<string, Source.TextLayout.RichMeasurementEntry>

local function measurePlainText(control, text)
    local previous = control:getString()
    control:setString(tostring(text or ""))
    local width = control:getLocalBounds().size.x
    control:setString(previous)
    return width
end

local function getRichMeasurementEntry(textConfigKey)
    local config = Data.GetRichTextConfig(textConfigKey)
    local entry = richMeasurementControls[textConfigKey]
    if entry == nil or entry.config ~= config then
        entry = { config = config, control = RichText.new(config, ""), markerFlags = {} }
        richMeasurementControls[textConfigKey] = entry
    end
    return entry
end

---@param text string
---@return Source.TextLayout.TextUnit[]
local function textUnits(text)
    local units = {}
    for _, codepoint in utf8.codes(text) do
        units[#units + 1] = { value = utf8.char(codepoint), marker = false }
    end
    return units
end

---@param textConfigKey string
---@param text          string
---@return Source.TextLayout.TextUnit[]
local function richTextUnits(textConfigKey, text)
    local units = {}
    local entry = getRichMeasurementEntry(textConfigKey)
    local index = 1
    while index <= #text do
        if text:sub(index, index) == "#" then
            local markerEnd = text:find("#", index + 1, true)
            if markerEnd ~= nil then
                local marker = text:sub(index, markerEnd)
                local isMarker = entry.markerFlags[marker]
                if isMarker == nil then
                    entry.control:setString(marker)
                    isMarker = entry.control:getLocalBounds().size.x == 0.0
                    entry.markerFlags[marker] = isMarker
                end
                units[#units + 1] = { value = marker, marker = isMarker }
                index = markerEnd + 1
            else
                units[#units + 1] = { value = "#", marker = false }
                index = index + 1
            end
        else
            local nextIndex = utf8.offset(text, 2, index)
            if nextIndex == nil then
                nextIndex = #text + 1
            end
            units[#units + 1] = { value = text:sub(index, nextIndex - 1), marker = false }
            index = nextIndex
        end
    end
    return units
end

---@param units      Source.TextLayout.TextUnit[]
---@param firstIndex integer | nil
---@param lastIndex  integer | nil
---@return string
local function concatenateUnits(units, firstIndex, lastIndex)
    local values = {}
    local first = firstIndex or 1
    local last = lastIndex or #units
    for index = first, last do
        local unit = units[index]
        ---@cast unit Source.TextLayout.TextUnit
        values[#values + 1] = unit.value
    end
    return table.concat(values)
end

---@param units      Source.TextLayout.TextUnit[]
---@param firstIndex integer | nil
---@param lastIndex  integer | nil
---@return string
local function concatenateMarkers(units, firstIndex, lastIndex)
    local values = {}
    local first = firstIndex or 1
    local last = lastIndex or #units
    for index = first, last do
        local unit = units[index]
        ---@cast unit Source.TextLayout.TextUnit
        if unit.marker then
            values[#values + 1] = unit.value
        end
    end
    return table.concat(values)
end

---@param units     Source.TextLayout.TextUnit[]
---@param lastIndex integer | nil
---@return integer
local function visibleUnitCount(units, lastIndex)
    local count = 0
    for index = 1, lastIndex or #units do
        local unit = units[index]
        ---@cast unit Source.TextLayout.TextUnit
        if not unit.marker then
            count = count + 1
        end
    end
    return count
end

---@param units Source.TextLayout.TextUnit[]
---@return integer | nil
local function lastBreakableSpace(units)
    for index = #units, 1, -1 do
        local unit = units[index]
        if not unit.marker and unit.value == " " then
            return index
        end
    end
    return nil
end

---@param units Source.TextLayout.TextUnit[]
---@return Source.TextLayout.TextUnit[]
local function withoutLeadingSpaces(units)
    local result = {}
    local foundVisibleContent = false
    for _, unit in ipairs(units) do
        if unit.marker then
            result[#result + 1] = unit
        elseif unit.value ~= " " or foundVisibleContent then
            result[#result + 1] = unit
            foundVisibleContent = true
        end
    end
    return result
end

local function wrapMeasuredText(text, maxWidth, textConfigKey, measure, tokenize)
    if not bool(text) then
        return ""
    end
    assert(
        maxWidth == maxWidth and maxWidth > 0.0 and maxWidth < math.huge,
        "Text wrap width must be a positive finite number"
    )

    local result = {}
    local activeMarkers = ""
    for paragraph in (text .. "\n"):gmatch("(.-)\n") do
        local linePrefix = activeMarkers
        local lineUnits = {}
        for _, unit in ipairs(tokenize(textConfigKey, paragraph)) do
            lineUnits[#lineUnits + 1] = unit
            if not unit.marker then
                local lineWidth = measure(textConfigKey, linePrefix .. concatenateUnits(lineUnits))
                while lineWidth > maxWidth and visibleUnitCount(lineUnits) > 1 do
                    local spaceIndex = lastBreakableSpace(lineUnits)
                    if spaceIndex ~= nil then
                        local line = concatenateUnits(lineUnits, 1, spaceIndex - 1)
                        if visibleUnitCount(lineUnits, spaceIndex - 1) > 0 then
                            result[#result + 1] = line
                        end
                        linePrefix = linePrefix .. concatenateMarkers(lineUnits, 1, spaceIndex)
                        local remaining = {}
                        for index = spaceIndex + 1, #lineUnits do
                            remaining[#remaining + 1] = lineUnits[index]
                        end
                        lineUnits = withoutLeadingSpaces(remaining)
                    else
                        local currentUnit = lineUnits[#lineUnits]
                        ---@cast currentUnit Source.TextLayout.TextUnit
                        lineUnits[#lineUnits] = nil
                        result[#result + 1] = concatenateUnits(lineUnits)
                        linePrefix = linePrefix .. concatenateMarkers(lineUnits)
                        lineUnits = { currentUnit }
                    end
                    lineWidth = measure(textConfigKey, linePrefix .. concatenateUnits(lineUnits))
                end
            end
        end
        result[#result + 1] = concatenateUnits(lineUnits)
        activeMarkers = linePrefix .. concatenateMarkers(lineUnits)
    end
    return table.concat(result, "\n")
end

function TextLayout.MeasurePlainText(control, text)
    return measurePlainText(control, text)
end

function TextLayout.MeasureRichText(textConfigKey, text)
    local entry = getRichMeasurementEntry(textConfigKey)
    entry.control:setString(tostring(text or ""))
    return entry.control:getLocalBounds().size.x
end

function TextLayout.FitPlainText(text, maxWidth, control)
    if not bool(text) then
        return ""
    end
    local result = text
    while bool(result) and TextLayout.MeasurePlainText(control, result) > maxWidth do
        local offset = utf8.offset(result, -1)
        result = offset == nil and "" or result:sub(1, offset - 1)
    end
    if result ~= text and utf8.len(result) > 1 then
        local offset = utf8.offset(result, -1)
        result = offset == nil and "." or result:sub(1, offset - 1) .. "."
    end
    return result
end

function TextLayout.WrapPlainText(text, maxWidth, control)
    return wrapMeasuredText(text, maxWidth, control, TextLayout.MeasurePlainText, function (_, paragraph)
        return textUnits(paragraph)
    end)
end

function TextLayout.WrapRichText(text, maxWidth, textConfigKey)
    return wrapMeasuredText(text, maxWidth, textConfigKey, TextLayout.MeasureRichText, richTextUnits)
end

return TextLayout
