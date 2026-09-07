---
name: create-rpg-maker-tileset
description: "Create, extend, repair, pack, and validate 32px tileset PNGs in ChatGPT Work or Codex. Inspect existing terrain, walls, stairs, props, and multi-cell structures before editing, preserving their scale and layout. New sheets default to regular RPG Maker XP; excludes character sheets, battle animations, and autotiles."
---

# Create RPG Maker Tilesets

Create a usable tileset, not merely a concept sheet. Keep the art coherent, then enforce dimensions and grid rules deterministically.

## Host, tools, and files

- Resolve this skill's directory from its loaded `SKILL.md` location. Resolve `scripts/` and `references/` relative to that directory, not the shell working directory. In a cloud host, materialize the bundled files with the host's skill-resource tools before running scripts; do not reuse a local Windows path.
- Use the current host's built-in image-generation/editing tool. If the `imagegen` skill is available, load its built-in workflow. Follow the actual tool schema for references and returned images; do not invent tool parameters or require a ChatGPT browser session. If generation is unavailable, report that prerequisite; existing-asset packing and validation can still run. Do not silently switch to a paid API or require an API key.
- Use the user's output directory, or a new `output/tilesets/<theme-name>/` directory under the task workspace. Keep source modules in `tiles/`, with `manifest.json`, applied prompts, the final PNG, and the coordinate legend beside it. Copy accepted source art from the actual path or downloadable artifact returned by the image tool. Do not write outputs into this skill directory or import them into game data unless requested.
- Find a Python 3.10+ interpreter and verify Pillow using `"<python>" -c "from PIL import Image"`. Reuse an available environment; if Pillow is missing, install it into a task virtual environment with that interpreter's `-m pip install Pillow`. Do not assume `python3` exists on Windows. In PowerShell, invoke a quoted executable path with `&`.
- Deliver actual files: use host artifact links in cloud Work and absolute file links for local outputs. If the host cannot expose generated image bytes, identify the missing export capability instead of presenting an inline concept as a packed tileset.

## Choose the workflow

- **Existing asset:** open the actual image before deciding what to change. Read [existing-asset-editing.md](references/existing-asset-editing.md), inspect details and measure the grid, then preserve the established layout with the `generic32` profile unless the user explicitly requests conversion to RMXP. Do not infer an RMXP layout from the filename or this skill's name.
- **New sheet:** determine the theme, style, and inventory. If unspecified, use the RMXP regular-tileset profile and the starter inventory below. Read [rmxp-tileset-spec.md](references/rmxp-tileset-spec.md) and follow the new-sheet workflow.

## New-sheet workflow

1. Establish the theme, style, and tile inventory.
2. Select the target profile from the user's engine and layout requirements.
3. Plan rows and coordinates before image generation. For `rmxp`, reserve cell `(0,0)` as fully transparent.
4. Generate source art with the image-generation tool. Prefer coherent batches by material or function, then isolate and repair individual modules. Do not trust the image model to produce exact sheet dimensions, grid placement, or perfect seams.
5. Inspect actual source dimensions and alpha, then normalize every module to dimensions that are exact multiples of 32px using the host's supported image-editing workflow. A requested size in the prompt is not proof of output size. Keep full-cell terrain opaque where it must tile; keep only unused sheet space and object surroundings transparent.
6. For repeatable textures, test opposite edges and repair visible seams before packing. Include required edge, corner, transition, and shadow variants rather than only center fills.
7. Write a JSON manifest and run `scripts/pack_tileset.py` to build the final PNG.
8. Run `scripts/validate_tileset.py` with the selected `--profile` on the result. Fix every error; treat warnings as a visual review checklist.
9. Inspect the PNG at 100% nearest-neighbor scale and as a repeated map mockup. Repair clipping, halos, inconsistent light direction, palette drift, duplicated silhouettes, and grid bleeding.
10. Deliver the final PNG, manifest, and a short tile-coordinate legend. Include a nearest-neighbor preview only when it helps review.

## Art-direction rules

- Use one camera angle, projection, palette, contrast range, outline policy, and light direction across the entire sheet.
- Design for readability at 32x32. Favor strong value groups and intentional clusters over high-frequency detail.
- Avoid text, labels, numbers, checkerboard transparency patterns, grid lines, mock UI, characters, and environmental backgrounds in source generations.
- Avoid photographic texture, antialiased fringes, semitransparent matte halos, and details that disappear after reduction.
- Generate multi-cell objects on transparent backgrounds with deliberate 32px anchors. Keep shadows on the same footprint policy across objects.
- Never remove the background from an opaque floor or wall fill merely to satisfy “transparent background.” Transparency applies to empty cells and the area around isolated objects.
- Never enlarge an existing building by scaling the whole image: repeat compatible wall, roof, or stair sections while keeping fixed-size construction details unchanged. Nearest-neighbor enlargement is suitable for inspection previews. For newly generated painted source reduction, downsample once, clean at 32px, then keep later preview scaling nearest-neighbor.

## Default starter inventory

When the user provides only a theme, create a compact but mappable set:

- 1 transparent reserved cell
- 3 seamless ground variants: base plus two subtle alternates
- wall face, wall top, left/right edges, inner/outer corners, and end caps
- floor-to-wall and material transitions needed by the chosen environment
- stairs or doorway where appropriate
- 6-10 small props with distinct silhouettes
- 1-3 multi-cell focal props when the theme benefits from them

Do not pad the sheet with near-duplicate decorative noise. Prefer a smaller complete grammar of map construction.

## Image-generation strategy

- Begin with a style anchor containing a few representative materials and props when no reference image is supplied.
- Generate each logical family separately: ground materials, structural modules, transitions, then props.
- Ask for orthographic/front-oblique game tiles, centered modules, clean alpha, consistent light, no cast-off canvas, and generous separation.
- Use editing passes for seam repair and style matching. Preserve approved modules as references across later batches.
- If a batch is too inconsistent to segment reliably, regenerate smaller batches instead of forcing poor crops into the sheet.

## Packing

Create a manifest like:

```json
{
  "profile": "rmxp",
  "tile_size": 32,
  "rows": 12,
  "items": [
    {"source": "tiles/stone_floor.png", "x": 1, "y": 0},
    {"source": "tiles/wall_block_2x2.png", "x": 0, "y": 2}
  ]
}
```

Each source must be RGBA PNG and have width and height divisible by 32. Coordinates are cell coordinates, not pixels. Source paths are relative to the manifest directory. Use resolved paths so execution works from any working directory:

```text
"<python>" "<skill-dir>/scripts/pack_tileset.py" "<output-dir>/manifest.json" "<output-dir>/tileset.png"
"<python>" "<skill-dir>/scripts/validate_tileset.py" "<output-dir>/tileset.png" --profile rmxp
```

Use `--allow-overwrite` only for an intentional repair layer. Never silently overlap modules.

For an existing layout or a requested custom 32px sheet, set `"profile": "generic32"` and an explicit positive integer `"columns"`; `"rows"` is also a positive integer. This profile has no reserved first cell. The `rmxp` profile defaults to eight columns and rejects any other explicit column count. Both profiles use `"tile_size": 32`.

Use `scripts/compose_regions.py` for pixel-level copying, translation, and repetition inside an existing structure; use the packer for complete grid-aligned modules. The existing-asset reference describes region composition and the accompanying coordinate map.

## Acceptance criteria

- PNG uses RGBA and real transparency.
- Canvas matches the agreed target dimensions, with width and height positive multiples of 32px. New `rmxp` sheets are exactly 256px wide and have a fully transparent cell `(0,0)`.
- Packed modules align to the 32px grid. Internal edits follow component pixel boundaries and preserve the agreed module anchors.
- No content spills into reserved or unintended cells.
- Seamless tiles repeat without visible borders in both axes.
- Structural variants can form legal corners and ends without improvised edits.
- Pixel density, outline treatment, palette, projection, and lighting remain consistent.
- Existing-asset edits preserve the measured wall thickness, column width, step height, and other fixed dimensions. Unchanged copied regions remain pixel-identical; any local generative repair is confined to the agreed region.
- Final delivery includes enough coordinate information to use the sheet immediately.

If the user explicitly requests RPG Maker VX/VX Ace or autotiles, do not guess the layout. Confirm the exact engine/version and create or use the appropriate profile before packing.
