The user requested a blueprint modification but no `patch_blueprint` or `replace_blueprint` has been executed yet.

- Do **not** respond with text only to claim changes were made.
- For small fixes use `patch_blueprint` with a JSON array of ops.
- For large restructuring use `replace_blueprint` with the **complete** updated blueprint JSON.

For `GetPlayer` + `setMoveEnabled`:

- `params` must be `["", true/false]` (self slot + enabled).
- Params link `rightInPin` 0 = self/actor, `rightInPin` 1 = enabled.
- **Never** use `["true"]` alone.
