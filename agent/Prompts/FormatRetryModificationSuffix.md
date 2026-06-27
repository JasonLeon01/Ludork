The user requested a blueprint modification.

- Prefer type `"patchblueprint"` with a JSON array of ops for small fixes.
- Use type `"replacefile"` with the **complete** blueprint JSON only for large structural changes.
- Do **not** use type `"reply"` until a save succeeds.

Example patch:

```json
{"message":"Fixed pins.","type":"patchblueprint","terminal":[{"op":"updateLink","event":"onOverlap","linkIndex":0,"rightInPin":0}]}
```
