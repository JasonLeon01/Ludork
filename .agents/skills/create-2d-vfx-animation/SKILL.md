---
name: create-2d-vfx-animation
description: "Create complete transparent 2D game VFX frame sequences in ChatGPT Work or Codex, with numbered PNGs, sprite sheets, animation previews, and manifests. Use for JRPG spells, impacts, slashes, bursts, auras, projectiles, and status effects. Excludes character walk cycles, skeletal animation, and video frame extraction."
---

# Create 2D VFX Animation

Build an actual frame sequence, not a single concept image or a video that is later sampled.

## Host, tools, and files

- Resolve this skill's directory from its loaded `SKILL.md` location. Resolve `scripts/` and `references/` relative to that directory, not the shell working directory. In a cloud host, materialize the bundled files with the host's skill-resource tools before running scripts; do not reuse a local Windows path.
- Use the current host's built-in image-generation/editing tool. If the `imagegen` skill is available, load its built-in workflow. Follow the actual tool schema for references and returned images; do not invent tool parameters or require a ChatGPT browser session. If generation is unavailable, report that prerequisite; do not silently switch to a paid API or require an API key.
- Use the user's output directory, or a new `output/vfx/<effect-name>/` directory under the task workspace. Keep a `frames/` subdirectory, a phase/filename checklist, and the applied prompts there. Do not write generated assets into this skill directory or import them into game data unless requested.
- Find a Python 3.10+ interpreter and verify Pillow using `"<python>" -c "from PIL import Image"`. Reuse an available environment; if Pillow is missing, install it into a task virtual environment with that interpreter's `-m pip install Pillow`. Do not assume `python3` exists on Windows. In PowerShell, invoke a quoted executable path with `&`.
- Copy each accepted image from the actual path or downloadable artifact returned by the image tool. Never guess a generated-image filename. If the host cannot expose image bytes, report the exact generated count and the missing export capability; an inline preview alone does not complete a file-delivery request.

## Non-negotiable completion contract

- Treat a request for `N` frames as a request for exactly `N` distinct final images.
- Generate one final frame per built-in image call, using the image tool exposed by the current host. Count saved, validated files rather than successful tool calls.
- Count a generated apex keyframe as one of the `N` frames only when it matches its assigned frame index. Generate the remaining `N - 1` frames before finishing.
- Keep a checklist of expected filenames and generated filenames. Do not enter the final response while any expected frame is missing.
- Do not pause for apex approval unless the user explicitly requests a review checkpoint or the request is genuinely ambiguous. The apex is normally an internal consistency reference.
- Do not present one keyframe as a finished animation. If a tool failure prevents completion, label the result incomplete and report the exact completed count, such as `4/12`.

## Default production profile

Use these defaults unless the user overrides them:

- one-shot effect, 12 frames at 12 fps
- 512 × 512 RGBA PNG frames
- front view, centered origin, fixed camera and fixed canvas
- transparent background with preserved alpha
- readable JRPG silhouette at gameplay scale
- filenames `frame_000.png` through `frame_011.png`
- deliver numbered frames, `spritesheet.png`, `preview.apng`, `preview.gif`, and `manifest.json`

Treat GIF as a lossy preview. Preserve PNG/APNG or the sprite sheet as the alpha-quality deliverable.

## Workflow

1. **Write the effect brief.** Record element, action, visual language, palette, viewpoint, dimensions, frame count, fps, loop behavior, anchor, and exclusions. If the request is usable, fill unspecified fields from the default profile instead of blocking.
2. **Create a style lock.** Define stable invariants: medium, stroke/edge character, detail density, glow behavior, palette, silhouette, canvas, and camera. When the user supplies a reference, label whether it controls style, shape, timing, or all three.
3. **Plan phases.** Read `references/effect-spec.md`. Assign every frame a phase and a visible change. Avoid equally spaced scale changes; emphasize anticipation, apex, and dissipation.
4. **Generate the apex frame as an internal checkpoint.** Assign it a real frame index, use the built-in image generation path, and request genuine transparency. Unless the user asked to choose a style variant, immediately continue instead of yielding the keyframe as the result.
5. **Generate every remaining frame.** Make separate image-generation calls until the filename checklist reaches `N/N`. Generate outward from the apex: neighboring frames first, then earlier and later frames. Use the apex and nearest accepted frame as references when possible. Never ask the image model to render a labeled grid or sprite sheet as the final asset.
6. **Lock invariants on every call.** Repeat the style lock, fixed canvas, fixed origin, transparent background, no text, no character, no environment, no border, and no camera change. Describe the exact visible state of that frame rather than saying only “next animation frame.”
7. **Review as a sequence.** Inspect silhouette, center of mass, scale, palette, alpha edges, energy continuity, and temporal spacing. Regenerate only the drifting frame with a single targeted correction. Do not repaint accepted frames merely to make minor differences disappear.
8. **Persist every frame.** After each call, copy the accepted image into `frames/` under its assigned filename and update the checklist. Inspect its actual dimensions and alpha before accepting it. A requested size in the prompt is not proof of output size. Use image-tool edits to fix the canvas or alpha. On resumption, inspect the saved checklist and files and generate only missing or rejected frames.
9. **Export deterministically.** After confirming `N/N` PNGs exist, run:

   ```text
   "<python>" "<skill-dir>/scripts/build_vfx_sheet.py" "<output-dir>/frames" --output-dir "<output-dir>" --expected-frames 12 --fps 12
   ```

   Replace `12` with the requested frame count and fps. Add `--loop` only for looping effects. The exporter checks consecutive filenames, includes the numbered frames in the output package, and records their paths relative to `manifest.json`.

10. **Report the production spec.** Return the completed frame count, fps, actual canvas, playback mode, alpha status, warnings, and links to the output directory and previews. Keep the phase map and applied prompts with the files. Use the host's artifact links in cloud Work and absolute file links for local outputs. Never report success unless the completed count equals the requested count.

## Image-generation rules

- Run the multi-frame request as one continuous work turn. Use commentary updates between calls when useful, but do not send a final response between frames.
- For a 12-frame request, plan 12 final image calls or one apex call plus 11 remaining frame calls. Do not confuse one image containing one effect with a frame sequence.
- For a frame edit, preserve all unmentioned pixels and invariants aggressively.
- Prefer actual adjacent-frame references over prose-only consistency claims.
- Keep the effect fully inside a stable safe area, normally 85% of the canvas.
- Do not bake checkerboards, black mattes, drop shadows from an imaginary floor, captions, frame numbers, or guides into an image.
- Do not claim transparency merely because the backdrop looks blank. Verify an alpha channel during export.
- If generated output lacks usable alpha, make one targeted transparent-background edit. If it still fails, report the limitation rather than silently using color-key transparency.

## Prompt construction

Read `references/prompt-recipes.md` before generating frames. Maintain two blocks:

- **Style lock:** identical across every call.
- **Frame delta:** unique phase, footprint, direction, opacity, fragments, and energy distribution for one frame.

For temporal consistency, describe conserved visual motifs such as the same clockwise spiral, three primary water ribbons, the same cyan core, or the same crescent direction. Let secondary droplets and wisps evolve.

## Quality gates

Reject or repair a frame when any of these occur:

- canvas size or origin changes
- background is opaque or contains a checkerboard
- effect is clipped unintentionally
- unrelated objects, characters, scenery, text, or borders appear
- palette or rendering medium changes noticeably
- silhouette jumps without a planned impact or smear frame
- the effect appears as a static image duplicated across frames
- loop endpoints disagree when a seamless loop was requested

Use the exporter warnings as diagnostics. A valid export does not guarantee good motion; always inspect the APNG or GIF preview.

## Resource routing

- Read `references/effect-spec.md` to design timing, phase allocation, looping, anchoring, and acceptance criteria.
- Read `references/prompt-recipes.md` to create the style lock, per-frame prompts, and repair prompts.
- Run `scripts/build_vfx_sheet.py` after frames exist to validate alpha/canvas consistency and build deliverables.
