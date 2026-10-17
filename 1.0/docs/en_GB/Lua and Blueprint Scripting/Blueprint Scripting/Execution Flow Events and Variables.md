# Execution Flow, Events and Variables

Blueprints define class defaults and event graphs in `Data/Blueprints`; Common Functions provide reusable graphs. See [Blueprints, Common Functions and Actors](<../../Editor User Guide/Blueprints Common Functions and Actors.md>) for editing. Script Mixin Blueprints define events in [Lua](<../Script Mixins/Authoring Workflow.md>) and do not execute their graphs.

## Events

An event is declared with `type = "event"`. Its Event entry provides ordered parameter outputs and one execution output. The execution connection selects the start node; without it, the local graph does not execute. A class Blueprint then searches its parent chain for the event implementation, continuing past empty ancestor graphs. Common Functions and General Data graphs have no inherited-event fallback.

General Data member graphs are synchronous abilities, not inherited or implicit Blueprint events. `Source.Gameplay.GeneralDataGraphAbility` activates a selected graph with `GameplayEventData` as parent. A `null` start returns `NoGraph`. Latent nodes are invalid, and errors propagate to the caller.

## Parent event calls

New graph-mode Blueprints and explicitly added local event graphs initialise `Event → Parent` when the parent chain declares the same event. Initial data links forward each event parameter to the corresponding Parent input. A parent Blueprint's custom event without a metadata signature has no parameter inputs. Existing graphs are not populated automatically; Script Mixin graphs remain inactive.

A Parent node has one execution input and output, typed inputs for the event parameters, and no receiver or return-data pins. It looks for the event implementation above the class that owns the current graph.

Each input supplies its current connected value or ordinary unconnected default. The argument array follows the SUPER length and fallback rules: missing or nil arguments may be filled from the caller's event context. A nil input does not explicitly clear the original event argument; values after a nil hole are subject to the same array-length boundary. Non-nil arguments within that boundary, including `false`, override the corresponding event values.

[GlobalFunctions.Utils.SUPER](<../Node Functions/Utils.md>) calls a parent event through `SUPER(obj, params)`. Ordinary instance methods use the `self` input described below.

## Common Functions

Common Functions share the caller's local context and graph owner, including nested calls. `SELF` and default instance receivers therefore refer to that same owner.

## Execution semantics

- `Pure = true` removes the execution input and output pins. Return values do not imply purity.
- `ExecSplit` maps named execution outputs to result values.
- `Latent` nodes return an `Engine.AsyncOperation`, suspend the graph and resume through declared states.
- Every Blueprint event drops repeated triggers for the same graph instance and event key while latent nodes from the current execution remain pending. A new trigger is accepted after all latent work of that execution resolves. Different graph instances and event keys remain independent.
- `LoopNode` describes loop-body and completion behaviour.
- Data links connect compatible typed pins, and execution links connect flow pins.

Runtime event queries and execution use native `Engine.Graph` userdata. Raw GraphData supports definition-time event inspection. Local execution contexts hold invocation state and are not executable graphs.

### Cancelling latent execution

`operation:cancel()` or `Engine.latentManager:cancel(operation)` cancels the waiting event executions, including their pending nodes and loops, without running outputs or completion callbacks. See [AsyncOperation](<../Global and Core Modules/Core Modules/Engine/Events and Module Root.md#asyncoperation>) for the shared Lua and Blueprint operation contract.

## Instance method receivers

Every instance-method node keeps its typed `self` input, including methods inherited by the current Blueprint. An explicit connection or filled value selects the receiver. With no connection and no value, the declared `"self"` default selects the graph owner. Connected nil values, incompatible receivers and missing owners fail before the method runs.

Short method names, `self.method` and fully qualified paths use the same parameter order and pass the receiver exactly once. Saved `params` follow that order, with the receiver at index 0; a stored `"self"` selects the owner, and an unset value uses the declared default. Parameter links use the same zero-based indices. Static functions and event payloads contain no receiver slot.

In General Data graphs, the owner is `GameplayEventData`. Connect its `target` or another intended object when calling an Actor method.

## Typed connections

A value may connect to a union when at least one branch accepts its type. A union output may connect only when the target accepts every possible branch. Lists, dictionaries and tuples apply the rule recursively. Derived types may connect to their declared bases. `int` may connect to `float`, but a `float` to `int` connection and conversions between different SFML vector types are rejected. `any` allows dynamic connections whose actual values are checked at runtime.

Enum pins retain their enum schema but use the enum module's underlying `string`, `bool`, `int` or `float` for compatibility. They can connect to compatible scalar pins and other enums with compatible underlying types, including recursively inside containers and unions. Integer enums may connect to float inputs; float enums may not connect to integer inputs. Values need not match one of the current named choices.

Incompatible connections remain visible in red after loading or pasting a graph, and their tooltip explains the error. Correct the connection before a normal save.

## Defaults and references

New inputs use the declared default or a valid literal for their type. Object and function inputs without either remain unset. Saved literals must match the declared type, even on connected inputs. Invalid values remain visible and block normal saving, and connected values are checked at runtime. Metadata default and return declarations are covered in [Functions and events](<Metadata Schema and Decorators.md#functions-and-events>).

An enum selector shows the key and saves its scalar value. Reopening it or returning to its field rereads the enum file. An unknown saved value is preserved with a message; a value of the wrong scalar type is invalid. Plain-text inputs accept the scalar, not the displayed key. See [Enum schemas](<Metadata Schema and Decorators.md#enum-schemas>) for module and metadata examples.

## Class attributes

The Blueprint's `attrs` contains values. Newly introduced variables also need a sibling `attrDefs` entry with an explicit `type`; **Add Attribute** writes both. Inherited metadata, Script Mixin and Blueprint fields retain their existing declarations, so a child overrides only the value. Field access nodes and Actor Info use that inherited type, including enum identity and nested container types.

Missing declarations, incompatible values and duplicate declarations fail both editor and runtime reads. The editor's global save and **Save Invalid Blueprint** cannot bypass this class schema check. Map Actor overrides use the resolved class schema as well. See [Blueprint attribute declarations](<Metadata Schema and Decorators.md#blueprint-attribute-declarations>) for stored examples.

## Class definitions and session lifetime

ClassDict instances in one Lua VM share resolved classes, definition tables and compiled graph templates. Changes to JSON class definitions or graph templates require a restart. [Lua hot reload](<../Lua Runtime and Modules.md#lua-hot-reload>) updates only compatible script functions.

`ClassDict:getData(path)`, also exposed as `Global.Data.GetClassData(path)` in the Game project, resolves through the same loading path as `get` before returning the resolver's JSON definition table. Unknown paths raise, and module classes without a JSON definition return nil. Repeated reads in one VM return the same table. Use `copy` or `deepcopy` when an independent table is needed.

Editing that table is visible to later definition reads, but it does not update generated classes, instance-default records or compiled templates. New instances and executable-event queries continue to use the generated results.

## Related pages

- [Metadata Schema and Decorators](<Metadata Schema and Decorators.md>)
- [Blueprints, Common Functions and Actors](<../../Editor User Guide/Blueprints Common Functions and Actors.md>)
- [GlobalFunctions.Container](<../Node Functions/Container.md>)
