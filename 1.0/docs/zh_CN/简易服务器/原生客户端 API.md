# 原生客户端 API

先按[部署与编辑器设置](<部署与编辑器设置.md>)配置 URL 和项目密钥。游戏代码通过 `Global.Net` 获取显式数据快照、调用 void RPC 和接收服务端命名事件。C++ 负责 WebSocket 连接、值编码以及游戏逻辑线程上的交付；Lua 不传入连接凭据。`GlobalCore.Server.CheckConnectionAsync` 仍是独立的 HTTP 健康检测接口。

## 定义网络模块与 proxy

Proxy 是保存一份服务端快照的本地 Lua 对象，getter 只读取快照，不发起网络请求。在独立模块中定义 `Global.NetProxy` 子类，再以与服务端相同的网络模块名注册。对应服务端实现见[服务端 Lua 模块](<HTTP 协议.md#服务端-lua-模块>)。

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

`Net.DefineModule(name, proxyClass)` 返回用于 RPC 与订阅的模块句柄。类必须继承 `Global.NetProxy`，重复名称会被拒绝。定义模块本身不连接服务器；获取 proxy 前须先 require 其定义。若子类覆盖 `init(moduleName, params, snapshot)`，必须用这些参数调用基类初始化函数；未覆盖时使用继承的初始化函数。类构造与镜像声明见 [Ludork 类](<../快速入门/Ludork Lua 进阶.md#ludork-类>)。

模块、RPC 与事件名区分大小写，由 1–128 个 ASCII 字母、数字、点、下划线或连字符组成。RPC 函数名对应服务端模块的 `Rpc` 表，与 proxy getter 名称无关。

公开的 Net、proxy、getter 与 RPC 函数采用 PascalCase，例如 `GetProxy`、`Sync`、`GetName`、`SetName`。类构造函数保留既有的 `init` 约定。

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

`Net.GetProxy(moduleName, params, callback)` 每次都发送新的快照请求，成功后构造新的 proxy，即使模块名与参数和之前完全相同也一样。请求时会复制参数，之后修改调用方的 table 不会改变该 proxy 的查询。不同 proxy 及其快照相互独立。

`proxy:Sync(callback)` 使用原先捕获的参数再次请求同一模块，刷新**现有对象**。只有响应成功后才整体替换快照。等待中、失败或取消的同步都保留旧快照供读取。同一 proxy 上的多次 Sync 按调用顺序串行处理，每次调用对应一次请求，不会合并。若要改变查询参数，应通过 `GetProxy` 获取新对象。

`proxy:GetValue(key)` 返回一个快照字段。Table 值及其嵌套 table 都是独立副本，修改 getter 结果不会写入快照或服务端。`GetName` 等业务 getter 应通过此方法读取，不直接访问私有字段。RPC 和服务端事件都不会自动刷新 proxy。

两个获取方法都要求回调，并返回 `Engine.AsyncOperation`。回调接收普通 Lua 结果表：

| 字段 | 含义 |
| --- | --- |
| `ok` | 是否成功获取并安装快照。 |
| `code` | 字符串错误码；成功为 `"None"`。 |
| `message` | 诊断文本；成功为空。 |
| `proxy` | GetProxy 新建的对象，或成功 Sync 的原对象；失败为 nil。 |
| `retryAfterSeconds` | 请求被限频时可选的整秒等待时间。 |

操作先保存结果，再调用回调；`operation:getResult()` 返回与回调参数**同一个 table**。失败以 `ok == false` 正常完成。显式取消会抑制 proxy 构造或快照替换，以及完成回调，但不能撤销服务器已接受的工作。关闭游戏会话会取消未完成工作并移除该会话的监听器。

请求从提交开始具有**十秒期限**，包括连接和排队时间。即使响应到达时逻辑线程处于暂停状态，超过期限才处理也会被拒绝。`Global.Net` 包装层会初始化潜伏操作更新，但仍须由运行中的游戏更新循环推进；裸脚本不会自行驱动回调。详见[异步操作更新](<../Lua 与蓝图脚本/Lua 运行时与模块.md#异步操作更新>)。

## 调用 RPC 与接收事件

```lua
local Profile = require("Source.Net.Profile")
local Logging = require("Global.Utils.Logging")

local errorToken = Profile.SubscribeError(function(error)
    Logging.warning("Profile RPC/connection failed (%s): %s", error.code, error.message)
end)

local eventToken = Profile.Subscribe("NameChanged", function(account)
    Logging.info("Name changed for %s", account)
    -- 当前界面需要新快照时，可在这里调用 GetProxy 或 Sync。
end)

Profile.CallRpcFunction("SetName", "player-1", "Ada")
```

`module.CallRpcFunction(functionName, ...)` 按位置传参，执行服务端同名 RPC。它**不返回任何值，也不返回操作句柄**。作为参数传入的 table 是一个参数，不是参数列表。中间及末尾的 nil 参数都保留位置。业务回复使用命名事件；内部成功确认不属于函数返回值。

`module.Subscribe(eventName, handler)` 与 `module.SubscribeError(handler)` 返回整数监听 token。`module.Unsubscribe(token)` 返回是否从当前模块移除了监听。需要接收某个 RPC 产生的事件时，应先订阅再发起调用。监听持续到显式移除或游戏会话结束，连接恢复后也保留。所属界面或对象不再需要监听时再清理：

```lua
Profile.Unsubscribe(eventToken)
Profile.Unsubscribe(errorToken)
```

事件携带服务端按位置传入的参数，在逻辑线程执行。事件仅在线交付，断开的客户端不会收到积压事件。事件与错误订阅使用 `EventBus`：某个监听器抛错时会记录日志，其他监听器继续执行。快照完成回调抛出的错误会在操作已保存结果后传播给宿主。这些应用错误不会被改写成网络消息格式错误。

模块错误包含 `code`、`message`、`module`，以及可选的 `functionName`、`requestId`、`retryAfterSeconds`。RPC 失败在信息可用时标明函数与请求；连接级错误可能不含这两项。快照失败通过对应结果回调交付。本地参数不合法、包含不支持的值或缺少回调属于编程错误，会在调用边界拒绝。

同一游戏会话的所有模块共用一条连接。首次获取、RPC 或订阅会启动连接，因此只订阅而不主动请求的客户端也能接收事件。连接丢失使未完成请求失败。普通连接故障后会自动重连，但不会重放 RPC、重新发起旧快照请求或同步现有 proxy。认证被拒绝后，该会话停止重连；须修正编译配置并启动新会话。

观察到超时或断开前，RPC 可能已经完成写入。应先读取新快照，再由业务决定是否重试；不存在自动回滚或恰好执行一次的重放机制。

## Lua 值与错误

参数和快照数据使用普通 Lua nil、布尔值、有限数字、字节字符串和无环 table。混合或稀疏的布尔／数字／字符串键、有符号 64 位整数都会保留。函数、线程、userdata、类实例／原生对象、元表、循环引用和 table 键会被拒绝。完整值域及大小规则见 [Lua 数据与传输编码](<HTTP 协议.md#lua-数据与传输编码>)；这些值不采用 `RuntimeData` 的 JSON 转换契约。

网络结果使用**字符串错误码**，包括 `Disabled`、`InvalidArgument`、`AuthenticationFailed`、`Timeout`、`ConnectionFailed`、`InvalidResponse`、`QueueFull`、`StorageFailure`、`ServerFailure`、`RateLimited`。服务端业务还可以返回其他合法错误码。用 code 判断，用 message 诊断。`retryAfterSeconds` 存在时，表示再次尝试前至少等待的整秒数。禁用构建保留 API，通过快照结果或模块错误报告 `Disabled`。

## 随附 Storage 模块

Require `Global.Net.Storage` 会注册内置客户端模块和 `Global.Net.StorageProxy` 类。服务端入口须以 `"Storage"` 注册 `Modules.Storage`。通过 `Net.GetProxy("Storage", params, callback)` 获取快照。

| `params.query` | Proxy getter |
| --- | --- |
| `"account_exists"` | `AccountExists()` |
| `"accounts"` | `GetAccounts()` |
| `"field"` | `IsFound()`、`GetFieldValue()` |
| `"fields"` | `GetEntries()`、`GetNextCursor()` |

Getter 仅读取该 proxy 对应查询的结果，不相关的 getter 可能返回 nil。Entries 中的项具有 `key` 与 `value` 字段。下一页游标为 nil 时分页结束；要使用其他游标，应发起新的 GetProxy。查询参数、账号抽样、字段顺序、缺失值及存储限制见[存储服务](<HTTP 协议.md#存储服务>)。

写入使用 `Storage.CallRpcFunction("WriteField", account, category, field, value)`，nil 表示删除字段。成功时向调用者发送 `FieldWritten(account, category, field)`。只有此随附 RPC 使用[配置的写入间隔](<HTTP 协议.md#写入限频>)，默认 180 秒；自定义 RPC 不继承该限制。不再提供写入 token API。

## 网络质量

独立的 `GlobalCore.Server.CheckConnectionAsync(callback)` 保留原生结果类型和三秒 HTTP 期限。自定义入口须初始化 `GlobalFunctions.NodeGraph.initLatent()` 并推进操作更新。回调接收 `GlobalCore.ServerConnectionResult`；生成声明中的原生对象可能为 nil，使用 `assert(result)` 收窄。

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

`Normal` 表示 300 ms 内收到配置服务的有效健康响应；`Weak` 表示超过 300 ms、但在三秒内收到；`Disconnected` 表示没有有效响应、请求失败或功能禁用。结果还包含 `ok`、类型为原生 `GlobalCore.ServerErrorCode` 枚举的 `code`、`message`，以及收到有效响应时的 `roundTripMs`。方法返回 `Engine.AsyncOperation`，保留回调／结果对象身份与取消语义。

健康接口不验证项目密钥，因此健康结果不能证明 RPC 认证成功，也不能代表整个互联网可用。系统 DNS 解析可能在取消后仍被阻塞，关闭会话时可能等待解析器返回；取消不保证 DNS 或退出具有硬性截止时间。
