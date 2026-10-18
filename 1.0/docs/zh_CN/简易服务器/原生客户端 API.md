# 原生客户端 API

先按[部署与编辑器设置](<部署与编辑器设置.md>)配置 URL 和密钥。网络传输、JSON 转换、写入 token 的持有与完成调度均在 C++ 中执行；Lua 只暴露带类型的 `GlobalCore.Server` 方法，不接收 URL 或密钥参数。传输层通过 SFML 网络模块使用 HTTP。

## 方法与结果

每个方法都要求完成回调，并返回 `Engine.AsyncOperation`。应在运行中游戏的逻辑线程调用，且已初始化潜伏操作更新。默认 Entry 会调用 `GlobalFunctions.NodeGraph.initLatent()`，自定义入口也需要初始化；操作生命周期见 [Lua 运行时与模块](<../Lua 与蓝图脚本/Lua 运行时与模块.md>)。在没有游戏更新循环的裸脚本中创建操作，不会自动驱动回调。

| `GlobalCore.Server` 方法 | 回调参数 |
|---|---|
| `CheckAccountExistsAsync(accountId, onCompleted)` | `GlobalCore.ServerAccountResult` |
| `ListAccountsAsync(sampleCount, categoryFilter, onCompleted)` | `GlobalCore.ServerAccountListResult` |
| `ReadFieldAsync(accountId, category, field, onCompleted)` | `GlobalCore.ServerReadResult` |
| `ListFieldsAsync(accountId, category, beforeKey, limit, onCompleted)` | `GlobalCore.ServerFieldListResult` |
| `AcquireWriteTokenAsync(accountId, onCompleted)` | `GlobalCore.ServerTokenResult` |
| `WriteFieldAsync(token, category, field, value, onCompleted)` | `GlobalCore.ServerResult` |
| `CheckConnectionAsync(onCompleted)` | `GlobalCore.ServerConnectionResult` |

所有结果都提供只读的 `ok`、`code`、`message` 和 `retryAfterSeconds: integer|nil` 属性。仅当 `code == GlobalCore.ServerErrorCode.None` 时，`ok` 为 true。用枚举判断业务结果，用 `message` 输出诊断。`retryAfterSeconds` 只在写入被限频时存在。使用查询数据前应检查 `ok`；失败时的空结果不代表查询成功且没有数据。

列表接口的可选参数仍占据对应位置：需要默认值时传 `nil`，必填回调始终放在最后。

| 结果 | 额外属性 |
|---|---|
| `ServerAccountResult` | 继承 `ServerResult`，新增只读 `exists: boolean`；仅在 `ok == true` 时有效。 |
| `ServerAccountListResult` | `accounts: string[]`，使用 `ipairs` 遍历账号 ID。 |
| `ServerReadResult` | `found: boolean`、`value: Engine.RuntimeDataValue`。字段不存在时 `found == false`；已存储的 JSON null 则为 `found == true`。 |
| `ServerFieldListResult` | `entries: GlobalCore.ServerFieldEntry[]`、`nextCursor: string\|nil`；游标为 nil 时结束分页。 |
| `ServerTokenResult` | `token: GlobalCore.ServerWriteToken\|nil`，申请成功时存在。 |
| `ServerConnectionResult` | `state: GlobalCore.ServerNetworkState`、`roundTripMs: number\|nil`，仅有效健康响应携带耗时。 |

原生声明位于 `<Server.hpp>` 与 `<Server/...hpp>`。C++ 回调接收对应的 `std::shared_ptr<Result>`，生成的 Lua 声明因此包含 `nil`，示例中使用 `assert(result)` 收窄类型。结果和 token 是返回的原生对象，不是需要自行构造的 table。`ServerFieldEntry` 是返回的值记录，具有只读的 `key: string` 与 `value: Engine.RuntimeDataValue`；entries 数组中的记录不会为 nil。`ServerWriteToken` 不透明，Lua 不能读取或填入其原始 token、绑定账号。

完成时先把结果存到操作上，再在游戏逻辑线程调用回调。完成后 `operation:getResult()` 返回与回调参数**同一个结果对象**。请求失败会以 `ok == false` 正常完成，不等同于取消。显式取消会抑制完成回调，并通知传输停止，但无法撤销服务器已经接受的写入。关闭游戏会话也会取消其未完成工作，不会从工作线程回调 Lua。

## 查询账号是否存在

使用 `CheckAccountExistsAsync` 查询账号：

```lua
local GlobalCore = require("GlobalCore")
local Logging = require("Global.Utils.Logging")

GlobalCore.Server.CheckAccountExistsAsync("player-1", function(result)
    assert(result)
    if not result.ok then
        Logging.warning("Account query failed: %s", result.message)
    elseif result.exists then
        Logging.info("Account exists")
    else
        Logging.info("Account is absent")
    end
end)
```

账号数据目录存在即视为账号存在，空目录也算。查询不会创建或预留账号，也不提供原子的防重保证；UUID 账号标识由应用生成。存储故障仍是错误，不能视为 `exists == false`。详见[账号存在性语义](<HTTP 协议.md#查询账号是否存在>)。

## 列出账号与字段

### 获取或随机抽取账号

`ListAccountsAsync(nil, nil, onCompleted)` 与 `sampleCount == 0` 返回全部符合条件的账号 ID，按 UTF-8 字节升序排列。正数表示无放回随机抽取，最多返回该数量，顺序随机。`categoryFilter` 先筛选再抽样，只检查文件存在性，不解析 JSON。数量范围、存储错误与 2 MiB 响应上限见[列出账号](<HTTP 协议.md#列出账号>)。

下面的游戏业务示例从具有 `history` 分类的账号中随机抽取最多十个：

```lua
local GlobalCore = require("GlobalCore")
local Logging = require("Global.Utils.Logging")

GlobalCore.Server.ListAccountsAsync(10, "history", function(result)
    assert(result)
    if not result.ok then
        Logging.warning("Account list failed: %s", result.message)
        return
    end
    for _, accountId in ipairs(result.accounts) do
        Logging.info("Account: %s", accountId)
    end
end)
```

账号列表没有分页游标。结果过大时以 `InvalidArgument` 失败，应改用更小的正数 `sampleCount`。

### 分页读取分类字段

字段按 **UTF-8 字节降序**排列，不按语言区域或数值含义排序。`beforeKey == nil` 从首页开始；指定键是排他的上界，只返回比它小的键，即使该上界本身没有存储。`limit == nil` 表示 50，显式数量必须为 1–100 的整数。`beforeKey` 遵循下方的字段名规则。

后面还有字段时，`nextCursor` 为本页最后一个键；将它原样传作下一页的 `beforeKey`，直到游标为 nil。账号或分类不存在、范围已遍历完时，成功返回空 `entries` 和 nil 游标；分类损坏则返回 `StorageFailure`。

使用返回的游标逐页读取：

```lua
local GlobalCore = require("GlobalCore")
local Logging = require("Global.Utils.Logging")

local function readPage(beforeKey)
    GlobalCore.Server.ListFieldsAsync("player-1", "history", beforeKey, nil, function(result)
        assert(result)
        if not result.ok then
            Logging.warning("Field list failed: %s", result.message)
            return
        end
        for _, entry in ipairs(result.entries) do
            Logging.info("%s = %s", entry.key, tostring(entry.value))
        end
        if result.nextCursor ~= nil then
            readPage(result.nextCursor)
        end
    end)
end

readPage(nil)
```

分页不提供跨写入的快照。要纳入新插入且大于当前游标的键，应从首页刷新。时间戳键的设计与响应大小限制见[列出分类字段](<HTTP 协议.md#列出分类字段>)。

## 在 Lua 中读写

以下示例写入一个字段，再读取：

```lua
local GlobalCore = require("GlobalCore")
local Logging = require("Global.Utils.Logging")

local Server = GlobalCore.Server

Server.AcquireWriteTokenAsync("player-1", function(result)
    assert(result)
    if not result.ok then
        Logging.warning("Token request failed: %s", result.message)
        return
    end
    local token = assert(result.token)
    Server.WriteFieldAsync(token, "profile", "score", 100, function(written)
        assert(written)
        if not written.ok then
            if written.code == GlobalCore.ServerErrorCode.RateLimited then
                Logging.warning("Wait at least %d seconds, then acquire a new write token.",
                    assert(written.retryAfterSeconds))
            else
                Logging.warning("Write failed: %s", written.message)
            end
            return
        end
        Server.ReadFieldAsync("player-1", "profile", "score", function(read)
            assert(read)
            if not read.ok then
                Logging.warning("Read failed: %s", read.message)
            elseif read.found then
                Logging.info("Score: %s", tostring(read.value))
            else
                Logging.info("Score is absent")
            end
        end)
    end)
end)
```

账号、分类和字段名无需预先 URL 编码，客户端会编码每个路径段。名称须遵循 [HTTP 标识符规则](<HTTP 协议.md>)，包括 1–64 个 UTF-8 字节的限制。

值遵循 [RuntimeData 转换规则](<../C++ 原生开发/运行时值边界.md#lua-值转换>)。顶层 null 会存储 null，而不是删除字段。后端与查看器保留 JSON 数字的原始数字文本，但无法恢复调用方转换时已经丢失的精度。

成功写入会按需创建账号和分类文件，并替换字典中的一个顶层字段。[写入契约](<HTTP 协议.md#申请并使用-token>)说明 1 MiB 请求／文件上限与事务边界；不会自动同步本地存档。

## Token 与超时

写入 token 绑定项目、账号和当前客户端会话，有效期**至多 60 秒**，只允许**一次写入**。客户端在发送前消费有效 token。写入失败或超时不会自动重试：先读取字段，再决定是否申请新 token 并重新写入。后端的 token 处理顺序与重启失效规则见 [HTTP 写入契约](<HTTP 协议.md#申请并使用-token>)。

服务器还会按项目和客户端 IP 限制写入频率，间隔可配置。完整规则见 [HTTP 写入限频](<HTTP 协议.md#写入限频>)。

被拒绝的写入以 `ServerErrorCode.RateLimited` 完成，`retryAfterSeconds` 向上取整到秒。**有效 token 已经被消费。** 至少等待返回的时间，再在准备写入时申请新 token；默认间隔比 token 有效期更长。

普通操作的期限为十秒，网络检测为三秒。网络工作不在游戏逻辑线程执行，完成结果由下一次操作更新交付。系统 DNS 解析可能在取消后仍被阻塞，停止游戏时可能等待解析器返回；不保证 DNS 可取消或退出具有硬性截止时间。

## 网络质量

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

| `GlobalCore.ServerNetworkState` | 含义 |
|---|---|
| `Normal` | 在 300 ms 以内收到有效服务器健康响应。 |
| `Weak` | 超过 300 ms，但在三秒期限内收到有效响应。 |
| `Disconnected` | 在期限内没有有效响应，或请求失败、功能未启用。 |

它检测配置的服务器，不判断整个互联网是否可用。健康接口不验证项目密钥，因此 Normal 不代表后续读写一定能通过认证。

## 错误码

`GlobalCore.ServerErrorCode` 包含 `None`、`Disabled`、`InvalidArgument`、`AuthenticationFailed`、`InvalidWriteToken`、`Timeout`、`ConnectionFailed`、`InvalidResponse`、`StorageFailure`、`ServerFailure`、`RateLimited`。

| 枚举值 | 含义 |
|---|---|
| `None` | 操作成功。 |
| `Disabled` | 当前构建没有启用 Ludork Server 传输。 |
| `InvalidArgument` | 名称、URL、JSON 值或大小不合法。 |
| `AuthenticationFailed` | 项目密钥或管理认证被拒绝。 |
| `InvalidWriteToken` | Token 缺失、过期、会话/账号/项目不匹配，或已消费。 |
| `Timeout` | 操作期限已到。 |
| `ConnectionFailed` | 传输无法建立或维持通信。 |
| `InvalidResponse` | 对端返回不合法的 HTTP、JSON 或操作响应。 |
| `StorageFailure` | 后端无法读取或保存数据。 |
| `ServerFailure` | 其他后端错误。 |
| `RateLimited` | 该项目对此客户端 IP 的写入间隔尚未结束；按 `retryAfterSeconds` 等待后申请新 token。 |

禁用构建保留相同的 API 与异步结果形式。未提供回调属于编程错误，会在调用边界拒绝；请求结果通过上述类型返回。

直接 HTTP 客户端与管理接口见 [HTTP 协议](<HTTP 协议.md>)。
