# GlobalFunctions.ScreenEffects

`GlobalFunctions.ScreenEffects` provides Blueprint nodes for triggering and clearing screen flashes, colour tones and screen shakes.

**Metadata source:** `Scripts/GlobalFunctions/ScreenEffects_meta.lua`

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `FlashScreen` | `red: int = 255`, `green: int = 255`, `blue: int = 255`, `alpha: int = 255`, `duration: float = 0.5` | — | Flashes the screen with the specified RGBA colour for `duration` seconds. |
| `StopFlashScreen` | — | — | Stops the active screen flash. |
| `ChangeScreenTone` | `red: int = 0`, `green: int = 0`, `blue: int = 0`, `gray: int = 0`, `duration: float = 0` | — | Transitions the screen colour tone and greyscale (`gray`) over `duration` seconds. |
| `ClearScreenTone` | `duration: float = 0` | — | Clears the screen colour tone over `duration` seconds. |
| `ScreenShake` | `power: float = 4`, `speed: float = 10`, `duration: float = 0.5` | — | Shakes the screen with the given power, speed and duration. |
| `StopScreenShake` | — | — | Stops the active screen shake. |
