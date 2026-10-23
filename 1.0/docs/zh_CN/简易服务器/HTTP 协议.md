# 服务端 Lua 与协议

Node.js 负责连接、项目认证、存储和进程管理，项目 Lua 5.4.5 模块实现显式快照、void RPC 与命名事件。每个项目运行在独立 Wasmoon 进程中，服务端没有游戏客户端的 Standard 扩展和原生模块。客户端 proxy 用法见[原生客户端 API](<原生客户端 API.md>)，项目入口路径见[部署与编辑器设置](<部署与编辑器设置.md#项目-lua-入口>)。

## 服务端 Lua 模块

项目入口从 `lua/` 加载框架模块，从项目自己的私有目录加载业务模块，并显式注册：

```lua
local Net = require("Framework.Net")
local Storage = require("Modules.Storage")
local Profile = require("Profile")

Net.RegisterModule("Storage", Storage)
Net.RegisterModule("Profile", Profile)
```

项目中的 `Profile.lua` 可以同时定义读取和写入：

```lua
local Profile = {Rpc = {}}

function Profile.Snapshot(context, params)
    if type(params) ~= "table" or type(params.account) ~= "string" then
        error({code = "InvalidArgument", message = "An account is required."})
    end
    local field = context.Storage:ReadField(params.account, "profile", "name")
    return {name = field.value or "New player"}
end

function Profile.Rpc.SetName(context, account, name)
    if type(name) ~= "string" or #name > 80 then
        error({code = "InvalidArgument", message = "Name must be at most 80 bytes."})
    end
    context.Storage:WriteField(account, "profile", "name", name)
    context:Notify("NameChanged", account)
end

return Profile
```

模块、RPC 与事件名区分大小写，由 1–128 个 ASCII 字母、数字、点、下划线或连字符组成。重复注册导致初始化失败。只有放入模块 `Rpc` 表的函数才能被远程调用；不提供某类操作时，可以省略 `Snapshot` 或 `Rpc`。

`Snapshot(context, params)` 接收一个纯 Lua 值，必须返回普通 table。每次获取都会重新调用它。该处理函数只能读取存储，不能发送事件。服务端不持有客户端 proxy，也不会自动复制数据变化。

`Rpc.FunctionName(context, ...)` 按位置接收纯 Lua 参数，包括中间和末尾的 nil。RPC 为 void，返回值会被丢弃。同一连接的调用保持发送顺序，每个项目串行处理其 handler。宿主存储调用会挂起当前协程，直到 Node 完成，因此可用顺序 Lua 代码编写逻辑。不要保留 context 或在 handler 返回后使用其服务。

| Context 成员 | 含义 |
| --- | --- |
| `ProjectName`、`ModuleName` | 当前项目与已注册模块。 |
| `SessionId` | 当前在线连接的不透明 ID。 |
| `PeerAddress` | 实际 TCP 对端地址，忽略转发请求头。 |
| `Storage` | 限定在当前项目内的存储服务。 |
| `Notify(event, ...)` | 为当前调用者排入一个事件。 |
| `Send(sessionId, event, ...)` | 为本项目的一个在线连接排入事件。 |
| `Broadcast(event, ...)` | 为本项目所有在线连接排入事件。 |
| `AdmitWrite(account, category, field)` | 仅在随附 `Storage.WriteField` RPC 内应用配置的间隔。 |

Context 方法使用 `:` 调用。事件携带当前模块名，仅在 handler 成功后发送；失败会丢弃它排入的事件，但不会回滚该 handler 已完成的存储写入。断开的接收者没有事件积压。事件不会刷新客户端 proxy 数据。

普通 Lua 错误使请求以 `ServerFailure` 失败。业务可以抛出包含字符串 `code`、`message` 以及可选正整数 `retryAfterSeconds` 的 table。错误码以 ASCII 字母开头，后接至多 63 个 ASCII 字母或数字；不合法的错误码会变为 `ServerFailure`。宿主错误保留稳定错误码。参数校验和账号授权仍由项目逻辑负责：共享项目密钥不认证具体账号 ID。

每个项目的 Lua 分配器上限为 64 MiB，Lua 执行中断预算为九秒，整个调用期限为十秒，包括排队时间。进程崩溃、执行中的调用超时或内存故障会使该项目连接失效并重启 Lua 状态；此前未完成的调用失败，不会重放。恢复期间的新请求在有界队列内等待新状态就绪，仍受原请求期限约束。其他项目、健康检测与管理服务仍可用。入口无法加载时，仅该项目请求失败，直到修正并重启。服务端逻辑不能使用 `io`、`os`、`debug`、`load`、`loadfile`、`dofile` 或加载原生库，持久化须通过 context 完成。

## 存储服务

项目、账号和分类的文件系统名称采用小写十六进制 UTF-8 编码，保留大小写差异并避免保留文件名与路径越界。账号、分类、字段名允许 1–64 个 UTF-8 字节，不含控制字符，也不能是完整的 `.` 或 `..`。账号／分类／字段名可包含斜杠，项目名不允许。账号 ID 由游戏逻辑提供；服务端不生成 UUID、不预留 ID，也不认证账号。

每个分类是数据型 `.ldata` 文件，外层为 `{version:1,data:WireValue}`。根值是以字段名为键的 table，采用下方的[带类型标签编码](<#lua-数据与传输编码>)，文件从不作为 Lua 执行。已有 `.json` 分类文件不会被读取或转换，不提供兼容读取或自动迁移。

| `context.Storage` 方法 | 结果 |
| --- | --- |
| `AccountExists(account)` | 布尔值：账号目录是否存在，空目录也算。 |
| `ListAccounts(sampleCount, categoryFilter)` | 符合条件的账号名数组。 |
| `ReadField(account, category, field)` | `{found = boolean, value = value}`；缺失字段为 `found = false`，value 为 nil。 |
| `ListFields(account, category, beforeKey, limit)` | `{entries = {{key = key, value = value}, ...}, nextCursor = cursor}`。 |
| `WriteField(account, category, field, value)` | nil；替换字段，value 为 nil 时删除字段。 |

这些方法使用 `:` 调用。读取不创建账号。账号／分类缺失时返回空列表或字段缺失结果；文件系统检查失败、数据损坏返回 `StorageFailure`，不会伪装成不存在。

`sampleCount` 接受非负有符号 64 位整数。Nil 或零按 UTF-8 字节升序返回全部匹配账号；正数按随机顺序无放回抽样，最多返回符合条件的账号数。`categoryFilter` 在抽样前检查普通 `.ldata` 文件是否存在，不校验文件内容。账号列表没有游标；完整响应超过消息上限时，应减小抽样数量。

字段按 UTF-8 字节降序排列，与区域设置或数值含义无关。Nil `beforeKey` 表示首页；指定键是排他的上界，不要求实际存在。`limit` 默认为 50，接受 1–100 的整数。还有匹配字段时，`nextCursor` 是本页最后一个键；否则为 nil。空范围返回空 entries 和 nil 游标。每次分页读取当前文件，不提供跨请求快照。若要纳入新插入且大于游标的键，应从首页刷新；需要字节顺序表示时间时，业务可使用固定宽度时间戳。

写入按需创建账号与分类。Nil 删除字段，但保留分类文件与账号目录。同一文件的更新串行执行，通过原子替换临时文件保存；不同文件可独立更新。每个完整分类文件上限为 **1 MiB**，包括编码开销与末尾换行。不提供跨文件事务、自动备份，也不会在后续语句失败时回滚之前的写入。超时或响应丢失可能发生在写入已完成之后。

随附 `Modules.Storage` 模块提供这些快照查询：

| `params.query` | 参数 | 快照 |
| --- | --- | --- |
| `"account_exists"` | `account` | `{exists = boolean}` |
| `"accounts"` | 可选 `sampleCount`、`categoryFilter` | `{accounts = {...}}` |
| `"field"` | `account`、`category`、`field` | `{found = boolean, value = value}` |
| `"fields"` | `account`、`category`；可选 `beforeKey`、`limit` | `{entries = {...}, nextCursor = cursor}` |

它的 `WriteField(account, category, field, value)` RPC 应用配置的间隔、写入字段，再向调用者发送 `FieldWritten(account, category, field)`。自定义模块直接调用相同的存储服务，并实现自己的业务规则。

### 写入限频

`ipWriteIntervalSeconds` 默认 **180 秒**，仅用于随附的 `Storage.WriteField` RPC，设置方式见[配置写入间隔](<部署与编辑器设置.md#配置写入间隔>)。同一 TCP 客户端 IP 在该项目不同账号／分类上的调用共用间隔。IPv4 映射的 IPv6 地址统一为 IPv4，忽略 `X-Forwarded-For`。同一 NAT 或反向代理后的客户端可能共用额度，不同项目相互独立。

准入后在存储前开始计时，后续存储失败仍消耗额度。拒绝不会延长原截止时间。`RateLimited` 携带正数 `retryAfterSeconds`，向上取整到秒。截止时间使用单调时钟，服务重启后清空。读取、健康检测、管理和自定义 RPC 不消耗此额度。自定义 RPC 不自动限频，也不能通过 `AdmitWrite` 消耗这个内置间隔。

## Lua 数据与传输编码

参数、快照、事件和存储值接受 nil、布尔值、有限数字、字节字符串与普通无环 table。整数保留完整有符号 64 位范围。Table 可具有混合或稀疏的布尔、数字和字符串键。Nil 是合法的位置参数；在 table 中表示没有该项。空 table 保持为 table。

函数、线程、userdata、元表、循环引用和 table 键会被拒绝。无环 table 的重复引用编码为独立副本，不保留对象身份。这些规则也排除了 Ludork 类实例与原生容器。

帧内容是 UTF-8 JSON，每个 Lua 值用一种带标签的数组表示：

```text
["nil"]
["bool", true]
["int", "9223372036854775807"]
["float", "1.2345678901234567"]
["string", "Ynl0ZXM="]
["table", [[WireKey, WireValue], ...]]
```

整数使用精确十进制文本，浮点数使用能够往返还原的有限双精度十进制文本。字符串用规范 base64 编码任意字节。`WireKey` 仅接受布尔、整数、浮点数和字符串形式。不允许等价键重复，例如整数 `1` 与浮点数 `1.0`；值为 nil 的 table 项不编码。

消息上限为 **2 MiB**，数据最多 **64 层嵌套**、**65,536 个值节点**。RPC／事件参数数组保留每个 nil 位置。上限按编码后的消息计算，因此包含 base64 和结构开销。

## WebSocket 消息

连接到 `ws://主机:7777/<URL编码后的项目名>/api/v1/net`，在 HTTP upgrade 中携带 `X-Ludork-Key`。服务端先发送 `{"type":"ready","version":1}`，再处理应用消息。凭据不出现在 URL 或帧内容中。不使用二进制帧与消息压缩。

```text
snapshot: {type:"snapshot", id:"1", module:"Profile", params:WireValue}
RPC:      {type:"rpc", id:"2", module:"Profile", name:"SetName", args:[WireValue,...]}
success:  {type:"result", id:"1", ok:true, data:WireValue}
RPC ack:  {type:"result", id:"2", ok:true}
failure:  {type:"result", id:"2", ok:false, code:"InvalidArgument", message:"..."}
event:    {type:"event", module:"Profile", event:"NameChanged", args:[WireValue,...]}
```

请求 ID 由 1–128 个 ASCII 字母、数字、点、下划线或连字符组成，在同一连接的未完成请求中必须唯一。重复的未完成 ID 会关闭连接。限频失败可能包含 `retryAfterSeconds`。RPC 确认只表示执行成功，没有业务返回值；服务端事件在成功确认之后发送。

每条连接及每个项目的待处理队列上限均为 256 次调用、8 MiB。每条连接的发送缓冲上限为 8 MiB，慢连接会被关闭。单个 handler 最多排入 256 个事件、8 MiB 编码事件消息。Ping/pong 检查会清理失效连接。客户端超时、取消和恢复规则见[调用 RPC 与接收事件](<原生客户端 API.md#调用-rpc-与接收事件>)。

## HTTP 健康检测与只读管理

HTTP API 基地址仍为 `http://主机:7777/<项目名>/api/v1`。`GET /health` 无需密钥，返回 `{"service":"LudorkServer","project":"MyGame"}`。数据查询和写入使用 WebSocket 模块 API，不提供账号／字段 REST 或写入 token 接口。

管理路径位于 `/admin` 下。3333 端口的生产 Node.js 服务提供构建后的网页，并将网页的同源 API 请求代理到 7777；后端执行认证，不启用 CORS。

| `/admin` 之后的方法与路径 | 要求 | 成功响应 |
| --- | --- | --- |
| `POST /login` | JSON `{"key":"...","password":"..."}` 与 `X-Ludork-Admin: 1` | `{"project":"MyGame"}`，并设置会话 Cookie。 |
| `GET /session` | 会话 Cookie | `{"project":"MyGame"}` |
| `POST /logout` | 会话 Cookie 与 `X-Ludork-Admin: 1` | `{}`，清除 Cookie。 |
| `GET /accounts` | 会话 Cookie | `{"accounts":["player-1"]}` |
| `GET /accounts/:account/categories` | 会话 Cookie | `{"categories":["profile"]}` |
| `GET /accounts/:account/categories/:category` | 会话 Cookie | 分类 table 的 `{value:WireValue}`。 |

每个路径参数单独编码为一个 URL 段。会话绑定项目，八小时后过期，退出登录或后端重启后失效。Cookie 为 HttpOnly、SameSite=Strict，作用域限定在该项目的管理 API。响应使用 `Cache-Control: no-store`，前端不将凭据保存在浏览器存储。管理 API 不提供写入或删除；查看器显示精确整数、带类型的键以及转义的字节字符串。

非 2xx HTTP 响应包含 `code`、`message` 和可选 `retryAfterSeconds`。常见状态为 400 `InvalidArgument`、401/403 `AuthenticationFailed`、资源缺失时的 404 `InvalidArgument`、429 `RateLimited`、500 `StorageFailure` 或 `ServerFailure`。限频 HTTP 响应另带 `Retry-After`。WebSocket 调用失败使用结果帧，不使用 HTTP 状态码。

HTTP 和 WebSocket 明文传输，共享密钥可访问项目注册的模块。开放服务前请阅读[部署信任边界](<部署与编辑器设置.md#管理与运行边界>)。
