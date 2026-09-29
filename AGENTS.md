# Documentation worktree

- This worktree owns `gh-pages`. Keep the business worktree on its current branch. Reuse this worktree; do not create another checkout or switch the business branch for documentation edits.
- Determine the target major.minor version from the business checkout's `versions.conf`. Read and edit the matching `<version>/docs` Markdown and `<version>/images`; update affected English and Chinese pages together after code changes and verification are complete.
- Preserve unrelated staged and unstaged changes. This workflow authorizes a scoped local documentation commit, including generated outputs, but never an automatic push or business-code commit.
- Read [README.md](README.md) for dependency setup, hook installation, routes and version additions. `__default__` contains frontend sources. Build entries, HTML and asset bundles are generated; edit their owning sources instead.
- Run frontend lint and `build_docs` for frontend changes; inspect affected pages for visible changes. Check Markdown links and images for content moves. The pre-commit hook checks that all build inputs are staged and adds generated outputs to the same commit; do not bypass it or absorb unrelated work to satisfy it.
- Keep generated files and the source files that produce them together in the documentation commit. Report the commit SHA and worktree path. Retain this worktree for reuse.
