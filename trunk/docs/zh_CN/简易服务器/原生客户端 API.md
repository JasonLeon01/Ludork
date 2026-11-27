# 原生客户端 API

请先按[部署与编辑器设置](<部署与编辑器设置.md>)配置服务器 URL 与项目密钥。游戏代码通过 `Global.Net` 获取显式数据快照、调用无返回值 RPC 并接收服务端命名事件；C++ 层负责维护 WebSocket 连接、传输编码及逻辑线程交付，Lua 层不直接传递连接凭据。`GlobalCore.Server.CheckConnectionAsync` 提供独立的 HTTP 健康探针。

## 禁用服务器集成

构建时若未启用服务器集成，所有服务器调用均只输出一条诊断日志并立即返回，不会建立连接、发送或排队请求、创建异步操作、注册监听或轮询、复制请求参数、修改 proxy 状态、触发回调或发布错误事件。各 API 仍可调用，返回值如下：

| 调用 | 禁用时的返回值 |
| --- | --- |
| `Net.GetProxy`、`proxy:Sync`、`Server.CheckConnectionAsync`、原生 `NetTransport.GetSnapshotAsync` | `nil` |
| `module.Subscribe`、`module.SubscribeError` | `0`（不创建监听） |
| `module.Unsubscribe` | `false` |
| `module.CallRpcFunction` | 无返回值 |

在等待操作完成或调用其方法前，务必检查返回值不为 `nil`。禁用状态下的调用既不会产生 `Disabled` 失败结果，也不会产生 `Disconnected` 健康检测结果。`Net.DefineModule`、使用本地数据构造 proxy、`proxy:GetValue` 以及原生 `NetTransport.CloneValue` 仍作为本地操作正常运行。下文的请求、回调与连接行为均以启用服务器集成为前提。

## 定义网络模块与 proxy

**proxy**（代理对象）是保存单份服务端快照的本地 Lua 对象，其 getter 仅读取该快照，不触发任何网络 I/O。在独立模块中定义 `Global.NetProxy` 子类，再以与服务端一致的网络模块名注册（服务端定义见[服务端 Lua 模块](<HTTP 协议.md#服务端-lua-模块>)）。

```lua
-- Scripts/Source/Net/ProfileProxy.lua
local NetProxy = require("Global.NetProxy")

local ProfileProxy = {}

function ProfileProxy:GetName()
    return self:GetValue("name")
end

return class(ProfileProxy, NetProxy)
```

```lua
-- Scripts/Source/Net/Profile.lua
local Net = require("Global.Net")
local ProfileProxy = require("Source.Net.ProfileProxy")

return Net.DefineModule("Profile", ProfileProxy)
```

`Net.DefineModule(name, proxyClass)` 返回用于调用 RPC 与管理订阅的模块句柄。`proxyClass` 必须继承自 `Global.NetProxy`，重复注册同名模块会被拒绝。定义模块不会立即发起连接；在请求 proxy 之前必须先 `require` 该模块定义。

若子类覆写 `init(moduleName, params, snapshot)`，必须使用相同参数调用基类初始化函数；否则自动执行继承的初始化函数。类构造与镜像声明规范请参阅 [Ludork 类](<../快速入门/Ludork Lua 进阶.md#ludork-类>)。

模块名、RPC 名与事件名均区分大小写，其字符集与长度限制见[服务端 Lua 模块](<HTTP 协议.md#服务端-lua-模块>)。RPC 函数名对应服务端模块的 `Rpc` 表，与 proxy 的 getter 名称无关。

公开的 Net、proxy、getter 与 RPC 函数统一使用 PascalCase（如 `GetProxy`、`Sync`、`GetName`、`SetName`），类构造函数则沿用 `init`。

## 获取与刷新快照

```lua
local Net = require("Global.Net")
local Profile = require("Source.Net.Profile")
local Logging = require("Global.Utils.Logging")

local operation = Net.GetProxy("Profile", {account = "player-1"}, function(result)
    if not result.ok then
        Logging.warning("Profile failed: %s", result.message)
        return
    end
    local proxy = assert(result.proxy)
    Logging.info("Name: %s", tostring(proxy:GetValue("name")))
    proxy:Sync(function(refreshed)
        if refreshed.ok then
            assert(refreshed.proxy == proxy)
            Logging.info("Refreshed name: %s", tostring(proxy:GetValue("name")))
        end
    end)
end)
```

- **`Net.GetProxy(moduleName, params, callback)`**：每次调用均发送新的快照请求，并在成功后构造新的 proxy 实例，即便模块名与参数与之前完全一致。请求发起时会复制参数，因此后续修改调用方的 table 不影响查询；不同 proxy 及其快照彼此独立。
- **`proxy:Sync(callback)`**：使用创建时捕获的参数重新请求同一模块并刷新**现有对象**，仅在响应成功后整体替换快照；同步进行中、失败或取消时，旧快照仍可正常读取。同一 proxy 上的多次 `Sync` 按调用顺序串行执行（每次调用对应独立请求，不会合并）。如需更改查询参数，请重新调用 `GetProxy`。
- **`proxy:GetValue(key)`**：读取单个快照字段；table 值（含嵌套 table）会作为独立副本返回，修改返回值不会影响快照或服务端数据。`GetName` 等业务 getter 应通过 `GetValue` 读取，避免直接访问私有字段。RPC 与服务端事件均不会自动刷新 proxy。

启用服务器集成时，上述两个获取方法都必须传入回调并返回 `Engine.AsyncOperation`。回调接收普通 Lua 结果表：

| 字段 | 含义 |
| --- | --- |
| `ok` | 是否成功获取并应用快照。 |
| `code` | 字符串错误码；成功时为 `"None"`。 |
| `message` | 诊断文本；成功时为空字符串。 |
| `proxy` | `GetProxy` 新建的对象，或 `Sync` 成功时刷新的原对象；失败时为 `nil`。 |
| `retryAfterSeconds` | 请求触发限频时的可选整秒等待时长。 |

异步操作会先保存该结果表再调用回调，因此 `operation:getResult()` 返回与回调参数**相同的 table 对象**。失败时操作正常完成且 `ok == false`。显式取消会阻止构造或替换 proxy 并跳过回调，但无法撤销服务端已受理的操作。关闭游戏会话时会自动取消未完成请求并清理监听器。

所有请求自提交起具有 **10 秒总期限**（包含连接与排队时间）；即便响应到达时逻辑线程处于暂停状态，超过期限后才处理的响应仍会被拒绝。`Global.Net` 包装层会初始化 Latent 操作更新，但仍需运行中的游戏主循环持续推进——裸脚本不会自动触发回调（详见[异步操作更新](<../Lua 与蓝图脚本/Lua 运行时与模块.md#异步操作更新>)）。

## 调用 RPC 与接收事件

```lua
local Profile = require("Source.Net.Profile")
local Logging = require("Global.Utils.Logging")

local errorToken = Profile.SubscribeError(function(error)
    Logging.warning("Profile RPC/connection failed (%s): %s", error.code, error.message)
end)

local eventToken = Profile.Subscribe("NameChanged", function(account)
    Logging.info("Name changed for %s", account)
    -- 当前界面需要新快照时，可在此调用 GetProxy 或 Sync。
end)

Profile.CallRpcFunction("SetName", "player-1", "Ada")
```

`module.CallRpcFunction(functionName, ...)` 按位置传参调用服务端同名 RPC，**不返回任何值，也不返回操作句柄**。传入的 table 作为单个参数传递而不会解包，中间及末尾的 `nil` 参数均保留位置。业务响应通过命名事件回传，内部成功确认帧不作为函数返回值暴露。

`module.Subscribe(eventName, handler)` 与 `module.SubscribeError(handler)` 返回整数监听令牌，`module.Unsubscribe(token)` 返回是否成功从该模块移除了对应监听。若需接收某次 RPC 触发的事件，请先订阅再发起调用。监听器持续生效直至显式取消订阅或游戏会话结束，并在连接恢复后保持有效；界面或对象销毁不再需要时请及时取消订阅：

```lua
Profile.Unsubscribe(eventToken)
Profile.Unsubscribe(errorToken)
```

事件在逻辑线程上按位置传递服务端参数，仅在线交付，断开连接期间不会积压历史事件。

事件与错误订阅基于 `EventBus`：单个监听器抛出错误时会记录日志，其余监听器继续执行。快照完成回调抛出的错误会在操作保存结果后向宿主传播，应用层错误不会被改写为网络格式错误。

模块错误表包含 `code`、`message`、`module`，以及可选的 `functionName`、`requestId` 与 `retryAfterSeconds`。RPC 失败会在信息可用时附带函数名与请求 ID，连接级错误则可能省略这两项。快照失败通过各自的结果回调交付。本地参数无效、包含不支持的值或缺少回调属于编程错误，会在调用边界直接抛出。

同一游戏会话内的所有模块共享单条连接，并在首次快照获取、RPC 调用或订阅时建立，因此仅订阅事件的客户端也能接收通知。连接中断会导致未完成请求失败；遇到常规故障时传输层会自动重连，但不会重放 RPC、重发旧快照请求或自动同步现有 proxy。若认证被拒绝，当前会话将停止重连，需修正编译配置后启动新会话。

> **重要**：在客户端检测到超时或连接断开之前，RPC 可能已经提交了存储写入。请先拉取最新快照，再由业务逻辑决定是否重试；系统不提供自动回滚或恰好执行一次的重放机制。

## Lua 值与错误

参数与快照数据支持普通 Lua `nil`、布尔值、有限数字、字节字符串与无环 table，完整保留混合或稀疏的布尔/数字/字符串键及有符号 64 位整数；函数、线程、userdata、类实例/原生对象、元表、循环引用及 table 键均会被拒绝。完整数值与大小规则请参阅 [Lua 数据与传输编码](<HTTP 协议.md#lua-数据与传输编码>)（此类网络值不采用 `RuntimeData` 的 JSON 转换契约）。

网络结果采用**字符串错误码**，包括 `InvalidArgument`、`AuthenticationFailed`、`Timeout`、`ConnectionFailed`、`InvalidResponse`、`QueueFull`、`StorageFailure`、`ServerFailure`、`RateLimited` 以及服务端定义的合法自定义错误码。业务分支应依据 `code` 判断，使用 `message` 输出诊断信息；若存在 `retryAfterSeconds`，重试前应至少等待对应的整秒数。

## 随附 Storage 模块

`require("Global.Net.Storage")` 即可注册内置客户端模块及其 `Global.Net.StorageProxy` 类（服务端入口须将 `Modules.Storage` 注册为 `"Storage"`）。通过 `Net.GetProxy("Storage", params, callback)` 获取快照：

| `params.query` | proxy getter |
| --- | --- |
| `"account_exists"` | `AccountExists()` |
| `"accounts"` | `GetAccounts()` |
| `"field"` | `IsFound()`、`GetFieldValue()` |
| `"fields"` | `GetEntries()`、`GetNextCursor()` |

每个 getter 仅读取该 proxy 对应查询的结果，无关的 getter 可能返回 `nil`。列表条目包含 `key` 与 `value` 字段，下一页游标为 `nil` 时表示分页结束（翻页需携带游标发起新的 `GetProxy` 请求）。查询参数、账号抽样、字段排序、缺失值处理与存储上限详见[存储服务](<HTTP 协议.md#存储服务>)。

写入调用 `Storage.CallRpcFunction("WriteField", account, category, field, value)`（传入 `nil` 表示删除字段）。成功后服务端向调用方发送 `FieldWritten(account, category, field)` 事件。仅此内置 RPC 受[配置的写入间隔](<HTTP 协议.md#写入限频>)限制（默认 180 秒）；自定义 RPC 不继承该限制，且无写入令牌 API。

## 网络质量

启用服务器集成时，独立的 `GlobalCore.Server.CheckConnectionAsync(callback)` 会执行 3 秒期限的 HTTP 健康探针。自定义入口脚本必须先初始化 `GlobalFunctions.NodeGraph.initLatent()` 并推进异步操作更新。回调接收 `GlobalCore.ServerConnectionResult`；由于生成的原生对象声明允许为空，请使用 `assert(result)` 收窄类型：

```lua
local GlobalCore = require("GlobalCore")
local Logging = require("Global.Utils.Logging")

GlobalCore.Server.CheckConnectionAsync(function(result)
    assert(result)
    if result.state == GlobalCore.ServerNetworkState.Normal then
        Logging.info("Normal: %s ms", tostring(result.roundTripMs))
    elseif result.state == GlobalCore.ServerNetworkState.Weak then
        Logging.info("Weak: %s ms", tostring(result.roundTripMs))
    else
        Logging.warning("Disconnected: %s", result.message)
    end
end)
```

`Normal` 表示在 300 ms 内收到已配置服务的有效健康响应；`Weak` 表示耗时超过 300 ms 但在 3 秒内收到响应；`Disconnected` 表示未收到有效响应或请求失败。结果对象还包含 `ok`、`code`（`GlobalCore.ServerErrorCode`）、`message` 以及收到有效响应时的 `roundTripMs`。该函数返回 `Engine.AsyncOperation`，遵循标准的回调/结果同一性与取消语义。

健康接口不校验项目密钥，因此健康检测通过既不证明 RPC 认证有效，也不代表外部互联网畅通。在原生客户端中，系统 DNS 解析在请求取消后仍可能保持阻塞并延迟会话关闭；取消操作无法保证 DNS 解析或关闭过程具备硬性截止时间。
