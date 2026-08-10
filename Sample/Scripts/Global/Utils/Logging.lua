local LEVEL_VALUES = { DEBUG = 10, INFO = 20, WARNING = 30, ERROR = 40 }

local Logging = {}
local currentLevel = LEVEL_VALUES.INFO

local function formatMessage(formatValue, ...)
    if select("#", ...) == 0 then
        return tostring(formatValue)
    end
    return string.format(tostring(formatValue), ...)
end

local function write(level, formatValue, ...)
    local message = formatMessage(formatValue, ...)
    if LEVEL_VALUES[level] < currentLevel then
        return
    end
    local written, writeError = io.stderr:write(level, ":", message, "\n")
    assert(written ~= nil, writeError)
    local flushed, flushError = io.stderr:flush()
    assert(flushed ~= nil, flushError)
end

function Logging.setLevel(level)
    local value = LEVEL_VALUES[level]
    assert(value ~= nil, "Invalid log level: " .. tostring(level))
    currentLevel = value
end

function Logging.debug(formatValue, ...)
    write("DEBUG", formatValue, ...)
end

function Logging.info(formatValue, ...)
    write("INFO", formatValue, ...)
end

function Logging.warning(formatValue, ...)
    write("WARNING", formatValue, ...)
end

function Logging.error(formatValue, ...)
    write("ERROR", formatValue, ...)
end

return Logging
