# GlobalCore: Time Management

This page documents scene timer entries (`TimerEntry`) and the static global clock and time-scale service (`TimeManager`). Instance method tables omit the implicit `self` receiver exposed as the first Blueprint target pin; static methods take no receiver.

## TimerEntry

Represents a scheduled scene timer. `isReady()` indicates that the timer has elapsed or been cancelled, not that its callback has finished; `cancel()` cancels the associated operation and prevents its expiry callback. To await completion, use the `Engine.AsyncOperation` returned by [SceneBase.addTimer](<Scenes and System.md#scene-timers>).

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `time` | `float` | — | — |
| `params` | `any[]` | — | — |
| `blocking` | `bool` | — | — |

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `task` | — | — | Callback invoked on expiry. |
| `isReady` | — | `bool` | — |
| `isCancelled` | — | `bool` | Pure. |
| `cancel` | — | — | — |

## TimeManager

Static time-scaling service. `getDeltaTime()` returns the frame delta with the current playback speed applied.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getCurrentTime` | — | `sf.Time` | Static. |
| `getDeltaTime` | — | `sf.Time` | Static. |
| `getSpeed` | — | `float` | Static. |
| `setSpeed` | `speed: float` | — | Static. |
