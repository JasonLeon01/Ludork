# RPG Maker XP regular tileset specification

Use this reference for newly created ordinary RMXP map tilesets or an explicitly requested conversion to that format. Autotiles are separate resources and use a different layout. Existing custom 32px sheets retain their established layout; inspect them using [existing-asset-editing.md](existing-asset-editing.md).

## Canvas and grid

- File format: PNG with alpha.
- Logical tile size: 32x32 pixels.
- Sheet width: exactly 256 pixels, or 8 tile columns.
- Sheet height: any positive multiple of 32 pixels.
- Top-left cell `(0,0)`: fully transparent. RPG Maker XP uses it as the empty tile.
- All tile and multi-cell object bounds must align to the 32px grid.
- Keep unused cells fully transparent.

## Transparency semantics

“Transparent background” does not mean every tile must contain alpha. A seamless floor normally fills all 32x32 pixels. A tree, sign, arch, column, or other isolated object normally has transparent pixels around its silhouette. Empty cells must be fully transparent.

Avoid RGB matte colors in pixels whose alpha is zero when possible. They can produce halos in some tools after filtering.

## Practical row planning

RPG Maker XP does not assign a universal semantic meaning to each row of a regular tileset, so plan a stable project convention. A useful order is:

1. reserved empty cell and base ground materials
2. ground variants and transitions
3. wall tops and wall faces
4. wall edges, corners, caps, doorways, and stairs
5. small props
6. multi-cell structures and focal props

Record coordinates as zero-based `(x,y)` cells in a legend or manifest.

## Existing and custom 32px sheets

The `generic32` profile accepts positive canvas dimensions divisible by 32, without requiring 256px width or an empty first cell. In a packing manifest, specify `"profile": "generic32"`, `"tile_size": 32`, and positive integer `"columns"` and `"rows"`. The output size is `columns * 32` by `rows * 32`. Validate it using `--profile generic32`.

This profile checks a 32px grid; it does not certify compatibility with every RPG Maker version. Follow the target project's actual image dimensions and tile-property indexing. Expanding the sheet width changes row-major tile indices even for artwork that stays at the same pixel coordinates.

## Visual checks

- Repeat seamless ground tiles in a 3x3 block and inspect all seams.
- Assemble wall pieces into rectangles, L-shapes, inner corners, and corridor ends.
- View at 100% with nearest-neighbor sampling.
- Test isolated objects on both light and dark representative floors.
- Check that shadows share one light direction and do not cross unintended cell bounds.
- Check every alpha edge for light or dark matte halos.

## Out of scope

- RMXP autotiles use a separate quadrant-based layout and require their own topology and animation validation.
- RPG Maker VX and VX Ace use fixed sheet families and layouts that differ from RMXP.
- RPG Maker MV and MZ normally use 48px tiles, not this 32px profile.
