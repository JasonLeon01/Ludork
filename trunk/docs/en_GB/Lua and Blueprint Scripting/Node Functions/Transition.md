# GlobalFunctions.Transition

Both functions return `Engine.AsyncOperation` and complete with `true`: freeze waits for capture of the outgoing frame (`Frozen`), and transition waits for submission of its final frame (`Finished`). Replacement and cancellation follow [GlobalCore.Transition](<../Global and Core Modules/Core Modules/GlobalCore/Scenes and System.md#transition>).

Metadata source: `Scripts/GlobalFunctions/Transition_meta.lua`

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `FreezeTransitionBackground` | `function` | — | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "Frozen", Frozen = { [1] = true } } |
| `RequestTransition` | `function` | transitionName: string = ""; transitionTime: float = 1 | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "Finished", Finished = { [1] = true } }; Meta { PathVars = { [1] = { [1] = "transitionName", [2] = "/Game/Assets/Transitions" } } } |
