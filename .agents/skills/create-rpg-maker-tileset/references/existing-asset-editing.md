# Understand and edit an existing tileset

Use this workflow for expansion, repair, or repacking of supplied art. Preserve the existing construction and pixel scale unless the user requests a redesign. Use `generic32` for the existing layout; converting it to an eight-column RMXP sheet is a separate task.

## Establish what the image contains

Open the original image with the host's image-viewing tool before editing. Inspect it at native size, then inspect nearest-neighbor detail views with a temporary 32px grid and contrasting backgrounds. Keep grids, labels, and review backgrounds out of the delivered PNG.

Record three kinds of evidence separately:

- **Observed structure:** identify complete building modules, isolated construction pieces, props, interior voids, walls, columns, doors, stairs, roofs, pediments, reliefs, shadows, and transparent spacing. Explain which parts belong together rather than treating every rectangular crop as an independent tile.
- **Measured constraints:** read the actual PNG dimensions, mode, alpha bounds, grid origin, and module rectangles. Measure representative opaque cross-sections of vertical walls and columns separately from shadows or antialiased fringes. Record stair rise, repeating texture pitch, door size, and anchor positions where they affect the requested edit.
- **Uncertainty:** distinguish likely decorative seams from structural boundaries, obscured parts from transparent space, and visual estimates from measured values. Resolve uncertainty with additional crops or the project's tile configuration. Ask only if the remaining interpretation materially changes the requested result.

Use the host model's visual reasoning for semantic interpretation, including GPT-6 when it is available. Numeric dimensions come from image inspection and measurement; a model's visual estimate or an image prompt is not a measurement. Do not introduce another model service or configuration to perform this step.

## Define the edit contract

Before composing, state the target in both cells and pixels. Clarify whether “expand by two cells” means two cells in total or two on each side when context does not settle it. Record:

- the affected modules and their source and destination rectangles;
- target canvas dimensions and any shifts needed for neighboring modules or loose props;
- fixed dimensions, such as wall thickness, column width, door opening, stair rise, trim, and texture scale;
- reusable straight runs or complete repeated pieces, plus parts that must move intact;
- local regions requiring newly generated art or seam repair.

For a larger wall enclosure, move corners and end pieces intact, then extend straight runs along their length. For a wider colonnade, add complete columns and compatible spacing. For deeper stairs, repeat the existing tread/riser rhythm and extend the side borders consistently. Keep a centered entrance centered in the enlarged module without widening its silhouette. Do not resize an entire building or stretch a decorative pediment to fill the target rectangle.

Prefer source-pixel composition when it meets the requested result and the user authorizes that editing method. Use the built-in image-editing tool for new details or local generative repair, following its actual reference-image interface. Inspect returned files at their actual size; an AI result is a candidate, not proof that dimensions or fixed details survived. Place an accepted repair only inside its planned region and preserve surrounding source pixels.

## Compose without resampling

`scripts/compose_regions.py` copies and repeats source rectangles at their native pixel size. It creates a transparent RGBA canvas, applies operations in order, and replaces each destination rectangle including transparent source pixels. Later operations can therefore erase earlier pixels; use overlap only intentionally. Source paths are relative to the manifest directory.

```json
{
  "tile_size": 32,
  "width": 320,
  "height": 64,
  "operations": [
    {
      "op": "copy",
      "source": "original.png",
      "source_rect": [0, 0, 32, 64],
      "dest_rect": [0, 0, 32, 64]
    },
    {
      "op": "repeat",
      "source": "original.png",
      "source_rect": [32, 0, 32, 64],
      "dest_rect": [32, 0, 288, 64]
    }
  ]
}
```

Rectangles are `[x, y, width, height]` in pixels. `copy` requires equal source and destination sizes. `repeat` tiles the source at its original size and crops any excess at the destination edge; choose source bounds whose repeated edges actually match. The canvas is aligned to 32px cells, while internal crops may follow a component's pixel boundary. No operation scales artwork.

```text
"<python>" "<skill-dir>/scripts/compose_regions.py" "<output-dir>/regions.json" "<output-dir>/candidate.png" --mapping "<output-dir>/mapping.json"
"<python>" "<skill-dir>/scripts/validate_tileset.py" "<output-dir>/candidate.png" --profile generic32
```

Use `scripts/pack_tileset.py` when placing complete modules whose bounds already align to the tile grid. Use the region composer for the internal construction of a module. An accepted AI patch can be another region source; do not substitute its whole sheet for the source art when only a local repair is intended.

## Keep project references consistent

When the task includes replacement in a project, inspect that project's tile size, tile indexing, collision/passability, directional flags, material data, and maps that reference the sheet. A wider canvas changes row-major indices. Move existing properties with their corresponding source cells and give added cells the properties of the corresponding repeated construction piece.

The optional region mapping contains `tile_size`, `columns`, `rows`, and `cells`. Each cell records `target_tile`, its `primary` source image and `source_tile`, and a `sources` count list. The primary source is selected by visible-pixel majority, or covered-pixel majority for a fully transparent cell. This assists migration but does not decide gameplay meaning. Review cells combining several source regions, newly generated patches, empty cells, and cells where a wall or doorway changes position. Do not assign gameplay properties solely from visible color or alpha. Follow the project's established defaults for unused cells.

## Acceptance

- Check the exact target size, RGBA encoding, alpha, grid, and absence of clipped or overlapping modules.
- Compare preserved regions pixel-for-pixel. Remeasure walls, columns, doors, and stairs using the same method as before the edit.
- Inspect native-size and enlarged views for repeated seams, texture phase, lighting, roof slope, relief proportions, halos, and stair continuity. View repeated terrain in a map-like arrangement when applicable.
- Check tile-property lengths and migrated indices after changing canvas width or module positions. Inspect the result in the target editor when available; script validation alone does not prove editor or gameplay acceptance.
- Validate the candidate before replacing the requested original. Keep the original available outside the repository during verification, preserve unrelated work, and remove task-created probes after checks. Deliver or link only the requested assets and useful review artifacts.
