# Ludork website and documentation

English | [简体中文](README_zh_CN.md)

The `gh-pages` branch owns the website, versioned Markdown and images. Keep it in a separate worktree; editor and engine sources remain on `main`.

## Develop and publish

Use the Node.js version required by [`__default__/package.json`](__default__/package.json), then run:

```sh
npm ci --prefix __default__
git config --local core.hooksPath .githooks
npm run dev --prefix __default__
```

| Task | Command |
| --- | --- |
| Build on Windows | `build_docs.bat` |
| Build on macOS/Linux | `sh build_docs.sh` |
| Preview the build | `npm run preview --prefix __default__` |

Both build scripts call `build_docs.mjs`. Edit frontend sources in `__default__` and versioned content, not generated HTML, assets or search indexes.

Stage the intended inputs before committing. The `gh-pages` pre-commit hook builds the site and includes generated outputs in the same commit; build failures or unstaged/untracked build inputs stop the commit. Configure the hook in each clone.

After review, push `gh-pages` and set GitHub Pages to **Deploy from a branch → gh-pages → /(root)**. No custom build workflow is needed. GitHub's web editor does not run local hooks, so build and commit changes locally before publishing.

## Content and routes

- [`versions.json`](versions.json) lists major.minor versions, default first. Each version contains `docs/en_GB`, `docs/zh_CN`, shared About/notices Markdown and `images`. Add a content directory and version entry to introduce a version; the build generates its HTML entry and language manifests.
- Each language's root `order.json` defines the `home` Markdown entry, the `faq` tree root and ordered `sections`. FAQ uses `{"key":"faq","path":"FAQ"}`; its directory's `order.json` defines the scenario pages in an ordered `items` tree, using keys such as `faq/projects`. Each top-level chapter also has an `order.json` with an ordered `items` tree. Nodes have a stable `key`, a `path` relative to their parent directory, and `children` for folders in the tree.
- Each version and language owns its own `docs/<language>/FAQ/` scenario pages. FAQ content is not shared across versions; its links point to the documentation in the same version and language.
- Use array order for navigation; filenames need no ordering prefixes. Preserve keys when renaming or reordering, and use the same keys for matching English and Chinese content so existing `doc` links stay valid.

| Route or parameter | Purpose |
| --- | --- |
| `/Ludork/docs/v1.0/` | Documentation; `lang`, `doc`, `path` and heading fragments select content. `path` is relative to the version directory. |
| `/Ludork/docs/v1.0/?lang=en_GB&doc=faq/projects` | The selected version's Projects and Upgrades FAQ page; FAQ scenario keys select individual pages. |
| `/Ludork/`, `/Ludork/about/`, `/Ludork/notices/` | Website entries; About and notices accept `version=1.0`, defaulting to the first version. |
| `embedded=1` | Hides the website header and the About/notices footer; language and version navigation preserve it. |

The editor supplies its language and major.minor version when opening embedded pages.

The documentation sidebar provides a Documentation link and an expandable FAQ node with scenario pages. Below the version selector, the title filter narrows the current language's chapter tree within the selected version; it does not search FAQ or document contents. The former chapter tabs are the chapter tree's first-level nodes.

## Documentation search index

The build generates `/Ludork/docs/v{version}/search-index.{language}.json` for programmatic queries, using this schema:

```text
{ version, language, documents: [{ key, title, path, content }] }
```

It includes the home page, all FAQ scenario pages and all chapter documents in navigation order. `title` is the filename without `.md`, `path` is relative to the version directory, and `content` is the full Markdown with LF line endings. Select the required version and language explicitly. The index becomes available online after publishing and is independent of the sidebar's title filter.
