# Runtime Value Boundaries

Choose native C++ API types according to the values an interface accepts, the ownership semantics it needs and whether it crosses the Lua VM boundary.

## Type selection

| Requirement | Type |
|---|---|
| Fixed fields or a fixed set of named choices | A struct, enum or concrete C++/SFML type |
| A finite choice of value types | `std::variant<T...>` |
| A value that may be absent | `std::optional<T>`; use `std::monostate` when `nil` is a variant alternative |
| Recursive JSON-compatible data without a VM | `RuntimeData` |
| Ownership of a known native object | A typed pointer with shared or weak ownership as required |
| Open script values, including native objects and references | `RuntimeValue` |
| Lua identity for equality, subscriptions or later calls | `RuntimeHandle` or `RuntimeIdentityPtr`, tied to one session |

Use `RuntimeValue` for open script boundaries such as event payloads, Blueprint pins and shared gameplay contexts. Convert to a concrete type before entering an algorithm with a fixed contract, keeping absence distinct from zero and from empty containers.

## Compiled native contracts

- **Compiled type schemas:** `<Runtime/TypeSchema.hpp>` defines recursive native type descriptions. `TypedDataService::compileType` parses a script description, and the typed overloads of `resolveTypedDataValue` and `resolveRuntimeTypedValue` reuse that compiled schema. A compiled `NodeDefinition` holds ordered parameter descriptions shared by its `DataNode`, template and instances, compiling each parameter's schema on its first required validation. Public entry points accepting mutable type tables parse the supplied description on each call.
- **Blueprint and Actor records:** Blueprint event descriptors, generated-class initialisation plans and component field mappings use native records, and native graph services pass `std::shared_ptr<Graph>`. Fixed Actor events keep their typed arguments until event dispatch needs script values, whereas open graph pins, defaults, multiple returns, shared local contexts and user fields retain dynamic value semantics. Native component copies reconstruct record values from their declared types before validation; caller-supplied component writes are strictly validated.
- **Numeric contracts:** `RuntimeNumber` is `std::variant<std::int64_t, double>`, also used by `GameplayNumber`. `AttributeSet` numeric access converts at the Lua field boundary, while `AbilitySystem` snapshots and notifications keep `RuntimeNumber` internally. Lua fields hold the `Current`-value storage, including direct writes and monitor notifications.
- **Cache ownership and `ClassDict`:** Component and event descriptor caches use Lua weak-key indexing. Private userdata owns their native records while associated Lua values live in userdata uservalues so GC and hot reload trace them (a C++ weak pointer is not a Lua weak reference). Session-owned class records keep explicit references and are cleared on shutdown. `ClassDict:getData` returns the original shared definition table; editing that table does not mutate compiled defaults or graph templates.

## Lua value conversion

- **Variants and primitives:** Direct Lua calls pass a `std::variant` alternative as its actual value with no wrapper. Lua integers prefer integer alternatives and floating-point numbers prefer floating-point alternatives; fractional numbers are never truncated into integers. Strings and booleans require their exact Lua types, and native SFML types remain distinct even when sharing the same numeric shape.
- **Enum schemas:** An enum schema such as `{ enum = "Enums.Engine.Direction" }` resolves its constant module and validates values against the underlying scalar type. Calls and stored values carry the scalar directly, and values outside the named choices remain valid when their scalar type matches. A dictionary's optional enum `key` must explicitly declare `valueType = "string"` and remains string-keyed at runtime. Unions store their normal branch wrapper while passing only the restored value at runtime. See [Enum schemas](<../Lua and Blueprint Scripting/Blueprint Scripting/Metadata Schema and Decorators.md#enum-schemas>).
- **Pure-data conversion:** Used by `RuntimeData` and `BIND_CLASS(pure_data = true)`, pure-data conversion accepts `nil`, the registered JSON-null sentinel, booleans, finite numbers, strings, acyclic arrays and string-keyed maps. It rejects functions, native objects, cycles, invalid keys and unrecognised metatables.
- **Arrays vs maps:** An empty Lua `{}` converts as a map, while `{ n = 0 }` converts as an empty array. A non-negative integer `n` preserves array length and `nil` holes, and emitted arrays include `n`. Registered JSON array metatables are accepted, and map null entries use the registered JSON-null sentinel to preserve their keys.

## Native object wrappers

Returning a shared native object first reuses its existing Lua owner. Once that wrapper has been collected, sources exposing the [native Cast protocol](<Macro Reference.md#native-cast-declarations>) look up a registered writer via `ludorkDynamicTypeKey()`. The writer receives the source's static type key and an aliasing `std::shared_ptr` to its subobject; generated conversion bases enable statically checked pointer recovery (including non-zero multiple-inheritance offsets) while preserving the original control block.

- **Fallback behaviour:** A missing writer or unsupported source falls back to an ordinary static-type wrapper, including external bases such as `sf::Drawable` with neither a Cast protocol nor an existing Lua owner. Non-polymorphic value types use their declared static type without dynamic-writer registration.
- **Identity and cleanup:** The wrapper registers the exposed type and all conversion-base subobject addresses so repeated returns preserve Lua object identity. Lua callback adapters release captured references through the Cast protocol. Opaque Lua references preserve their original value and follow [Lua references and session lifetime](#lua-references-and-session-lifetime).

## Containers and views

- **Owning containers:** `RuntimeData` owns VM-independent data whose copies detach on mutation. The unbound C++ `RuntimeData::equals` method compares recursively with exact value types, ordered array elements and key-based map contents regardless of JSON object iteration order. `RuntimeValue` can own native objects without a VM or hold session-bound Lua handles; its `Array` is `std::vector<RuntimeValue>` and its `Map` is `std::unordered_map<std::string, RuntimeValue>`. Constructing from `RuntimeData` shares its containers, while the `Array` and `Map` constructors take ownership.
- **Borrowed views:** Read containers via `value.view().array()` or `.map()`, which return optional borrowed views. Array indexing and iteration yield `RuntimeValueView`, and `Map::find` distinguishes a missing key from a stored `nil`. Keep the owning source alive and unmodified while any view or borrowed string is in use. Views cannot be obtained from temporary owning values; call `toValue()`, `toArray()` or `toMap()` when ownership is needed.
- **Mutation and pure-data conversion:** `getMutableIf<Array/Map>()` materialises or detaches the container being modified. `toData()` shares an existing `RuntimeData` branch or recursively converts bridge containers, rejecting native objects and handles.

## Lua references and session lifetime

Ordinary argument and result passing preserves `RuntimeValue` directly. For explicit conversion and Lua-reference operations, include `<Runtime/RuntimeReference.hpp>`:

| Operation | Behaviour |
|---|---|
| `intern` | Obtains the handle required by `get`, `set` and `invoke`; reuses an existing handle or creates a new Lua table from an owning container |
| `retain` | Keeps scalars and existing non-scalar handles; converts owning containers to new Lua tables |
| `snapshot` | Copies ordinary acyclic arrays or string-keyed maps while preserving nested function and object references; other tables and opaque-identity bindings keep their identity |
| `data` | Converts to pure data, rejecting native objects and non-convertible references |

`snapshot` preserves array length through `n`, omits `nil` map fields and is independent of class and instance default cloning.

Lua calls preserve all return values (including intermediate and trailing `nil`) and propagate errors. Handles cannot cross Lua VMs and become invalid when their session closes. An empty handle or identity pointer wraps as `nil` in `RuntimeValue`.
