# Ludork Lua Advanced

Ludork's Standard library installs native classes, containers, math and string helpers, configuration parsers and cooperative async APIs before `Scripts/Entry.lua` runs, without `require`. Its declarations live in `Scripts/stub/Standard.d.lua` and `Scripts/stub/Class.d.lua`; `Engine`, `GlobalCore`, `GlobalFunctions` and LuaSF have separate bindings.

## Ludork classes

Create a plain definition table without a metatable, add defaults and methods, then return `class(definition, ...)`. The definition must not already be finalised, every base must be a finalised Ludork class or a registered native type and duplicate bases are rejected. Base order determines the C3 method-resolution order (MRO) used to search inherited members.

```lua
local Engine = require("Engine")

local Actor = Engine.Actor

local Pickup = {}

Pickup.count = 1
Pickup.sound = ""

function Pickup:init(texture, tag)
    Actor.init(self, texture, tag)
    self._collected = false
end

return class(Pickup, Actor)
```

### Defaults and instance state

Editable or inheritable defaults belong on the definition table before finalisation, including metadata fields, component defaults, `default*` spawn values and Blueprint-overridable identifiers.

`init` applies constructor arguments and creates per-instance state. Copy mutable class defaults whenever each instance must own an independent value:

```lua
local RouteFollower = {}

RouteFollower.speed = 96.0
RouteFollower.defaultRoute = {}

function RouteFollower:init()
    self._route = deepcopy(self.defaultRoute)
    self._routeIndex = 1
end

return class(RouteFollower)
```

> **Important:** Do not initialise a public default only inside `init`. Subclasses and Blueprint discovery read defaults from the class table.

An explicit `nil` restored from Blueprint, Component or General Data is stored as an own field that overrides inherited defaults; inheritance, `copy` and `deepcopy` preserve it. Assigning `nil` in Lua clears that override and resumes inheritance lookup. Native properties and Lua getters and setters follow their normal lookup and dispatch rules.

### Construction, lifecycle and inheritance

- **Construction:** `Type.new(...)` constructs an instance and calls `init`. Finalised classes are also callable; follow the convention of neighbouring modules. A class without `init` accepts no arguments unless a single native root defines its constructor contract. Invoke required native-base initialisers explicitly.
- **Native property deferral:** Before a native root is constructed, typed assignments to its properties are stored on the composite and applied during native-base initialisation. Native property access targets that root; script getters and setters resolve from script classes, and setter errors propagate to the caller.
- **Lifecycle and disposal:** Lifecycle methods such as `onCreate`, `onTick` and `onDestroy` run only when their owning runtime contract calls them. `instance:dispose()` runs the resolved implementation at most once, releases native ownership and instance fields, and is not repeated by garbage collection.
- **Super calls and multiple inheritance:** Inside a method, `super()` resolves the next implementation from the defining class; use `super(ClassName, self)` when an indirect callback makes ownership ambiguous. Multiple inheritance is supported, and declared base order must stay stable.
- **Script Mixins:** [Script Mixins](<../Lua and Blueprint Scripting/Script Mixins/Runtime Contract.md>) use lifecycle callbacks instead of `init`.

### Class utilities

| API | Purpose |
|---|---|
| `Class.isInstance(value, target)` | Test a raw Lua type or an instance against a class's complete MRO. |
| `Class.isSubclass(value, target)` | Test a class against its complete MRO. |
| `Class.type(value)` | Return its Ludork class or native fallback type. |
| `Class.hasOwnField(value, key)` | Test whether a field is stored directly. |
| `Class.getMro(value)` | Return a detached MRO array. |
| `Class.getParameterNames(callable)` | Return declared parameter names without `self`. |
| `Class.constructNamed(type, values)` | Match named values to `init` parameters and construct. |
| `Class.super(cls?, self?)` / `super(...)` | Resolve the next implementation in the active MRO. |
| `Class.monitor` / `Class.unmonitor` | Add or remove a named subscription to a field. |
| `Class.MISSING` | Sentinel used when a monitored field had no previous value. |

Use `Class.monitor(target, name, callback, params?, notifyEqualWrites?, identifier?)` to subscribe to writes on a table or userdata field without invoking `callback` immediately, and `Class.unmonitor(target, name, identifier?)` to remove that subscription:

- **Identifiers and ordering:** `identifier` defaults to `""`. Registering again with the same identifier replaces that subscription in place; subscriptions with distinct identifiers run in registration order. `Class.unmonitor` removes only the named subscription and ignores missing ones.
- **Value filtering and arguments:** Monitored fields reject `nil` assignments and notify only when their visible value changes unless `notifyEqualWrites` is `true`. A previously missing or explicit `nil` value reports `Class.MISSING`. Each callback receives `(oldValue, newValue, ...)`, followed by the entries of `params`.
- **Re-entrancy and errors:** Re-entrant assignments take effect while each subscription suppresses its own recursion. Callback errors stop dispatch and propagate without rolling back the assignment.
- **Snapshot dispatch and teardown:** Dispatch iterates a subscription snapshot: removing or replacing a subscription before its turn skips the old callback, and newly registered callbacks start on subsequent assignments. Removing the last subscription restores ordinary field storage and the table's original metatable.

### Type checks

Use `Class.isInstance` for every type check. Its target may be a finalised Ludork or registered native class, or an exact Lua type name: `nil`, `boolean`, `number`, `string`, `function`, `userdata`, `thread` or `table`.

```lua
if Class.isInstance(value, "string") then
    print(string.upper(value))
end

local isActor = Class.isInstance(actor, Actor)
local isNativeVector = Class.isInstance(position, sf.Vector2f)
local isInteger = Class.isInstance(value, "number") and math.type(value) == "integer"
```

String targets preserve Lua `type` semantics: native containers are userdata, and callable tables are reported as tables. Unknown names return `false`; targets that are neither strings nor tables raise argument errors. Call `math.type` only after a number check, use `Class.type` only when the resolved type value is needed and never compare a Lua `type` result with `==` or `~=`.

## Project truth values and copies

Ludork's `bool(value)` extends [Lua truth values](<Lua Basics.md#operators>): `0`, empty strings, empty tables and empty native containers also count as `false`. Use explicit `nil` comparisons when absence is a distinct protocol value.

| API | Behaviour |
|---|---|
| `copy(value)` | Shallow-copy tables and supported native values. |
| `deepcopy(value)` | Recursively copy keys and values while preserving aliases and cycles. |
| `asizeof(value)` | Estimate the memory owned by a Lua or native value graph, in bytes. |

For known bound value types, call `value:copy()` or `value:deepcopy()` directly (for example `position:copy()`, `rect:copy()` or `colour:copy()`) to produce independent values. Global `copy` and `deepcopy` handle Lua tables and dynamic graphs, delegating supported bound values to the same LuaGlue entry points. Resources without a value-copy policy are returned by reference rather than copied.

For monitored tables, `copy` and `deepcopy` copy the current logical field values and preserve the original metatable without the monitoring proxy or subscriptions. The copied values can be saved normally; subscriptions belong to live instances and must be registered again after loading.

## Native containers

`list`, `tuple` and `dict` are native containers. They do not replace the Lua tables required by JSON, metadata, Blueprint values or Core bindings.

```lua
local route = list("north", nil, "east")
local position = tuple(4, 7)
local visited = dict {
    [position] = true,
}
```

| Type | Main operations |
|---|---|
| `list` | `append`, `extend`, `insert`, `pop`, `remove`, `clear`, `index`, `count`, `contains`, `reverse`, `sort`, `copy`, `unpack`, `toTable` |
| `tuple` | `index`, `count`, `contains`, `unpack`, `toTable` |
| `dict` | `get`, `setdefault`, `update`, `pop`, `remove`, `clear`, `contains`, `keys`, `values`, `items`, `copy`, `toTable` |

- **Construction and indexing:** `list(...)` and `tuple(...)` store arguments in 1-based slots; a single table, `list` or `tuple` argument is shallow-copied as a sequence. `dict(mapping?)` copies a mapping. Tuples reject `nil`, whereas lists and dictionaries preserve `nil` slots and values. Negative sequence indices are invalid, and list writes and inserts must target an existing slot or `#value + 1`.
- **Iteration and lookup:** Native `ipairs` visits `nil` list slots, `dict` iteration preserves insertion order and structural mutation during iteration raises an error. `dict:get(key, default)` uses `default` only when the key is missing; `dict:contains(key)` distinguishes absence from a stored `nil`. Method names take precedence over same-named string keys on `dict`, so read them with `value:get("get")` or `dict.get(value, "get")`.
- **Mutation and snapshots:** Remove dictionary entries with `remove` or `pop` rather than `dictionary[key] = nil`. `remove` returns whether it found the key; `pop` raises on a missing key unless given a default. `list:pop()` removes the last slot by default, `list:remove` raises when its value is absent and `list:sort(compare?)` is stable. `dict:keys()` and `dict:values()` return insertion-ordered `list` snapshots; `dict:items()` is a lazy iterator.
- **Equality and tuple keys:** Lists and dictionaries compare structurally, including cyclic graphs. Tuples act as immutable native-dictionary keys: booleans, finite numbers, strings and nested tuples compare by value, whereas mutable or reference values compare by identity. `nil` and non-finite numeric tuple keys are invalid. Ordinary Lua tables hash tuple userdata by identity.

### Conversion boundaries

`toTable` recursively converts native containers while preserving aliases and cycles. Converted lists and tuples preserve JSON-array shape when empty or nested, `nil` list slots and dictionary values become `cjson.null`, and tuple keys become compact strings such as `(1,2)` (failing on collision with an existing string key). JSON encoders, metadata, Blueprint values and Core bindings do not perform this conversion automatically.

Use record arrays instead when key identity must be reconstructed after loading.

## Math extensions

Standard adds these functions to Lua's `math` library. Except for `isFinite`, arguments must be finite Lua numbers (convert numeric strings explicitly with `tonumber`). Invalid ranges, arithmetic overflow and integer results outside the Lua integer range raise errors. `gcd` and `lcm` require Lua integers, and `round` and `trunc` preserve integer inputs exactly.

`lerp`, `inverseLerp` and `remap` support reversed intervals and extrapolation without clamping.

| API | Behaviour |
|---|---|
| `math.isFinite(value)` | Return `true` for finite Lua numbers, `false` otherwise. |
| `math.clamp(value, min, max)` | Return a float constrained to `[min, max]`. Require `min <= max`. |
| `math.lerp(a, b, t)` | Linearly interpolate from `a` to `b` by `t`. |
| `math.round(value)` | Round to the nearest integer, choosing the even integer at exact halves. |
| `math.trunc(value)` | Truncate towards zero and return an integer, unlike the exact conversion in `math.tointeger`. |
| `math.isNearZero(value, epsilon=0.1)` | Test `abs(value) < epsilon`. Require `epsilon >= 0`. |
| `math.gcd(a, b)` | Return the non-negative greatest common divisor. `gcd(0, 0)` is `0`. |
| `math.lcm(a, b)` | Return the non-negative least common multiple, or `0` if either argument is `0`. |
| `math.sign(value)` | Return the integer `-1`, `0` or `1`. Both signed zeros return `0`. |
| `math.inverseLerp(a, b, value)` | Return `(value - a) / (b - a)`. Require `a ~= b`. |
| `math.remap(value, inMin, inMax, outMin, outMax)` | Map proportionally from the input interval to the output interval. Require `inMin ~= inMax`. |
| `math.smoothstep(edge0, edge1, value)` | Clamp the normalised position `t` to `[0, 1]`, then return `t²(3 − 2t)`. Require `edge0 < edge1`. |
| `math.moveTowards(current, target, maxDelta)` | Move by at most `maxDelta` without passing the target. Require `maxDelta >= 0`. |

## String and table extensions

Standard adds these helpers to Lua's existing libraries:

| API | Purpose |
|---|---|
| `string.pformat` | Format `{}` positional fields or `{name}` fields from a final mapping table. `{{` and `}}` escape braces. |
| `string.contains`, `string.startsWith`, `string.endsWith` | Test string contents or boundaries. |
| `string.isEmpty`, `string.isBlank` | Test empty or whitespace-only text. |
| `string.strip`, `string.stripLeading`, `string.stripTrailing` | Remove Unicode whitespace. |
| `string.replace`, `string.split` | Replace or split text. |
| `string.utf8Length`, `string.utf8Slice` | Measure or slice by UTF-8 codepoints. |
| `string.graphemeLength` | Count Unicode grapheme clusters (user-perceived characters). |
| `string.stripUnicode` | Trim Unicode `White_Space` from both ends. |
| `table.contains`, `table.index` | Search a dense Lua array using Lua equality, stopping at the first `nil` slot. |
| `table.orderedStringKeys` | Return preferred keys first, then remaining string keys sorted. |

## Platform, configuration and files

`PLATFORM` is the lowercase target name (`win32`, `darwin`, `ios`, `android`, `ohos`, `web` or the CMake system name). `LUDORK_MOBILE` and `LUDORK_DESKTOP` expose the runtime host category: on HarmonyOS, phones and tablets report mobile and 2-in-1 devices report desktop. Do not infer the platform from paths or environment variables.

`configparser.ConfigParser()` provides `read`, `write`, `has_section`, `add_section`, `get`, `getfloat`, `getint`, `getboolean` and `set`. `read(path)` and `write(path)` select XML plist for a `.plist` extension (case-insensitive) on macOS only; all other paths, including explicit `.ini` files on macOS, use INI. Use [the game configuration path API](<../Lua and Blueprint Scripting/Default Gameplay/Runtime Data Configuration and Saves.md#configuration>) for the platform's default game configuration file.

Configuration plists contain one root dictionary whose keys name sections, and each section is a dictionary of keys and string values (including numbers and booleans written as text):

```xml
<plist version="1.0">
  <dict>
    <key>Main</key>
    <dict>
      <key>scale</key><string>1.25</string>
      <key>verticalsync</key><string>true</string>
    </dict>
  </dict>
</plist>
```

- **Plist restrictions:** Binary plists and non-string values (`<integer>`, `<real>`, `<true/>`, arrays or nested dictionaries) are unsupported, and keys that differ only in case within one section are rejected. Empty sections and strings, Unicode text and interior whitespace are preserved; XML characters such as `&` and `<` are escaped on write.
- **Case sensitivity and conversion:** Section names are case-sensitive; keys are case-insensitive while preserving the last spelling passed to `set`. Values are stored as strings and converted on read by `getfloat`, `getint` and `getboolean` (`1`/`yes`/`true`/`on` are `true`; `0`/`no`/`false`/`off` are `false`, ignoring case and surrounding whitespace). Missing values or failed conversions return the fallback or `nil`.
- **Merging and read errors:** Repeated successful reads merge into existing sections and preserve unrelated keys. `read` returns `false` if the file is absent (INI also returns `false` on open failure); an unreadable, malformed or unsupported existing plist raises an error and leaves existing sections unchanged.
- **Write atomicity:** `write` serialises before opening the destination so serialisation failures leave the existing file intact; file-write errors raise.

`locale.getdefaultlocale()` returns the host locale pair.

Filesystem extensions:

| API | Purpose |
|---|---|
| `os.getcwd`, `os.listdir` | Read the current directory, or sorted direct entry names relative to that directory. |
| `os.createDirectories` | Create a UTF-8 directory path and any missing parents; existing directories are accepted. |
| `os.removeFile` | Remove the file at a UTF-8 path. Missing files and failures raise. Does not replace `os.remove`. |
| `os.path.join`, `os.path.splitext`, `os.path.basename`, `os.path.dirname`, `os.path.abspath` | Build or inspect paths. |
| `os.path.isdir`, `os.path.isfile` | Test existing directory or file paths. Missing paths return `false`; inspection errors propagate. |
| `os.path.getmtime` | Return a file modification timestamp. |

`os.listdir`, `os.path.isdir`, `os.path.isfile` and `io.open` access only the real filesystem; `/Game/Assets/...` paths work only with Ludork resource APIs. For packed Data, `os.path.getmtime("Data/...")` returns the timestamp of the containing `.ldpak` file; other filesystem APIs do not expose archive entries.

Binary-safe codec globals:

| API | Purpose |
|---|---|
| `zlib.compress`, `zlib.decompress` | Compress data to zlib, or decompress zlib or gzip data. |
| `base64.encode`, `base64.decode` | Encode or decode Base64. Decoding accepts whitespace and valid padding, and rejects malformed data. |

## Cooperative tasks and file batches

`asyncio.create_task(callback, ...)` schedules a cooperative task and returns its handle. Inside a task, call `asyncio.sleep(seconds)` for a delay or `asyncio.await(operation)` to wait for an asynchronous result while the game loop continues. If a task raises a Lua runtime error, the engine ends the task, logs its Lua stack trace and keeps running other tasks.

Use `asyncio.create_required_task(callback, ...)` for work that must succeed for the game to run. It takes the same arguments and returns the same handle type, but an unhandled Lua error in a required task ends the game session.

For example, while a `SceneMap` is active, these configured tutorials run in sequence:

```lua
local Scene = require("GlobalFunctions.Scene")

local task = asyncio.create_task(function ()
    asyncio.await(Scene.ShowTutorial("Map01Menu"))
    asyncio.await(Scene.ShowTutorial("Map01Dialogue"))
    asyncio.await(Scene.ShowMessage("Guide", "Ready to continue."))
end)
```

`await` returns the operation's final result; if the operation is cancelled, the task ends without executing the next statement. Call `asyncio.cancel_task(task)` to stop only that task while the shared operation and its other waiters stay active. Task completion and cancellation execute Lua `__close` cleanup.

See [AsyncOperation](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Events and Module Root.md#asyncoperation>) for the operation API and [Lua Runtime and Modules](<../Lua and Blueprint Scripting/Lua Runtime and Modules.md#asynchronous-operation-updates>) for custom-entry initialisation.

For larger reads, `asyncio.start_file_batch(specs)` reads independent files concurrently and delivers them in manifest order. `asyncio.file_worker_count()` returns the process's default parallel file-read limit based on available logical processors; a batch uses at most that many workers and at most one per file (WebGL uses one cooperative worker).

- **Specifications:** Give each specification a unique `category` and a `root`. For a directory scan, set `suffix` (and optionally `excludeSuffix` or `recursive = true`); for a single file, set `file` to a relative path under `root` without scan options. Logical `Data/...` roots read packaged data—including the encrypted equivalent of a `.json` file—while other roots use the physical filesystem. A missing required input fails the job.
- **Polling and cancellation:** `asyncio.poll_file_batch(job, maxItems?, waitForItem?)` returns progress, ordered items and any structured failure, defaulting to one item with no wait. Pass a positive `maxItems` and `true` to wait for the next ordered item or a terminal state, or pass `0` to inspect progress without collecting items; use non-blocking polls during frame updates. Call `asyncio.cancel_file_batch(job)` to cancel the job.
- **Job states:** Match `state` against the `asyncio` constants `FILE_BATCH_SCANNING` (`"scanning"`), `FILE_BATCH_RUNNING` (`"running"`), `FILE_BATCH_COMPLETED` (`"completed"`), `FILE_BATCH_CANCELLING` (`"cancelling"`), `FILE_BATCH_CANCELLED` (`"cancelled"`) and `FILE_BATCH_FAILED` (`"failed"`). Completed, cancelled and failed are terminal job states; delivered data conversions follow their own stepping contract below.
- **Worker decoding (`parseData = true`):** Selected filenames or suffixes must end in `.json` or `.ldc`. Workers decode JSON or binary LDC (reporting decode failures with `operation = "parse"`), and polling returns a `FileBatchDataConversion` whose `contentBytes` gives the decoded payload byte count before conversion into Lua values.
- **Incremental conversion:** Advance a `FileBatchDataConversion` on the Lua logic thread with `asyncio.step_file_batch_data(conversion, maxNodes, maxMilliseconds)` using positive budgets. It returns `completed, processed, data` (delivering `data` once on completion). Matching `Engine.loadData`, arrays carry their logical length in `n`, root null becomes `nil`, null array slots are absent keys and null object fields keep `cjson.null`.
- **Conversion lifetime:** Call `asyncio.clear_file_batch_data` to release a conversion idempotently; cancellation, garbage collection and shutdown also invalidate it. Conversions cannot cross Lua VMs or threads.
- **Raw bytes (`parseData` omitted or `false`):** `item.content` contains the original file bytes, including the LDC header and encoded payload. Pass them to `Engine.decodeData` when needed (Blueprint caches keep these bytes for deferred decoding).

## Timing and memory diagnostics

`perfCounter()` returns a monotonic high-resolution timestamp in seconds. `processMemoryMB()` returns current process memory in MiB; use `asizeof` to measure an owned value graph rather than process-wide memory.
