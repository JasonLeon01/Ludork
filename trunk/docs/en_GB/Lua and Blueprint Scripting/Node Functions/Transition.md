# GlobalFunctions.Transition

`GlobalFunctions.Transition` provides latent Blueprint nodes for capturing the outgoing frame and playing a screen transition. Both functions return `Engine.AsyncOperation` and complete with `true`: freeze waits for capture of the outgoing frame (`Frozen`), and transition waits for submission of its final frame (`Finished`). See [GlobalCore.Transition](<../Global and Core Modules/Core Modules/GlobalCore/Scenes and System.md#transition>) for replacement and cancellation.

**Metadata source:** `Scripts/GlobalFunctions/Transition_meta.lua`

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `FreezeTransitionBackground` | — | `Engine.AsyncOperation` | Latent. Branch: `Frozen` (`true`). Captures the outgoing frame as the transition background. |
| `RequestTransition` | `transitionName: string = ""`, `transitionTime: float = 1` | `Engine.AsyncOperation` | Latent. Branch: `Finished` (`true`). Path (`transitionName`): `/Game/Assets/Transitions`. |
