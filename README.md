# Ludork website and documentation

This orphan `gh-pages` branch owns the website, versioned Markdown and images. The editor and engine remain on `main`. Use a separate worktree and keep it for later documentation tasks.

## Develop and publish

Use Node.js matching `__default__/package.json`, then run from this worktree:

```sh
npm ci --prefix __default__
git config --local core.hooksPath .githooks
npm run dev --prefix __default__
```

Build with `sh build_docs.sh` on macOS/Linux or `build_docs.bat` on Windows. Both call `build_docs.mjs`; generated pages and assets live at the branch root. Never edit them by hand. `npm run preview --prefix __default__` previews the build and serves versioned content from this worktree.

Stage the intended documentation and frontend changes before committing. On `gh-pages`, the pre-commit hook builds the site and stages only its generated outputs into the same commit. A failed build or unstaged/untracked build input stops the commit. It does not commit unrelated files, create another commit or push. Configure the hook in each new clone; this repository's `main` worktree uses its own `.githooks/pre-commit` for the existing code check.

Review and push `gh-pages` yourself. In GitHub Pages, select **Deploy from a branch**, **gh-pages**, **/(root)**. GitHub serves the committed static files; this branch has no custom build workflow. GitHub's web editor does not run local hooks, so build and commit locally before publishing frontend or document-tree changes.

## Content and routes

- `versions.json` lists available major.minor versions, newest/default first; currently `1.0`. Each version contains `docs/en_GB`, `docs/zh_CN`, shared About/notices Markdown, and `images`.
- `/Ludork/docs/v1.0/` is the documentation entry. `lang`, `doc`, `path` and heading fragments select its language and content. `path` is relative to the version directory, such as `docs/en_GB/Ludork Documentation.md`.
- Each language root has an `order.json` with `home` and ordered `sections`; each top-level chapter has an `order.json` with an ordered `items` tree. Every node has a fixed `key` and a `path` relative to its parent directory; folder nodes in the tree contain `children`. File and directory names have no ordering prefixes. Change array order to reorder the navigation, and preserve keys when renaming or reordering content. Matching English and Chinese pages and folders use the same keys; existing `doc=01/03` links remain valid.
- The pale-blue heading navigator at the bottom right links to the current page's subheadings. It starts collapsed on mobile; use its arrow to open the scrollable list.
- `/Ludork/`, `/Ludork/about/` and `/Ludork/notices/` are the website entries. About and notices accept `version=1.0`; without it they use the default version.
- `embedded=1` hides the website header and, on About/notices, its footer. Language/version navigation preserves the parameter. The editor supplies its current language and major.minor version.
- To add a version, add its content directory and entry in `versions.json`; the build derives the documentation HTML entry and language manifests. Vite owns generated source entries under `__default__/docs/v*/` and publishes their compiled output under `docs/v*/`.

## 中文

此 orphan `gh-pages` 分支保存官网、各版本 Markdown 和图片，编辑器与引擎代码位于 `main`。使用独立 worktree，并保留以供后续复用。

新 clone 先执行上方依赖安装及 hook 配置命令。Windows 使用 `build_docs.bat`，macOS/Linux 使用 `sh build_docs.sh` 构建；不要手改根目录生成的页面和资源。先暂存本次修改，再提交：hook 会构建并把生成文件纳入同一次提交；构建失败或存在未暂存、未跟踪的构建输入时停止提交，不自动 push。

自行 review 并 push 后，在 GitHub Pages 选择 **Deploy from a branch → gh-pages → /(root)**。无需自定义构建 CI。GitHub 网页编辑不会执行本地 hook，前端或文档目录变更需要在本地构建后提交。

`versions.json` 按默认版本优先列出版本，目前只有 `1.0`；内容位于 `1.0/docs`，图片位于 `1.0/images`。新增版本时增加目录和版本项即可生成对应入口。文档路径为 `/Ludork/docs/v1.0/`，支持语言、章节、文件路径和标题锚点；关于和声明通过 `version=1.0` 指定版本。`embedded=1` 隐藏官网导航，供编辑器内嵌网页使用。

各语言根目录的 `order.json` 使用 `home` 指定首页，使用有序的 `sections` 列出顶级章节；每个顶级章节的 `order.json` 使用有序的 `items` 树维护其完整目录。每个节点包含固定的 `key` 和相对于父目录的 `path`，树中的目录节点还包含 `children`。文件名和目录名不带排序前缀，调整数组顺序即可调整导航顺序；重命名或排序时保留原有 `key`，中英文对应页面和目录使用相同的 `key`，现有 `doc=01/03` 链接仍然有效。

右下角的淡蓝色小标题导航可跳转到当前页的小标题；移动端默认收起，点击箭头展开后可滑动列表。
