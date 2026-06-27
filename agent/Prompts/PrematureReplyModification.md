The user requested a blueprint modification but no `patchblueprint` or `replacefile` has been executed yet.

- Do **not** use type `"reply"` to claim changes were made.
- For small fixes use type `"patchblueprint"` with a JSON array of ops.
- For large restructuring use type `"replacefile"` with the **complete** updated blueprint JSON.

For `GetPlayer` + `setMoveEnabled`:

- `params` must be `["", true/false]` (self slot + enabled).
- Params link `rightInPin` 0 = self/actor, `rightInPin` 1 = enabled.
- **Never** use `["true"]` alone.
