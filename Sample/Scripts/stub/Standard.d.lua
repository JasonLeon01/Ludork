---@meta

---@type string
PLATFORM = PLATFORM or ""

---@type boolean
LUDORK_MOBILE = LUDORK_MOBILE or false

---@type boolean
LUDORK_DESKTOP = LUDORK_DESKTOP or false

--- Whether standard save slots use encrypted LDC files instead of plain JSON.
---@type boolean
SAVE_AS_LDC = SAVE_AS_LDC or false

---@generic T
---@param value? T
---@return boolean
function bool(value) end

--- Shallow-copy a Lua table or supported native value.
--- Lua table metatables are preserved. Native resources without an explicit
--- value-copy strategy retain their identity.
--- Monitored tables copy their current logical fields and original metatable, without subscriptions.
---@generic T
---@param value T
---@return T
function copy(value) end

--- Recursively copy Lua table keys and values, preserving aliases and cycles.
--- Supported native values are copied; native resources without an explicit
--- value-copy strategy retain their identity.
--- Monitored tables copy their current logical fields and original metatable, without subscriptions.
---@generic T
---@param value T
---@return T
function deepcopy(value) end

---@class list<T>: { [integer]: T }
---@operator len: integer
---@operator eq(list<T>): boolean
---@operator add(list<T>): list<T>
---@operator mul(integer): list<T>
local list_value = {}

--- Append one value, including nil, to the end of the list.
---@param value T
function list_value:append(value) end

--- Append every value from another sequence.
---@param values table<integer, T> | list<T> | tuple<T>
function list_value:extend(values) end

--- Insert a value at a 1-based index.
---@param index integer
---@param value T
function list_value:insert(index, value) end

--- Remove and return a value. The last value is used when index is omitted.
---@param index? integer
---@return T
function list_value:pop(index) end

--- Remove the first equal value, raising an error when it is absent.
---@param value T
function list_value:remove(value) end

--- Remove every value from the list.
function list_value:clear() end

--- Return the 1-based index of the first equal value.
---@param value T
---@return integer
function list_value:index(value) end

--- Count values equal to the requested value.
---@param value T
---@return integer
function list_value:count(value) end

--- Return whether the list contains an equal value.
---@param value T
---@return boolean
function list_value:contains(value) end

--- Reverse the list in place.
function list_value:reverse() end

--- Sort the list in place with an optional less-than callback.
---@param comparator? fun(left: T, right: T): boolean
function list_value:sort(comparator) end

--- Return a shallow native-list copy.
---@return list<T>
function list_value:copy() end

--- Return every list slot as multiple values, preserving nil slots.
---@return T ...
function list_value:unpack() end

--- Recursively convert native containers to Lua tables.
--- Native nil slots become cjson.null so their positions are retained.
---@return table<integer, any>
function list_value:toTable() end

---@class tuple<T>: { [integer]: T }
---@operator len: integer
---@operator eq(tuple<T>): boolean
---@operator add(tuple<T>): tuple<T>
---@operator mul(integer): tuple<T>
local tuple_value = {}

--- Return the 1-based index of the first equal value.
---@param value T
---@return integer
function tuple_value:index(value) end

--- Count values equal to the requested value.
---@param value T
---@return integer
function tuple_value:count(value) end

--- Return whether the tuple contains an equal value.
---@param value T
---@return boolean
function tuple_value:contains(value) end

--- Return every tuple slot as multiple values.
---@return T ...
function tuple_value:unpack() end

--- Recursively convert native containers to Lua tables.
---@return table<integer, any>
function tuple_value:toTable() end

---@class dict<K, V>: { [K]: V }
---@operator len: integer
---@operator eq(dict<K, V>): boolean
local dict_value = {}

--- Return a stored value or the optional default when the key is absent.
---@generic D
---@param key      K
---@param default? D
---@return V | D | nil
function dict_value:get(key, default) end

--- Return an existing value, or store and return the supplied default.
---@param key      K
---@param default? V
---@return V | nil
function dict_value:setdefault(key, default) end

--- Merge values from another mapping into this dictionary.
---@param values table<K, V> | dict<K, V>
function dict_value:update(values) end

--- Remove and return a value. A missing key raises unless default is supplied.
---@generic D
---@param key      K
---@param default? D
---@return V | D | nil
function dict_value:pop(key, default) end

--- Remove a key and report whether it existed.
---@param key K
---@return boolean
function dict_value:remove(key) end

--- Remove every entry from the dictionary.
function dict_value:clear() end

--- Return whether the dictionary contains a key, including one mapped to nil.
---@param key K
---@return boolean
function dict_value:contains(key) end

--- Return keys in insertion order as a native list.
---@return list<K>
function dict_value:keys() end

--- Return values in insertion order as a native list.
---@return list<V>
function dict_value:values() end

--- Return a lazy insertion-ordered key/value iterator.
--- Use it directly in `for key, value in dictionary:items() do`.
---@return fun(state: any, control: any): K?, V? iterator
---@return nil state
---@return nil initial
function dict_value:items() end

--- Return a shallow native-dictionary copy.
---@return dict<K, V>
function dict_value:copy() end

--- Recursively convert native containers to a Lua table.
--- Native nil values become cjson.null so their keys are retained.
---@return table<any, any>
function dict_value:toTable() end

--- Construct a mutable native list from values.
--- One raw table, list or tuple argument is shallow-copied as a sequence.
---@class list.Type
---@overload fun<T>(...: T): list<T>
---@overload fun<T>(values: list<T>): list<T>
---@overload fun<T>(values: tuple<T>): list<T>
---@overload fun<T>(values?: T[]): list<T>
list = {}

--- Construct an immutable native tuple from values.
--- One raw table, list or tuple argument is shallow-copied as a sequence.
--- Tuple elements cannot be nil.
---@class tuple.Type
---@overload fun<T>(...: T): tuple<T>
---@overload fun<T>(values: list<T>): tuple<T>
---@overload fun<T>(values: tuple<T>): tuple<T>
---@overload fun<T>(values?: T[]): tuple<T>
tuple = {}

--- Construct a mutable native dictionary from an optional mapping.
---@class dict.Type
---@overload fun<K, V>(values?: table<K, V> | dict<K, V>): dict<K, V>
dict = {}

---@param format string
---@param ...    any
---@return string
function string.pformat(format, ...) end

---@param value  string
---@param target string
---@return boolean
function string.contains(value, target) end

---@param value  string
---@param prefix string
---@return boolean
function string.startsWith(value, prefix) end

---@param value  string
---@param suffix string
---@return boolean
function string.endsWith(value, suffix) end

---@param value string
---@return boolean
function string.isEmpty(value) end

---@param value string
---@return boolean
function string.isBlank(value) end

---@param value string
---@return string
function string.strip(value) end

---@param value string
---@return string
function string.stripLeading(value) end

---@param value string
---@return string
function string.stripTrailing(value) end

---@param value       string
---@param target      string
---@param replacement string
---@return string
function string.replace(value, target, replacement) end

---@param value     string
---@param separator string
---@return string[]
function string.split(value, separator) end

---@param value string
---@return integer
function string.utf8Length(value) end

---@param value  string
---@param start  integer
---@param finish integer
---@return string
function string.utf8Slice(value, start, finish) end

---@generic T
---@param values   T[]
---@param expected T
---@return boolean
function table.contains(values, expected) end

---@generic T
---@param values   T[]
---@param expected T
---@return integer | nil
function table.index(values, expected) end

---@param values          table<string, any>
---@param preferredOrder? string[]
---@return string[]
function table.orderedStringKeys(values, preferredOrder) end

--- Return true only for finite Lua numbers, without converting numeric strings.
---@param value any
---@return boolean
function math.isFinite(value) end

--- Math extensions require finite numbers without implicit string conversion.
--- Invalid bounds, numeric overflow and integer results outside the Lua integer
--- range raise errors.
--- Clamp to a closed interval; minimum must not exceed maximum.
---@param value   number
---@param minimum number
---@param maximum number
---@return number
function math.clamp(value, minimum, maximum) end

--- Linearly interpolate or extrapolate without clamping alpha.
---@param from  number
---@param to    number
---@param alpha number
---@return number
function math.lerp(from, to, alpha) end

--- Round to the nearest integer, choosing the even integer at exact halves.
--- Lua integer inputs retain their exact value.
---@param value number
---@return integer
function math.round(value) end

--- Truncate towards zero; Lua integer inputs retain their exact value.
--- Unlike math.tointeger, fractional numbers are accepted. Strings are not.
---@param value number
---@return integer
function math.trunc(value) end

--- Test abs(value) < epsilon; epsilon defaults to 0.1 and must be non-negative.
---@param value    number
---@param epsilon? number
---@return boolean
function math.isNearZero(value, epsilon) end

--- Return the non-negative greatest common divisor; gcd(0, 0) is zero.
--- Arguments must be Lua integers.
---@param left  integer
---@param right integer
---@return integer
function math.gcd(left, right) end

--- Return the non-negative least common multiple, or zero if either input is zero.
--- Arguments must be Lua integers.
---@param left  integer
---@param right integer
---@return integer
function math.lcm(left, right) end

--- Return -1, 0 or 1; both signed zeros return zero.
---@param value number
---@return integer
function math.sign(value) end

--- Return (value - a) / (b - a), without clamping; endpoints must differ.
---@param a     number
---@param b     number
---@param value number
---@return number
function math.inverseLerp(a, b, value) end

--- Map between ranges without clamping. Input endpoints must differ;
--- either range may run in reverse.
---@param value  number
---@param inMin  number
---@param inMax  number
---@param outMin number
---@param outMax number
---@return number
function math.remap(value, inMin, inMax, outMin, outMax) end

--- Return t*t*(3-2*t), with t normalised and clamped to [0, 1].
--- edge0 must be less than edge1.
---@param edge0 number
---@param edge1 number
---@param value number
---@return number
function math.smoothstep(edge0, edge1, value) end

--- Move at most maxDelta towards target without passing it; maxDelta must be non-negative.
---@param current  number
---@param target   number
---@param maxDelta number
---@return number
function math.moveTowards(current, target, maxDelta) end

---@return number
function perfCounter() end

---@class ConfigParser
local ConfigParser = {}

---@param path string
---@return boolean
function ConfigParser:read(path) end

---@param section string
---@return boolean
function ConfigParser:has_section(section) end

---@param section string
function ConfigParser:add_section(section) end

---@generic T
---@param section   string
---@param key       string
---@param fallback? T
---@return string | T | nil
function ConfigParser:get(section, key, fallback) end

---@generic T
---@param section   string
---@param key       string
---@param fallback? T
---@return number | T | nil
function ConfigParser:getfloat(section, key, fallback) end

---@generic T
---@param section   string
---@param key       string
---@param fallback? T
---@return integer | T | nil
function ConfigParser:getint(section, key, fallback) end

---@generic T
---@param section   string
---@param key       string
---@param fallback? T
---@return boolean | T | nil
function ConfigParser:getboolean(section, key, fallback) end

---@param section string
---@param key     string
---@param value   string | number
function ConfigParser:set(section, key, value) end

---@param path string
function ConfigParser:write(path) end

configparser = {}

---@return ConfigParser
function configparser.ConfigParser() end

locale = {}

---@return string, string
function locale.getdefaultlocale() end

base64 = {}

---@param value string
---@return string
function base64.encode(value) end

---@param value string
---@return string
function base64.decode(value) end

zlib = {}

---@param value string
---@return string
function zlib.compress(value) end

---@param value string
---@return string
function zlib.decompress(value) end

os.path = {}

--- `os.listdir`, `os.path.isdir`, `os.path.isfile`, and `io.open` address only
--- the real filesystem and do not resolve `/Game/Assets/...` or packed Data.
--- `os.path.getmtime` also accepts a packed `Data/...` logical path and returns
--- its containing archive's modification time.
---@return string
function os.getcwd() end

---@param path string
function os.createDirectories(path) end

---@param path string
function os.removeFile(path) end

---@param path string
---@return string[]
function os.listdir(path) end

---@param ... string
---@return string
function os.path.join(...) end

---@param path string
---@return string, string
function os.path.splitext(path) end
---@param path string
---@return string
function os.path.basename(path) end

---@param path string
---@return string
function os.path.dirname(path) end

---@param path string
---@return string
function os.path.abspath(path) end

---@param path string
---@return boolean
function os.path.isdir(path) end

---@param path string
---@return boolean
function os.path.isfile(path) end

---@param path string
---@return number
function os.path.getmtime(path) end

asyncio = {}

---@class AsyncioTask

---@param callback fun(...: any)
---@param ...      any
---@return AsyncioTask
function asyncio.create_task(callback, ...) end
---@param task AsyncioTask
---@return boolean
function asyncio.cancel_task(task) end

---@param seconds number
function asyncio.sleep(seconds) end

---@class FileBatchSpec
---@field category       string
---@field root           string
---@field suffix?        string
---@field excludeSuffix? string
---@field recursive?     boolean
---@field required?      boolean
---@field parseJson?     boolean

---@class FileBatchItem
---@field index         integer
---@field category      string
---@field relativePath  string
---@field content?      string
---@field conversion?   FileBatchJsonConversion
---@field contentBytes? integer
---@field encryptedData boolean

---@class FileBatchJsonConversion

---@class FileBatchJob

---@class FileBatchError
---@field operation "scan" | "open" | "read" | "parse"
---@field category  string
---@field path      string
---@field code      integer
---@field message   string

---@class FileBatchSnapshot
---@field state     "scanning" | "running" | "completed" | "cancelling" | "cancelled" | "failed"
---@field total     integer
---@field completed integer
---@field delivered integer
---@field drained   boolean
---@field items     FileBatchItem[]
---@field error?    FileBatchError

---@param specs FileBatchSpec[]
---@return FileBatchJob
function asyncio.start_file_batch(specs) end

---@param job       FileBatchJob
---@param maxItems? integer
---@return FileBatchSnapshot
function asyncio.poll_file_batch(job, maxItems) end

---@param job FileBatchJob
---@return boolean
function asyncio.cancel_file_batch(job) end

---@param conversion      FileBatchJsonConversion
---@param maxNodes        integer
---@param maxMilliseconds number
---@return boolean completed
---@return integer processed
---@return any data
function asyncio.step_file_batch_json(conversion, maxNodes, maxMilliseconds) end

---@param conversion FileBatchJsonConversion
---@return boolean
function asyncio.clear_file_batch_json(conversion) end

---@generic T
---@param value T
---@return integer
function asizeof(value) end

---@return number
function processMemoryMB() end
