# Ludork 官网与文档

[English](README.md) | 简体中文

`gh-pages` 分支保存官网、各版本 Markdown 和图片，使用独立 worktree；编辑器与引擎源码保留在 `main`。

## 开发与发布

使用 [`__default__/package.json`](__default__/package.json) 要求的 Node.js 版本，然后执行：

```sh
npm ci --prefix __default__
git config --local core.hooksPath .githooks
npm run dev --prefix __default__
```

| 操作 | 命令 |
| --- | --- |
| Windows 构建 | `build_docs.bat` |
| macOS/Linux 构建 | `sh build_docs.sh` |
| 预览构建结果 | `npm run preview --prefix __default__` |

两个构建脚本均调用 `build_docs.mjs`。修改 `__default__` 中的前端源码及各版本内容，不要手改生成的 HTML、资源或搜索索引。

提交前暂存本次修改的输入文件。`gh-pages` 的 pre-commit hook 会构建站点，并把生成文件纳入同一次提交；构建失败或存在未暂存、未跟踪的构建输入时，提交会停止。每次 clone 后都需要配置 hook。

检查后自行 push `gh-pages`，在 GitHub Pages 中选择 **Deploy from a branch → gh-pages → /(root)**，无需自定义构建工作流。GitHub 网页编辑不会执行本地 hook，因此发布前应在本地构建并提交。

## 内容与路由

- [`versions.json`](versions.json) 列出主、次版本号，默认版本排在首位。每个版本包含 `docs/en_GB`、`docs/zh_CN`、共用的关于与声明 Markdown，以及 `images`。新增版本时添加内容目录和版本项，构建会生成对应的 HTML 入口及语言 manifest。
- 各语言根目录的 `order.json` 定义 `home` 和有序的 `sections`；各顶级章节的 `order.json` 用有序的 `items` 树定义目录。节点包含固定的 `key` 和相对父目录的 `path`，树中的目录节点还包含 `children`。
- 通过数组顺序调整导航，文件名无需排序前缀。重命名或排序时保留原有 key，中英文对应内容使用相同的 key，以保持现有 `doc` 链接有效。

| 路由或参数 | 用途 |
| --- | --- |
| `/Ludork/docs/v1.0/` | 文档入口；通过 `lang`、`doc`、`path` 和标题锚点选择内容。`path` 相对于版本目录。 |
| `/Ludork/`、`/Ludork/about/`、`/Ludork/notices/` | 官网入口；关于和声明支持 `version=1.0`，省略时使用首个版本。 |
| `embedded=1` | 隐藏官网页头及关于、声明页的页脚；语言和版本切换保留此参数。 |

编辑器打开内嵌页面时，会传入当前语言及主、次版本号。

## 文档搜索索引

构建会生成供程序查询的 `/Ludork/docs/v{version}/search-index.{language}.json`，结构如下：

```text
{ version, language, documents: [{ key, title, path, content }] }
```

索引按导航顺序包含首页和全部文档。`title` 是去掉 `.md` 的文件名，`path` 相对于版本目录，`content` 是统一使用 LF 换行的完整 Markdown。查询时显式选择所需版本与语言。索引在发布后可在线访问，不增加网站搜索界面。
