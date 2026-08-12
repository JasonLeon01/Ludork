local Engine = require("Engine")
local Utils = require("Source.NodeFunctions.Utils")

local Node = Engine.Node

local Math = {}

---@param fn        function
---@param a         any
---@param b         any
---@param operation fun(left: any, right: any): any
local function updateInPlace(fn, a, b, operation)
    local refLocal = Node.getRefLocal(fn)
    if type(a) == "string" and refLocal ~= nil and refLocal[a] ~= nil then
        refLocal[a] = operation(refLocal[a], b)
        return
    end
    if Utils.IsNodeReference(a) then
        a:set(operation(a:get(), b))
        return
    end
    operation(a, b)
end

function Math.BuildVector2f(x, y)
    x = x == nil and 0.0 or x
    y = y == nil and 0.0 or y
    local vectorX = tonumber(x)
    local vectorY = tonumber(y)
    ---@cast vectorX number
    ---@cast vectorY number
    return sf.Vector2f.new(vectorX, vectorY)
end

function Math.BuildVector2i(x, y)
    x = x == nil and 0 or x
    y = y == nil and 0 or y
    return sf.Vector2i.new(Engine.ToInteger(x), Engine.ToInteger(y))
end

function Math.BuildVector2u(x, y)
    x = x == nil and 0 or x
    y = y == nil and 0 or y
    return sf.Vector2u.new(Engine.ToInteger(x), Engine.ToInteger(y))
end

function Math.BuildVector3f(x, y, z)
    x = x == nil and 0.0 or x
    y = y == nil and 0.0 or y
    z = z == nil and 0.0 or z
    local vectorX = tonumber(x)
    local vectorY = tonumber(y)
    local vectorZ = tonumber(z)
    ---@cast vectorX number
    ---@cast vectorY number
    ---@cast vectorZ number
    return sf.Vector3f.new(vectorX, vectorY, vectorZ)
end

function Math.BuildVector3i(x, y, z)
    x = x == nil and 0 or x
    y = y == nil and 0 or y
    z = z == nil and 0 or z
    return sf.Vector3i.new(Engine.ToInteger(x), Engine.ToInteger(y), Engine.ToInteger(z))
end

function Math.IsNearZero(num, epsilon)
    epsilon = epsilon == nil and 0.1 or epsilon
    return Engine.IsNearZero(num, epsilon)
end

function Math.IsVector2NearZero(v, epsilon)
    epsilon = epsilon == nil and 0.1 or epsilon
    return Engine.IsVector2NearZero(v, epsilon)
end

function Math.IsVector3NearZero(v, epsilon)
    epsilon = epsilon == nil and 0.1 or epsilon
    return Engine.IsVector3NearZero(v, epsilon)
end

function Math.Vector2fRound(v)
    return Engine.Vector2fRound(v)
end

function Math.Vector2fFloor(v)
    return Engine.Vector2fFloor(v)
end

function Math.Vector2fCeil(v)
    return Engine.Vector2fCeil(v)
end

function Math.ToVector2f(v)
    return Engine.ToVector2f(v)
end

function Math.ToVector2i(v)
    return Engine.ToVector2i(v)
end

function Math.ToVector2u(v)
    return Engine.ToVector2u(v)
end

function Math.ToVector3f(v)
    return Engine.ToVector3f(v)
end

function Math.ToVector3i(v)
    return Engine.ToVector3i(v)
end

function Math.ToIntRect(x, y, width, height)
    x = x == nil and 0 or x
    y = y == nil and 0 or y
    width = width == nil and 32 or width
    height = height == nil and 32 or height
    return Engine.ToIntRect(x, y, width, height)
end

function Math.ToFloatRect(x, y, width, height)
    x = x == nil and 0.0 or x
    y = y == nil and 0.0 or y
    width = width == nil and 32.0 or width
    height = height == nil and 32.0 or height
    return Engine.ToFloatRect(x, y, width, height)
end

function Math.Clamp(value, min_val, max_val)
    value = value == nil and 0.0 or value
    min_val = min_val == nil and 0.0 or min_val
    max_val = max_val == nil and 1.0 or max_val
    return Engine.Clamp(value, min_val, max_val)
end

function Math.Lerp(a, b, t)
    a = a == nil and 0.0 or a
    b = b == nil and 1.0 or b
    t = t == nil and 0.5 or t
    return Engine.Lerp(a, b, t)
end

function Math.Abs(value)
    value = value == nil and 0 or value
    return math.abs(value)
end

function Math.ToInt(value)
    value = value == nil and 0 or value
    return Engine.ToInteger(value)
end

function Math.ToFloat(value)
    value = value == nil and 0 or value
    local result = tonumber(value)
    ---@cast result number
    return result
end

function Math.Max(values)
    values = values or {}
    if not bool(values) then
        error("max() arg is an empty sequence")
    end
    local result = values[1]
    for index = 2, #values do
        if values[index] > result then
            result = values[index]
        end
    end
    return result
end

function Math.Min(values)
    values = values or {}
    if not bool(values) then
        error("min() arg is an empty sequence")
    end
    local result = values[1]
    for index = 2, #values do
        if values[index] < result then
            result = values[index]
        end
    end
    return result
end

function Math.Sqrt(value)
    value = value == nil and 0 or value
    return math.sqrt(value)
end

function Math.Pow(base, exp)
    base = base == nil and 1 or base
    exp = exp == nil and 2 or exp
    return base ^ exp
end

function Math.Vector2Distance(v1, v2)
    return math.sqrt((v1.x - v2.x) ^ 2 + (v1.y - v2.y) ^ 2)
end

function Math.Vector3Distance(v1, v2)
    return math.sqrt((v1.x - v2.x) ^ 2 + (v1.y - v2.y) ^ 2 + (v1.z - v2.z) ^ 2)
end

function Math.Vector2Dot(v1, v2)
    return v1:dot(v2)
end

function Math.Vector3Dot(v1, v2)
    return v1:dot(v2)
end

function Math.Vector2Cross(v1, v2)
    return v1:cross(v2)
end

function Math.Vector3Cross(v1, v2)
    return v1:cross(v2)
end

function Math.Vector2Length(v)
    return v:length()
end

function Math.Vector3Length(v)
    return v:length()
end

function Math.Vector2LengthSquared(v)
    return v:lengthSquared()
end

function Math.Vector3LengthSquared(v)
    return v:lengthSquared()
end

function Math.Vector2Normalized(v)
    return v:normalized()
end

function Math.Vector3Normalized(v)
    return v:normalized()
end

function Math.GetAngle(v)
    return v:angle()
end

function Math.GetAngleTo(v1, v2)
    return v1:angleTo(v2)
end

function Math.AsDegrees(angle)
    return angle:asDegrees()
end

function Math.AsRadians(angle)
    return angle:asRadians()
end

function Math.Vector2ComponentWiseDiv(v, div)
    return v:componentWiseDiv(div)
end

function Math.Vector2ComponentWiseMul(v, mul)
    return v:componentWiseMul(mul)
end

function Math.Vector2Perpendicular(v)
    return v:perpendicular()
end

function Math.Vector2ProjectedOnto(v, axis)
    return v:projectedOnto(axis)
end

function Math.Vector2RotatedBy(v, phi)
    return v:rotatedBy(phi)
end

function Math.DegreesToAngle(degrees_)
    degrees_ = degrees_ == nil and 0.0 or degrees_
    return sf.degrees(degrees_)
end

function Math.RadiansToAngle(radians_)
    radians_ = radians_ == nil and 0.0 or radians_
    return sf.radians(radians_)
end

function Math.RandomInt(min_val, max_val)
    min_val = min_val == nil and 0 or min_val
    max_val = max_val == nil and 100 or max_val
    return math.random(Engine.ToInteger(min_val), Engine.ToInteger(max_val))
end

function Math.RandomFloat(min_val, max_val)
    min_val = min_val == nil and 0.0 or min_val
    max_val = max_val == nil and 1.0 or max_val
    return min_val + (max_val - min_val) * math.random()
end

function Math.GCD(a, b)
    a = a == nil and 1 or a
    b = b == nil and 1 or b
    return Engine.GCD(a, b)
end

function Math.LCM(a, b)
    a = a == nil and 1 or a
    b = b == nil and 1 or b
    return Engine.LCM(a, b)
end

function Math.ADD(a, b)
    return a + b
end

function Math.SUB(a, b)
    return a - b
end

function Math.MUL(a, b)
    return a * b
end

function Math.DIV(a, b)
    return a / b
end

function Math.MOD(a, b)
    return a % b
end

function Math.POW(a, b)
    return a ^ b
end

function Math.EQUALS(a, b)
    return a == b
end

function Math.NOT_EQUALS(a, b)
    return a ~= b
end

function Math.LESS(a, b)
    return a < b
end

function Math.LESS_EQUALS(a, b)
    return a <= b
end

function Math.GREATER(a, b)
    return a > b
end

function Math.GREATER_EQUALS(a, b)
    return a >= b
end

function Math.AND(a, b)
    return bool(a) and bool(b)
end

function Math.OR(a, b)
    return bool(a) or bool(b)
end

function Math.NOT(a)
    return not bool(a)
end

function Math.XOR(a, b)
    return bool(a) ~= bool(b)
end

function Math.NAND(a, b)
    return not (bool(a) and bool(b))
end

function Math.NOR(a, b)
    return not (bool(a) or bool(b))
end

function Math.XNOR(a, b)
    return bool(a) == bool(b)
end

function Math.IADD(a, b)
    updateInPlace(Math.IADD, a, b, function (left, right)
        return left + right
    end)
end

function Math.ISUB(a, b)
    updateInPlace(Math.ISUB, a, b, function (left, right)
        return left - right
    end)
end

function Math.IMUL(a, b)
    updateInPlace(Math.IMUL, a, b, function (left, right)
        return left * right
    end)
end

function Math.IDIV(a, b)
    updateInPlace(Math.IDIV, a, b, function (left, right)
        return left / right
    end)
end

function Math.IMOD(a, b)
    updateInPlace(Math.IMOD, a, b, function (left, right)
        return left % right
    end)
end

function Math.IPOW(a, b)
    updateInPlace(Math.IPOW, a, b, function (left, right)
        return left ^ right
    end)
end

return Math
