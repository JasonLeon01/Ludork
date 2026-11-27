# 服务端 Lua 与协议

Node.js 负责网络连接、项目认证、存储与进程监管，项目 Lua 5.4.5 模块则实现显式快照、无返回值 RPC 与命名事件。每个项目运行在独立的 Wasmoon 进程中，不包含客户端的 Standard 扩展或原生模块。客户端代理用法请参阅[原生客户端 API](<原生客户端 API.md>)，项目入口路径配置请参阅[部署与编辑器设置](<部署与编辑器设置.md#项目-lua-入口>)。

## 服务端 Lua 模块

项目入口从 `lua/` 加载框架模块，从项目私有目录加载项目模块，并显式完成注册：

```lua
local Net = require("Framework.Net")
local Storage = require("Modules.Storage")
local Profile = require("Profile")

Net.RegisterModule("Storage", Storage)
Net.RegisterModule("Profile", Profile)
```

项目中的 `Profile.lua` 可同时定义读取与写入逻辑：

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

- **命名与注册**：模块名、RPC 名与事件名区分大小写，由 1–128 个 ASCII 字母、数字、点号、下划线或连字符组成。重复注册会导致初始化失败。仅模块 `Rpc` 表中的函数可远程调用；若不需要某类操作，可省略 `Snapshot` 或 `Rpc`。
- **快照（`Snapshot(context, params)`）**：接收单个纯 Lua 值，且必须返回普通 table。每次客户端请求均会重新执行该函数，期间仅允许只读存储访问且不能发送事件。服务器既不缓存客户端代理，也不会自动同步数据变更。
- **RPC（`Rpc.FunctionName(context, ...)`）**：按位置接收纯 Lua 参数（包括中间及末尾的 `nil`），返回值会被丢弃。来自同一连接的调用按发送顺序执行，且每个项目串行执行处理函数。宿主存储调用会挂起当前协程直至 Node.js 完成，从而支持顺序风格编写 Lua 代码。请勿保存 `context` 或在处理函数返回后继续调用其服务。

| Context 成员 | 含义 |
| --- | --- |
| `ProjectName`、`ModuleName` | 当前项目名称与已注册模块名称。 |
| `SessionId` | 当前在线连接的不透明标识。 |
| `PeerAddress` | 实际 TCP 对端地址，忽略转发请求头。 |
| `Storage` | 作用域限定在当前项目内的存储服务。 |
| `Notify(event, ...)` | 为当前调用方排入事件。 |
| `Send(sessionId, event, ...)` | 为本项目的单个在线连接排入事件。 |
| `Broadcast(event, ...)` | 为本项目所有在线连接排入事件。 |
| `AdmitWrite(account, category, field)` | 仅限在随附的 `Storage.WriteField` RPC 内应用已配置的写入间隔。 |

Context 方法均使用 `:` 调用。事件会附带当前模块名，并在处理函数成功返回后统一发送；若处理函数失败，其排入的事件会被丢弃，但已完成的存储写入不会回滚。已断开的接收端不会收到历史积压事件，事件也不会自动刷新客户端代理数据。

普通 Lua 错误会使请求返回 `ServerFailure`。处理函数也可抛出包含字符串 `code`、字符串 `message` 与可选正整数 `retryAfterSeconds` 的 table；自定义错误码须以 ASCII 字母开头，后接至多 63 个 ASCII 字母或数字（非法错误码会映射为 `ServerFailure`），宿主错误则保留其稳定错误码。

由于共享项目密钥不用于认证单个账号 ID，参数校验与账号授权须由项目逻辑自行实施。

- **执行限制**：每个项目拥有 64 MiB Lua 分配器上限、9 秒 Lua 执行中断预算，以及包含排队时间在内的 10 秒总调用期限。
- **恢复与隔离**：进程崩溃、执行中调用超时或内存故障会使该项目的现有连接失效并重启其 Lua 状态。此前未完成的调用将直接失败且不重放；恢复期间到达的新请求会在有界队列内按原期限等待，同时其他项目、健康检测与管理服务不受影响。若入口文件加载失败，该项目的所有请求都将失败，直至修复源码并重启服务。
- **沙箱限制**：服务端逻辑禁止使用 `io`、`os`、`debug`、`load`、`loadfile`、`dofile` 或加载原生库；所有持久化操作必须通过 `context.Storage` 完成。

## 存储服务

项目、账号与分类在文件系统中的名称均采用小写十六进制 UTF-8 编码，既保留大小写区分，又避免系统保留文件名与路径穿越风险。账号 ID 由游戏逻辑提供；服务器不生成 UUID、不预留 ID，也不负责账号认证。

账号、分类与字段名均接受 1–64 个不含控制字符的 UTF-8 字节，且不能恰好为 `.` 或 `..`；与项目名不同，这三者允许包含斜杠。

每个分类对应一个纯数据 `.ldata` 文件，外层封装为 `{version:1,data:WireValue}`，根节点为以字段名为键的 table，并采用下文所述的[带标签编码](<#lua-数据与传输编码>)。该文件从不作为 Lua 代码执行。已有的 `.json` 分类文件不会被读取或转换，不提供兼容读取或自动迁移。

| `context.Storage` 方法 | 结果 |
| --- | --- |
| `AccountExists(account)` | 布尔值：账号目录是否存在（空目录同样返回 `true`）。 |
| `ListAccounts(sampleCount, categoryFilter)` | 符合条件的账号名数组。 |
| `ReadField(account, category, field)` | `{found = boolean, value = value}`；字段缺失时返回 `found = false`、`value = nil`。 |
| `ListFields(account, category, beforeKey, limit)` | `{entries = {{key = key, value = value}, ...}, nextCursor = cursor}`。 |
| `WriteField(account, category, field, value)` | `nil`；覆盖字段值，当 `value` 为 `nil` 时删除该字段。 |

- **调用方式与缺失数据**：`context.Storage` 方法均使用 `:` 调用。读取操作不会创建账号；账号或分类缺失时返回空列表或字段缺失结果，而文件系统检查失败或数据损坏会抛出 `StorageFailure`。
- **账号列表（`ListAccounts`）**：`sampleCount` 接受非负有符号 64 位整数（`nil` 或 `0` 按 UTF-8 字节升序返回全部匹配账号；正数则按随机顺序抽样不重复账号，上限为匹配总数）。`categoryFilter` 仅检查普通 `.ldata` 文件是否存在，不校验其内容。该接口无分页游标；若完整响应超过消息上限，应减小 `sampleCount`。
- **字段排序与游标（`ListFields`）**：字段按 UTF-8 字节降序排列，与区域设置及数值大小无关。`beforeKey = nil` 表示从首页开始；传入指定键时作为排他上界，不要求该键实际存在。`limit` 默认为 `50`（有效范围 `1–100`）。存在更多匹配字段时，`nextCursor` 为本页最后一个键，否则为 `nil`（空范围返回空 `entries` 与 `nextCursor = nil`）。
- **实时分页（`ListFields`）**：分页直接读取实时文件，不提供跨请求快照。如需获取大于当前游标的新插入键，应从首页重新刷新；若希望字节序与时间一致，请使用固定宽度时间戳。
- **写入与原子性（`WriteField`）**：写入会按需创建账号与分类；写入 `nil` 会删除目标字段，同时保留分类文件与账号目录。同一文件的更新串行执行，并通过临时文件原子提交；不同文件可独立更新。
- **文件大小与事务限制**：单个完整分类文件上限为 **1 MiB**（包含编码开销与末尾换行）。系统不提供跨文件事务、自动备份或后续语句失败时的自动回滚；写入提交后仍可能出现请求超时或响应丢失。

随附的 `Modules.Storage` 模块提供以下快照查询：

| `params.query` | 参数 | 快照 |
| --- | --- | --- |
| `"account_exists"` | `account` | `{exists = boolean}` |
| `"accounts"` | 可选 `sampleCount`、`categoryFilter` | `{accounts = {...}}` |
| `"field"` | `account`、`category`、`field` | `{found = boolean, value = value}` |
| `"fields"` | `account`、`category`；可选 `beforeKey`、`limit` | `{entries = {...}, nextCursor = cursor}` |

其 `WriteField(account, category, field, value)` RPC 会应用配置的写入间隔、写入字段，并向调用方发送 `FieldWritten(account, category, field)` 事件。自定义模块可直接调用 `context.Storage` 并自行实现业务规则。

### 写入限频

配置的 `ipWriteIntervalSeconds`（默认 **180 秒**，详见[配置写入间隔](<部署与编辑器设置.md#配置写入间隔>)）仅作用于随附的 `Storage.WriteField` RPC，并在同一 TCP 客户端 IP 访问同一项目下的所有账号与分类时共享额度。IPv4 映射的 IPv6 地址会归一化为 IPv4，且忽略 `X-Forwarded-For`，因此位于同一 NAT 或反向代理后的客户端共享同一额度，不同项目则各自独立计算。

准入检查在存储执行前即启动计时，因此后续存储失败仍会消耗本次额度；限频拒绝不会延长原截止时间。`RateLimited` 错误包含向上取整为整秒的正整数 `retryAfterSeconds`。截止时间采用单调时钟，在服务重启后重置。读取、健康检测、管理接口与自定义 RPC 均不消耗该额度，且自定义 RPC 无法调用 `AdmitWrite` 来复用内置限频间隔。

## Lua 数据与传输编码

参数、快照、事件与存储值接受 `nil`、布尔值、有限数字、字节字符串与普通无环 table。整数保留完整的有符号 64 位范围；table 支持混合或稀疏的布尔、数字与字符串键；`nil` 可作为合法位置参数（在 table 中表示字段缺失）；空 table 按空 table 编码。

函数、线程、userdata、元表、循环引用及 table 类型的键均会被拒绝。无环 table 中的重复引用会编码为独立副本，不保留对象同一性。上述规则同样排除了 Ludork 类实例与原生容器。

传输帧采用 UTF-8 JSON，每个 Lua 值均编码为单个带标签的数组：

```text
["nil"]
["bool", true]
["int", "9223372036854775807"]
["float", "1.2345678901234567"]
["string", "Ynl0ZXM="]
["table", [[WireKey, WireValue], ...]]
```

整数采用精确十进制文本，浮点数采用可往返还原的有限双精度十进制文本，字符串则以规范 Base64 编码任意字节序列。`WireKey` 仅接受布尔、整数、浮点数与字符串形式；不允许等价键重复（例如整数 `1` 与浮点数 `1.0`），值为 `nil` 的 table 条目会被省略。

单条消息上限为 **2 MiB**，最大 **64 层嵌套**、至多 **65,536 个值节点**。RPC 与事件参数数组保留每个 `nil` 的位置。各项上限均按编码后的完整帧计算，包含 Base64 与结构开销。

## WebSocket 消息

客户端连接至 `ws://主机:6666/<URL编码后的项目名>/api/v1/net`；若使用[已绑定项目域名](<部署与编辑器设置.md#绑定项目域名>)，可直接连接 `ws://主机:6666/api/v1/net`。浏览器 HTTPS 端点经由 TLS 代理使用 WSS。凭据从不出现在 URL 中，且不使用二进制帧与消息压缩。

连接建立后首条消息必须发送 `{"type":"auth","key":"<项目密钥>"}`，服务器在接收任何应用帧之前返回 `{"type":"ready","version":1}`。认证超时为 10 秒：认证失败时服务器以状态码 `4401` 关闭连接，客户端在该会话内停止自动重连。

```text
snapshot: {type:"snapshot", id:"1", module:"Profile", params:WireValue}
RPC:      {type:"rpc", id:"2", module:"Profile", name:"SetName", args:[WireValue,...]}
success:  {type:"result", id:"1", ok:true, data:WireValue}
RPC ack:  {type:"result", id:"2", ok:true}
failure:  {type:"result", id:"2", ok:false, code:"InvalidArgument", message:"..."}
event:    {type:"event", module:"Profile", event:"NameChanged", args:[WireValue,...]}
```

请求 ID 由 1–128 个 ASCII 字母、数字、点号、下划线或连字符组成，在同一连接的未完成请求中必须唯一（重复的未完成 ID 会导致连接关闭）。限频失败响应可能包含 `retryAfterSeconds`。RPC 确认帧仅表明执行成功，不含业务返回值；服务端事件在成功确认帧之后发出。

- **队列与缓冲区上限**：每条连接与每个项目的待处理入站工作上限均为 256 次调用与 8 MiB；每条连接的出站发送缓冲上限为 8 MiB（消费过慢的连接会被关闭）。
- **事件上限与保活**：单次处理函数最多排入 256 个事件及 8 MiB 编码后的事件消息。Ping/pong 帧用于检测并清理失效连接。有关客户端超时、取消与恢复规则，请参阅[调用 RPC 与接收事件](<原生客户端 API.md#调用-rpc-与接收事件>)。

## HTTP 健康检测与只读管理

HTTP API 基地址为 `http://主机:6666/<项目名>/api/v1`（已绑定项目域名时为 `http://主机:6666/api/v1`）。`GET /health` 无需密钥，返回 `{"service":"LudorkServer","project":"MyGame"}`：若 URL 包含项目名，响应项目必须与之完全匹配；若 URL 不含项目路径，则接受任意非空项目名。

数据查询与写入均通过 WebSocket 模块 API 完成，不提供账号或字段的 REST 接口或写入令牌接口。

`GET /health` 通过 `Access-Control-Allow-Origin: *` 允许无凭据的浏览器跨域请求。该跨域许可仅限公开健康检测路由。

管理路由位于 `/admin` 下。3333 端口的生产 Node.js 服务托管构建后的管理网页，并将同源 API 请求代理至 6666 端口；后端负责执行认证，且不为管理接口启用 CORS。

| `/admin` 下的方法与路径 | 要求 | 成功响应 |
| --- | --- | --- |
| `POST /login` | JSON `{"key":"...","password":"..."}` 与 `X-Ludork-Admin: 1` | `{"project":"MyGame"}`，并设置会话 Cookie。 |
| `GET /session` | 会话 Cookie | `{"project":"MyGame"}` |
| `POST /logout` | 会话 Cookie 与 `X-Ludork-Admin: 1` | `{}`，清除会话 Cookie。 |
| `GET /accounts` | 会话 Cookie | `{"accounts":["player-1"]}` |
| `GET /accounts/:account/categories` | 会话 Cookie | `{"categories":["profile"]}` |
| `GET /accounts/:account/categories/:category` | 会话 Cookie | 分类 table 的 `{value:WireValue}`。 |

每个路径参数须单独编码为一个 URL 路径段。会话与项目绑定，有效期为 8 小时，在退出登录或后端重启后立即失效；Cookie 设置为 `HttpOnly` 与 `SameSite=Strict`，作用域限定于该项目的管理 API。

所有管理响应均设置 `Cache-Control: no-store`，前端不在浏览器存储中保存任何凭据。管理接口不提供写入或删除操作；数据查看器会完整渲染精确整数、带类型的键以及转义字节字符串。

非 2xx HTTP 响应均包含 `code`、`message` 及可选 `retryAfterSeconds`。常见状态码包括 `400`（`InvalidArgument`）、`401`/`403`（`AuthenticationFailed`）、`404`（资源缺失时的 `InvalidArgument`）、`429`（`RateLimited`，同时设置 `Retry-After` 响应头）与 `500`（`StorageFailure` 或 `ServerFailure`）。WebSocket 调用失败通过结果帧返回，不使用 HTTP 状态码。

HTTP 与 WebSocket 流量均为明文传输，且共享密钥可访问项目注册的全部模块。公开部署服务前，请先审阅[部署信任边界](<部署与编辑器设置.md#管理与运行边界>)。
