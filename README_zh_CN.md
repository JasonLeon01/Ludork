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

两个构建脚本均调用 `build_docs.mjs`。修改 `__default__` 中的前端源码及 `trunk` 中的开发文档，已发布的快照保持不变。不要手改生成的 HTML、资源或搜索索引。

创建发布快照时，在 Windows 上执行 `build_docs_release.bat 1.0`，在 macOS/Linux 上执行 `sh build_docs_release.sh 1.0`。这两个入口调用 `build_docs_release.mjs`，只接受一个 `major.minor` 版本号，主、次版本均为不带前导零的十进制整数，不带 `v` 前缀或补丁版本。脚本先规范化 trunk 文本，再将整个 `trunk` 目录复制到新版本目录，写入 `versions.json`，然后构建站点。所有语言、导航、图片、关于及声明均包含在快照中。快照源只允许普通文件和目录，不接受符号链接。目标目录或版本项已存在时会报错，不允许覆盖快照；复制或构建失败时，仅删除本次新建的快照并恢复版本清单。此命令在发布时手动执行，不会暂存、提交或推送改动。

创建快照后，后续内容修改仍全部在 `trunk` 中进行。之后的站点构建只读取已发布内容，不会修改它们；共用前端、HTML 入口及生成的索引仍会为所有版本重新构建。

本分支的文本文件统一使用 LF，由 `.gitattributes` 固定 Git 检出时的换行格式。`build_docs.mjs` 在读取 `versions.json`、检查提交输入和启动 Vite 之前，会移除 trunk 文档、前端及构建文本输入开头的 UTF-8 字节顺序标记（BOM），并将 CRLF 和单独的 CR 原地转换为 LF，避免这些编码与换行差异改变资源哈希。只有需要转换的文件才会写回，冻结的发布目录、依赖、生成目录和二进制资源不参与转换。

提交前暂存本次修改的输入文件。`gh-pages` 的 pre-commit hook 会构建站点，并把生成文件纳入同一次提交；构建失败或存在未暂存、未跟踪的构建输入时，提交会停止。每次 clone 后都需要配置 hook。

检查后自行 push `gh-pages`，在 GitHub Pages 中选择 **Deploy from a branch → gh-pages → /(root)**，无需自定义构建工作流。GitHub 网页编辑不会执行本地 hook，因此发布前应在本地构建并提交。

## 内容与路由

- [`versions.json`](versions.json) 列出 `trunk` 和已有的主、次版本发布快照。正式版本按主、次版本的数值倒序显示，`trunk（开发中）` 固定在末尾。默认使用最新正式版；尚无正式版时使用 trunk。每个版本包含 `docs/en_GB`、`docs/zh_CN`、共用的关于与声明 Markdown，以及 `images`；构建会生成对应的 HTML 入口及语言 manifest。
- 各语言根目录的 `order.json` 定义首页 Markdown 条目 `home`、FAQ 树根 `faq` 和有序的 `sections`。FAQ 使用 `{"key":"faq","path":"FAQ"}`，其目录下的 `order.json` 用有序的 `items` 树定义场景子页，使用 `faq/projects` 等 key。各顶级章节也通过 `order.json` 中有序的 `items` 树定义目录。节点包含固定的 `key` 和相对父目录的 `path`，树中的目录节点还包含 `children`。
- 每个版本、每种语言分别维护自己的 `docs/<语言>/FAQ/` 场景子页。FAQ 内容不跨版本共用，内部链接指向相同版本、相同语言的文档。
- 通过数组顺序调整导航，文件名无需排序前缀。重命名或排序时保留原有 key，中英文对应内容使用相同的 key，以保持现有 `doc` 链接有效。

| 路由或参数 | 用途 |
| --- | --- |
| `/Ludork/docs/trunk/` | 开发文档入口；通过 `lang`、`doc`、`path` 和标题锚点选择内容。`path` 相对于版本目录。 |
| `/Ludork/docs/v1.0/` | 冻结的 `1.0` 文档，创建对应快照后可用，内容参数与开发文档相同。 |
| `/Ludork/docs/trunk/?lang=zh_CN&doc=faq/projects` | 所选版本的“项目创建与升级”FAQ 页面；使用各场景的 key 选择对应子页。 |
| `/Ludork/`、`/Ludork/about/`、`/Ludork/notices/` | 官网入口；关于和声明支持 `version=trunk` 或 `version=1.0` 等发布版本，省略时使用最新正式版；尚无正式版时使用 trunk。 |
| `embedded=1` | 隐藏官网页头及关于、声明页的页脚；语言和版本切换保留此参数。 |

编辑器打开内嵌页面时，会传入当前语言。开发版选择 trunk，正式版选择与自身主、次版本号匹配的快照。

文档侧栏提供文档首页入口与可展开的 FAQ 节点，FAQ 的子节点按使用场景分类。版本选择下方的标题筛选只缩小所选版本、当前语言的章节树范围，不搜索 FAQ 或文档正文。原有章节标签页改为章节树的第一级节点。

## 文档搜索索引

构建会为开发文档生成 `/Ludork/docs/trunk/search-index.{language}.json`，为发布快照生成 `/Ludork/docs/v{version}/search-index.{language}.json`，供程序查询，结构如下：

```text
{ version, language, documents: [{ key, title, path, content }] }
```

开发文档的 `version` 为 `"trunk"`，发布快照则使用对应的主、次版本号。索引按导航顺序包含首页、全部 FAQ 场景子页和全部章节文档。`title` 是去掉 `.md` 的文件名，`path` 相对于版本目录，`content` 是统一使用 LF 换行的完整 Markdown。查询时显式选择所需版本与语言。索引在发布后可在线访问，与侧栏的标题筛选相互独立。
