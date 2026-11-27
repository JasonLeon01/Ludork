# Execution Flow, Events and Variables

Blueprints define class defaults and visual event graphs in `Data/Blueprints`, while Common Functions provide reusable graphs shared across Blueprints. See [Blueprints, Common Functions and Actors](<../../Editor User Guide/Blueprints Common Functions and Actors.md>) for editor usage. Script Mixin Blueprints define their events in [Lua](<../Script Mixins/Authoring Workflow.md>) and do not execute graphs.

## Events

An event is declared with `type = "event"`. Its Event entry node exposes ordered parameter outputs and a single execution output.

- **Local execution and fallback**: The execution connection selects the first node to run. If it is unconnected, the local graph does not execute; a class Blueprint then searches its parent chain for an implementation, skipping empty ancestor graphs. Common Functions and General Data graphs have no inherited-event fallback.
- **General Data member graphs**: General Data graphs run as synchronous abilities rather than inherited Blueprint events. `Source.Gameplay.GeneralDataGraphAbility` activates the selected graph with `GameplayEventData` as its owner. A `null` start node returns `NoGraph`. Latent nodes are invalid inside General Data graphs, and runtime errors propagate to the caller.

## Parent event calls

New graph-mode Blueprints and newly added local event graphs initialise with an `Event → Parent` connection whenever an ancestor declares the same event, with initial data links forwarding each event parameter to the matching Parent input. A parent Blueprint's custom event without a metadata signature has no parameter inputs. Existing graphs are not populated automatically, and Script Mixin graphs are inactive.

A Parent node has one execution input, one execution output, typed data inputs matching the event parameters, and no receiver or return-data pins. It resolves the event implementation above the class that owns the current graph.

- **Argument resolution**: Each input supplies its connected value or unconnected default. The argument array follows `SUPER` length and fallback rules: missing or `nil` arguments can fall back to the caller's event context.
- **`nil` holes and overrides**: A `nil` input does not clear the original event argument, and values after a `nil` hole are subject to the same array-length boundary. Non-nil arguments within that boundary — including `false` — override the corresponding event values.

[GlobalFunctions.Utils.SUPER](<../Node Functions/Utils.md>) invokes a parent event from Lua or Blueprints via `SUPER(obj, params)`. Ordinary instance methods use the `self` input described under [Instance method receivers](<#instance-method-receivers>).

## Common Functions

Common Functions share the caller's local context and graph owner, including across nested calls. `SELF` and default instance receivers therefore refer to that same owner.

## Execution semantics

Node metadata and graph links govern how execution flows through a graph:

- **`Pure = true`**: Removes execution input and output pins. Having return values does not imply purity.
- **`ExecSplit`**: Maps named execution outputs to runtime return values.
- **`Latent`**: Returns an `Engine.AsyncOperation`, suspends the graph and resumes along declared state outputs.
- **Re-entry suppression**: Every Blueprint event drops repeated triggers for the same graph instance and event key while latent nodes from the current execution are pending. Once all latent work resolves, new triggers are accepted. Different graph instances and event keys execute independently.
- **`LoopNode`**: Declares loop-body and completion execution outputs.
- **Pin cardinality**: Data links connect compatible typed pins, and execution links connect flow pins. Each execution output accepts one connection; execution inputs accept multiple. Data outputs accept multiple connections; each data input accepts one.

Runtime event queries and execution operate on native `Engine.Graph` userdata. Raw `GraphData` supports definition-time event inspection only, and local execution contexts hold per-invocation state rather than executable graphs. When constructing a graph without a parent class, pass the read-only string `Engine.Graph.NO_PARENT_CLASS` (`"NOT_WRITTEN"`) as `Graph.new`'s `parentClassName`; the `parent` parameter supplies the graph owner independently.

### Cancelling latent execution

Call `operation:cancel()` or `Engine.latentManager:cancel(operation)` to cancel waiting event executions — including pending nodes and loops — without running downstream outputs or completion callbacks. See [AsyncOperation](<../Global and Core Modules/Core Modules/Engine/Events and Module Root.md#asyncoperation>) for the shared Lua and Blueprint operation contract.

## Instance method receivers

Every instance-method node exposes a typed `self` input, including methods inherited by the current Blueprint. An explicit connection or entered value selects the receiver; if neither is set, the declared `"self"` default resolves to the graph owner. Connected `nil` values, incompatible receivers and missing owners raise an error before the method runs.

- **Parameter layout**: Short method names, `self.method` and fully qualified paths share one parameter order and pass the receiver once. Saved `params` place the receiver at index `0`: a stored `"self"` selects the owner, while an unset entry uses the declared default. Parameter links use the same zero-based indices. Static functions and event payloads have no receiver slot.
- **General Data graphs**: In General Data graphs, the owner is `GameplayEventData`. Connect its `target` (or another object) to `self` when calling an Actor method.

## Typed connections

The graph validator checks data pin connections against the declared types:

- **Unions**: A value connects to a union when at least one branch accepts its type. A union output connects only when the target accepts every branch.
- **Containers**: Lists, dictionaries and tuples apply compatibility rules recursively.
- **Inheritance**: Derived types connect to their declared bases.
- **Numbers and vectors**: `int` connects to `float`, whereas `float` to `int` and conversions between distinct SFML vector types are rejected.
- **`any`**: Allows dynamic connections whose actual runtime values are checked on invocation.
- **Enums**: Enum pins preserve their enum schema but check compatibility using the module's underlying `string`, `bool`, `int` or `float` type. They connect to compatible scalar pins and other compatible enums, including inside containers and unions. Integer enums connect to `float` inputs; `float` enums cannot connect to `int` inputs. Values do not need to match a current named choice.

> **Note:** Incompatible connections appear in red after loading or pasting a graph, with a tooltip explaining the mismatch. Fix them before performing a normal save.

## Defaults and references

New inputs start with their declared default or a valid literal for their type; object and function inputs without either stay unset. Saved literals must match the declared type, even when the pin is connected. Invalid literals appear as errors and block normal saving, and connected values are checked at runtime. See [Functions and events](<Metadata Schema and Decorators.md#functions-and-events>) for metadata default and return declarations.

An enum selector displays keys and saves the underlying scalar value, rereading the enum file whenever the selector opens or regains focus. An unknown saved value stays preserved with an informational message, whereas a value of the wrong scalar type is invalid. Plain-text inputs expect the scalar value, not the displayed key. See [Enum schemas](<Metadata Schema and Decorators.md#enum-schemas>) for module and metadata examples.

## Class attributes

A Blueprint stores attribute values in `attrs` and declares newly introduced variables in a sibling `attrDefs` object with an explicit `type`; **Add Attribute** writes both. Inherited metadata, Script Mixin and ancestor Blueprint fields already carry declarations, so a child Blueprint overrides only the value in `attrs`. Field access nodes and Actor Info use that inherited type, including enum identity and nested container types.

> **Important:** Missing declarations, incompatible values and duplicate declarations fail both editor and runtime loads. Neither global save nor **Save Invalid Blueprint** bypasses class schema validation.

Map Actor overrides validate against the same resolved class schema. See [Blueprint attribute declarations](<Metadata Schema and Decorators.md#blueprint-attribute-declarations>) for JSON examples.

## Class definitions and session lifetime

`ClassDict` instances within one Lua VM share resolved classes, definition tables and compiled graph templates. Changes to JSON class definitions or graph templates require a restart; [Lua hot reload](<../Lua Runtime and Modules.md#lua-hot-reload>) updates only compatible script functions.

`ClassDict:getData(path)` — also exposed as `Global.Data.GetClassData(path)` in Default Gameplay — uses the same resolution path as `get` and returns the resolver's JSON definition table. Unknown paths raise an error, and Lua module classes without a JSON definition return `nil`. Repeated reads within one VM return the same table, so use `copy` or `deepcopy` when you need an independent copy.

Mutating that returned table affects subsequent definition reads, but does not rebuild generated classes, instance-default records or compiled templates; new instances and executable-event queries use the previously generated results.
