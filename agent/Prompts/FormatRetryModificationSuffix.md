The user requested a blueprint modification.

- Prefer `patch_blueprint` with a JSON array of ops for small fixes.
- Use `replace_blueprint` with the **complete** blueprint JSON only for large structural changes.
- Do **not** respond with text only until a save succeeds.

Example patch op:

```json
{"op":"updateLink","event":"onOverlap","linkIndex":0,"rightInPin":0}
```
