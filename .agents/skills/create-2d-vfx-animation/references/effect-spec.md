# Effect specification and timing

## Production brief

Capture this compact schema before generation:

```yaml
effect: water burst
usage: battle spell centered on target
motion: clockwise spiral erupts outward and breaks into droplets
view: front
canvas: 512x512
frames: 12
fps: 12
playback: one-shot
anchor: center
safe_area: 85%
palette: bright cyan, azure, deep blue, white highlights
medium: hand-painted 2D JRPG VFX
alpha: true transparency
must_keep: three major water ribbons, cyan core, clockwise flow
avoid: character, environment, text, border, ground shadow, checkerboard
```

## One-shot phase template

For 12 frames, start with this allocation and adapt it to the requested motion:

| Frames | Phase | Visible purpose |
| --- | --- | --- |
| 0–1 | anticipation | Small compressed core; direction becomes readable |
| 2–3 | ignition | Main shape appears rapidly; high acceleration |
| 4–6 | expansion | Footprint and secondary motion grow; silhouette evolves |
| 7–8 | apex | Largest, brightest, clearest spell identity |
| 9–10 | breakup | Core weakens; ribbons split into droplets or wisps |
| 11 | dissipation | Sparse fading remnants; no new large shape |

Do not force this template onto an impact that needs a one-frame flash or a projectile that needs travel frames. Preserve the narrative rhythm: preparation, event, consequence.

## Loop template

For a seamless aura, vortex, or status loop:

- Keep total footprint, center of mass, and average brightness approximately stable.
- Cycle internal motifs around the anchor instead of growing and disappearing.
- Make the last frame advance naturally into the first; do not duplicate the first frame as the last unless the engine requires it.
- Use phase offsets for secondary particles so all features do not reset together.
- Preview at least three cycles before accepting the loop.

## Export contract

- Pass the requested count as `--expected-frames N`; input names must be exactly `frame_000.png` through `frame_{N-1:03d}.png`, with no missing or extra PNGs.
- Previews play once by default. Add `--loop` for indefinite playback; `manifest.json` records `playback` as `one-shot` or `loop`.
- The export directory contains `frames/`, `spritesheet.png`, `preview.apng`, `preview.gif`, and `manifest.json`. Frame paths in the manifest are relative to that directory, so the whole package can be moved together.
- If the output `frames/` directory contains PNGs outside the requested sequence, export stops before writing; choose a new output directory instead of deleting unrelated files.
- Inputs must be PNGs on a common canvas and contain transparent pixels unless opaque output was explicitly requested. A fully transparent fade frame is allowed and reported as a warning.
- APNG preserves alpha. GIF is a preview composited over a dark background and cannot substitute for the original frames.

## Anchor and canvas rules

- Use a constant canvas for every frame.
- Keep the gameplay attachment point fixed even while the visual center moves.
- For target-centered bursts, use the canvas center.
- For ground impacts, use bottom-center and allow upward expansion.
- For projectiles, define a separate engine pivot if the visible mass moves across the canvas.
- Reserve enough padding for glow and droplets; intentional overscan must be declared.

## Temporal spacing

- Use larger shape changes during ignition and impact.
- Hold identity near the apex for one or two frames.
- Let breakup accelerate spatially while opacity decays.
- Use smear or stretched shapes for fast motion instead of uniform cross-fades.
- Avoid linear scaling as the only change; alter curvature, thickness, branching, fragmentation, and brightness distribution.

## Acceptance checklist

- The effect reads at intended in-game size.
- A viewer can identify the element and action from the apex silhouette.
- Consecutive frames share conserved motifs without becoming duplicates.
- Motion direction remains coherent.
- No unplanned teleporting, pulsing canvas, or camera movement occurs.
- Alpha edges do not show a dark or light matte.
- First/last behavior matches one-shot or loop semantics.
- The engine pivot remains stable.
