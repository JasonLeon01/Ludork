---@meta Source.NodeFunctions.Math

---@param x number
---@param y number
---@return sf.Vector2f
function Math.BuildVector2f(x, y) end

---@param x integer
---@param y integer
---@return sf.Vector2i
function Math.BuildVector2i(x, y) end

---@param x integer
---@param y integer
---@return sf.Vector2u
function Math.BuildVector2u(x, y) end

---@param x number
---@param y number
---@param z number
---@return sf.Vector3f
function Math.BuildVector3f(x, y, z) end

---@param x integer
---@param y integer
---@param z integer
---@return sf.Vector3i
function Math.BuildVector3i(x, y, z) end

---@param num     integer | number
---@param epsilon number
---@return boolean
function Math.IsNearZero(num, epsilon) end

---@param v       sf.Vector2f
---@param epsilon number
---@return boolean
function Math.IsVector2NearZero(v, epsilon) end

---@param v       sf.Vector3f
---@param epsilon number
---@return boolean
function Math.IsVector3NearZero(v, epsilon) end

---@param v sf.Vector2f
---@return sf.Vector2f
function Math.Vector2fRound(v) end

---@param v sf.Vector2f
---@return sf.Vector2f
function Math.Vector2fFloor(v) end

---@param v sf.Vector2f
---@return sf.Vector2f
function Math.Vector2fCeil(v) end

---@param v sf.Vector2i | sf.Vector2u
---@return sf.Vector2f
function Math.ToVector2f(v) end

---@param v sf.Vector2f | sf.Vector2u
---@return sf.Vector2i
function Math.ToVector2i(v) end

---@param v sf.Vector2f | sf.Vector2i
---@return sf.Vector2u
function Math.ToVector2u(v) end

---@param v sf.Vector3i
---@return sf.Vector3f
function Math.ToVector3f(v) end

---@param v sf.Vector3f
---@return sf.Vector3i
function Math.ToVector3i(v) end

---@param x      integer
---@param y      integer
---@param width  integer
---@param height integer
---@return sf.IntRect
function Math.ToIntRect(x, y, width, height) end

---@param x      number
---@param y      number
---@param width  number
---@param height number
---@return sf.FloatRect
function Math.ToFloatRect(x, y, width, height) end

---@param value   number
---@param min_val number
---@param max_val number
---@return number
function Math.Clamp(value, min_val, max_val) end

---@param a number
---@param b number
---@param t number
---@return number
function Math.Lerp(a, b, t) end

---@param value integer | number
---@return integer | number
function Math.Abs(value) end

---@param value integer | number
---@return integer
function Math.ToInt(value) end

---@param value integer | number
---@return number
function Math.ToFloat(value) end

---@generic T
---@param values T[]
---@return T
function Math.Max(values) end

---@generic T
---@param values T[]
---@return T
function Math.Min(values) end

---@param value integer | number
---@return integer | number
function Math.Sqrt(value) end

---@param base integer | number
---@param exp  integer | number
---@return integer | number
function Math.Pow(base, exp) end

---@param v1 sf.Vector2f | sf.Vector2i | sf.Vector2u
---@param v2 sf.Vector2f | sf.Vector2i | sf.Vector2u
---@return number
function Math.Vector2Distance(v1, v2) end

---@param v1 sf.Vector3f
---@param v2 sf.Vector3f
---@return number
function Math.Vector3Distance(v1, v2) end

---@param v1 sf.Vector2f | sf.Vector2i | sf.Vector2u
---@param v2 sf.Vector2f | sf.Vector2i | sf.Vector2u
---@return number
function Math.Vector2Dot(v1, v2) end

---@param v1 sf.Vector3f | sf.Vector3i
---@param v2 sf.Vector3f | sf.Vector3i
---@return number
function Math.Vector3Dot(v1, v2) end

---@param v1 sf.Vector2f | sf.Vector2i | sf.Vector2u
---@param v2 sf.Vector2f | sf.Vector2i | sf.Vector2u
---@return number | integer
function Math.Vector2Cross(v1, v2) end

---@param v1 sf.Vector3f | sf.Vector3i
---@param v2 sf.Vector3f | sf.Vector3i
---@return sf.Vector3f | sf.Vector3i
function Math.Vector3Cross(v1, v2) end

---@param v sf.Vector2f
---@return number
function Math.Vector2Length(v) end

---@param v sf.Vector3f
---@return number
function Math.Vector3Length(v) end

---@param v sf.Vector2f | sf.Vector2i | sf.Vector2u
---@return number | integer
function Math.Vector2LengthSquared(v) end

---@param v sf.Vector3f | sf.Vector3i
---@return number | integer
function Math.Vector3LengthSquared(v) end

---@param v sf.Vector2f
---@return sf.Vector2f
function Math.Vector2Normalized(v) end

---@param v sf.Vector3f
---@return sf.Vector3f
function Math.Vector3Normalized(v) end

---@param v sf.Vector2f
---@return sf.Angle
function Math.GetAngle(v) end

---@param v1 sf.Vector2f
---@param v2 sf.Vector2f
---@return sf.Angle
function Math.GetAngleTo(v1, v2) end

---@param angle sf.Angle
---@return number
function Math.AsDegrees(angle) end

---@param angle sf.Angle
---@return number
function Math.AsRadians(angle) end

---@param v   sf.Vector2f | sf.Vector2i | sf.Vector2u | sf.Vector3f | sf.Vector3i
---@param div sf.Vector2f | sf.Vector2i | sf.Vector2u | sf.Vector3f | sf.Vector3i
---@return sf.Vector2f | sf.Vector2i | sf.Vector2u | sf.Vector3f | sf.Vector3i
function Math.Vector2ComponentWiseDiv(v, div) end

---@param v   sf.Vector2f | sf.Vector2i | sf.Vector2u | sf.Vector3f | sf.Vector3i
---@param mul sf.Vector2f | sf.Vector2i | sf.Vector2u | sf.Vector3f | sf.Vector3i
---@return sf.Vector2f | sf.Vector2i | sf.Vector2u | sf.Vector3f | sf.Vector3i
function Math.Vector2ComponentWiseMul(v, mul) end

---@param v sf.Vector2f | sf.Vector2i | sf.Vector2u
---@return sf.Vector2f | sf.Vector2i | sf.Vector2u
function Math.Vector2Perpendicular(v) end

---@param v    sf.Vector2f
---@param axis sf.Vector2f
---@return sf.Vector2f
function Math.Vector2ProjectedOnto(v, axis) end

---@param v   sf.Vector2f
---@param phi sf.Angle
---@return sf.Vector2f
function Math.Vector2RotatedBy(v, phi) end

---@param degrees_ number
---@return sf.Angle
function Math.DegreesToAngle(degrees_) end

---@param radians_ number
---@return sf.Angle
function Math.RadiansToAngle(radians_) end

---@param min_val integer
---@param max_val integer
---@return integer
function Math.RandomInt(min_val, max_val) end

---@param min_val number
---@param max_val number
---@return number
function Math.RandomFloat(min_val, max_val) end

---@param a integer
---@param b integer
---@return integer
function Math.GCD(a, b) end

---@param a integer
---@param b integer
---@return integer
function Math.LCM(a, b) end

---@param a any
---@param b any
---@return any
function Math.ADD(a, b) end

---@param a any
---@param b any
---@return any
function Math.SUB(a, b) end

---@param a any
---@param b any
---@return any
function Math.MUL(a, b) end

---@param a any
---@param b any
---@return any
function Math.DIV(a, b) end

---@param a any
---@param b any
---@return any
function Math.MOD(a, b) end

---@param a any
---@param b any
---@return any
function Math.POW(a, b) end

---@param a any
---@param b any
---@return boolean
function Math.EQUALS(a, b) end

---@param a any
---@param b any
---@return boolean
function Math.NOT_EQUALS(a, b) end

---@param a any
---@param b any
---@return boolean
function Math.LESS(a, b) end

---@param a any
---@param b any
---@return boolean
function Math.LESS_EQUALS(a, b) end

---@param a any
---@param b any
---@return boolean
function Math.GREATER(a, b) end

---@param a any
---@param b any
---@return boolean
function Math.GREATER_EQUALS(a, b) end

---@param a boolean
---@param b boolean
---@return boolean
function Math.AND(a, b) end

---@param a boolean
---@param b boolean
---@return boolean
function Math.OR(a, b) end

---@param a boolean
---@return boolean
function Math.NOT(a) end

---@param a boolean
---@param b boolean
---@return boolean
function Math.XOR(a, b) end

---@param a boolean
---@param b boolean
---@return boolean
function Math.NAND(a, b) end

---@param a boolean
---@param b boolean
---@return boolean
function Math.NOR(a, b) end

---@param a boolean
---@param b boolean
---@return boolean
function Math.XNOR(a, b) end

---@param a any
---@param b any
function Math.IADD(a, b) end

---@param a any
---@param b any
function Math.ISUB(a, b) end

---@param a any
---@param b any
function Math.IMUL(a, b) end

---@param a any
---@param b any
function Math.IDIV(a, b) end

---@param a any
---@param b any
function Math.IMOD(a, b) end

---@param a any
---@param b any
function Math.IPOW(a, b) end

return Math
