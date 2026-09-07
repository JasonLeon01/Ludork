# Prompt recipes

## Apex keyframe

```text
Use case: stylized-concept
Asset type: isolated 2D game VFX apex keyframe
Primary request: <effect action at maximum intensity>
Subject: <specific conserved motifs and silhouette>
Style/medium: <style lock>
Composition/framing: fixed <width>x<height> canvas, <anchor>, front view, effect fully inside 85% safe area
Color palette: <locked palette>
Lighting/mood: self-illuminated magical energy; highlights belong to the effect only
Constraints: genuine transparent background and preserved alpha; fixed camera; isolated effect; no clipping
Avoid: character, environment, floor, ground shadow, text, border, frame grid, checkerboard, watermark
```

## Individual frame

```text
Create one production frame of the same 2D game effect.

STYLE LOCK — DO NOT CHANGE:
<repeat medium, palette, edge treatment, glow, conserved motifs, canvas, view, anchor, safe area>

FRAME DELTA:
Frame: <index>/<last index>
Phase: <anticipation|ignition|expansion|apex|breakup|dissipation|loop phase>
Visible state: <exact footprint, curvature, thickness, fragmentation, opacity, brightness distribution>
Change from nearest reference: <one clear temporal change>

OUTPUT CONSTRAINTS:
One isolated effect frame only; genuine transparent background with alpha; no character; no environment; no floor shadow; no text; no border; no grid; no camera or canvas change.
```

## Water burst example

Use this as a pattern, not as a mandatory style:

```text
STYLE LOCK — DO NOT CHANGE:
Hand-painted 2D fantasy JRPG spell VFX; bright cyan core, azure mids, deep-blue outer water, small white specular accents; three primary clockwise water ribbons; strong readable silhouette; front view; centered origin; fixed 512x512 canvas; genuine transparent background; effect inside 85% safe area.

FRAME DELTA:
Frame 05/11, expansion phase. The cyan core has opened into a 60%-canvas circular burst. The same three ribbons curve clockwise, thicker near the center and tapering outward. Add six small droplets following the established arcs. Brightness is rising but remains below the apex. Compared with frame 04, expand mostly outward and increase curvature; do not rotate the whole effect or move its center.
```

## Targeted repair prompts

### Restore transparency

```text
Change only the background to genuine transparency and preserve the effect pixels, canvas size, placement, colors, glow, and silhouette. Remove any checkerboard, white matte, black matte, floor, and shadow. Preserve soft alpha edges.
```

### Fix center drift

```text
Change only the placement: align the declared attachment point to the exact canvas anchor used by the reference frame. Preserve shape, scale, palette, detail, alpha, and canvas size.
```

### Fix style drift

```text
Restyle only this frame to match the reference frame's hand-painted edge treatment, detail density, palette, highlight size, and glow falloff. Preserve this frame's phase-specific silhouette, footprint, fragments, anchor, canvas, and transparency.
```

### Fix unwanted scenery

```text
Remove only all characters, scenery, floor cues, borders, labels, and unrelated objects. Keep the isolated spell effect unchanged on a genuine transparent background.
```
