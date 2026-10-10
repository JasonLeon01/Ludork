# HTTP 协议

API 基地址为 `http://主机:7777/<项目名>/api/v1`。项目、账号、大类和字段名分别编码为独立的 URL 路径段，路由与名称均区分大小写。[原生客户端](<原生客户端 API.md>)会在内部构造这些请求。

账号、大类和字段名允许 1–64 个 UTF-8 字节，不含控制字符，也不能是完整的 `.` 或 `..`；在路径段中编码后可以包含 `/` 或 `\`。项目名另外禁止这两种斜杠。服务器将项目、账号和大类名转换成小写十六进制 UTF-8 文件名，避免文件系统大小写折叠、保留名称与路径越界。分类文件是以字段名为键的字典。

## 游戏数据接口

| API 基地址之后的方法与路径 | 认证 | 成功响应 |
|---|---|---|
| `GET /health` | 无 | `{"service":"LudorkServer","project":"MyGame"}` |
| `GET /accounts?sampleCount=10&category=history` | `X-Ludork-Key` | `{"accounts":["player-1"]}` |
| `GET /accounts/:account` | `X-Ludork-Key` | `{"exists":true}` 或 `{"exists":false}` |
| `GET /accounts/:account/categories/:category/fields?limit=50` | `X-Ludork-Key` | `{"entries":[{"key":"...","value":...}],"nextCursor":null}` |
| `GET /accounts/:account/categories/:category/fields/:field` | `X-Ludork-Key` | `{"found":true,"value":...}` |
| `POST /accounts/:account/write-tokens` | `X-Ludork-Key` | `{"token":"...","expiresAt":...}` |
| `PUT /accounts/:account/categories/:category/fields/:field` | `Authorization: Bearer <token>` | `{}` |

成功响应为 HTTP 200，`Content-Type: application/json`。账号查询、读取、申请 token、写入分别进行认证；健康响应仅表示可达。

### 列出账号

```http
GET /MyGame/api/v1/accounts?sampleCount=10&category=history
X-Ludork-Key: project-key
```

```json
{"accounts":["player-2","player-1"]}
```

两个查询参数均可省略：

| 参数 | 含义 |
|---|---|
| `sampleCount` | 省略或 `0`：按 UTF-8 字节升序返回全部符合条件的账号。正数：无放回随机抽取，最多返回指定数量，顺序随机。接受 `0` 至 `9223372036854775807` 的十进制整数。 |
| `category` | 只保留指定分类以普通文件形式存在的账号，先筛选再抽样。该值按查询参数编码。 |

符合条件的账号不足指定数量时，返回全部符合条件的账号；没有匹配项时，成功返回 `{"accounts":[]}`。分类筛选只检查文件存在性，不检查 JSON 是否有效：损坏内容仍符合筛选条件，但随后读取时返回 `StorageFailure`。分类路径不是普通文件或检查失败时，查询也会失败。不创建账号或文件。

序列化后的响应不能超过 2 MiB。查询全部账号或抽样过大时，返回 HTTP 400 `InvalidArgument`，消息为 `The account list exceeds 2 MiB. Request a smaller positive sampleCount.`，不截断列表，也不返回账号游标。不接受未知、重复、空值或不合法的查询参数。

### 查询账号是否存在

```http
GET /MyGame/api/v1/accounts/player-1
X-Ludork-Key: project-key
```

```json
{"exists":false}
```

账号数据目录存在时，`exists` 为 true，空目录也算，与管理账号列表一致。目录不存在时返回 HTTP 200 和 `exists: false`。账号路径不是目录或检查失败时返回 HTTP 500 `StorageFailure`，不会伪装成账号不存在。该查询不检查分类文件内容。

账号查询、读取字段和签发 token 均不创建账号；首次写入时按需创建账号目录。查询不预留 ID，也不提供原子的防重保证，其他客户端可能在查询后写入。UUID 标识由客户端自行生成，不提供 UUID 生成接口。

### 列出分类字段

```http
GET /MyGame/api/v1/accounts/player-1/categories/history/fields?limit=2
X-Ludork-Key: project-key
```

```json
{
  "entries": [
    {"key":"2026-10-10T12:30:00.123Z","value":{"score":100}},
    {"key":"2026-10-10T12:20:00.123Z","value":null}
  ],
  "nextCursor":"2026-10-10T12:20:00.123Z"
}
```

`limit` 默认为 50，接受 1–100 的整数。字段按 UTF-8 字节降序排列。可选的 `before` 是排他的上界，只返回 UTF-8 字节小于该值的键，不要求边界键实际存在。它遵循字段名规则，并需要按查询参数进行 URL 编码。首页省略该参数，不能传空值。

还有后续字段时，`nextCursor` 为本页最后一个键，将它原样作为下一页的 `before`，例如 `?before=2026-10-10T12%3A20%3A00.123Z&limit=2`；否则为 JSON null。账号或分类不存在、范围已遍历完时，成功返回 `{"entries":[],"nextCursor":null}`；分类内容损坏则返回 `StorageFailure`。不接受未知或重复的查询参数。

每页读取当前分类，并发写入期间不提供跨页快照。并发插入的大于当前游标的键，需要从首页刷新才能进入遍历范围。如果希望字节顺序代表时间，由应用生成固定宽度、统一格式和时区的时间戳键。服务器不生成时间戳，也不按字段值排序；重复使用键会替换原值。列表查询不创建数据。序列化后的分页响应超过 2 MiB 时，返回 HTTP 400 `InvalidArgument` 并提示减小 limit。

### 读取字段

```http
GET /MyGame/api/v1/accounts/player-1/categories/profile/fields/score
X-Ludork-Key: project-key
```

```json
{"found":true,"value":100}
```

账号、大类或字段不存在时，返回 `{"found":false,"value":null}`；已存储的 null 返回 `{"found":true,"value":null}`。读取不创建文件。

### 申请并使用 token

```http
POST /MyGame/api/v1/accounts/player-1/write-tokens
X-Ludork-Key: project-key
```

响应包含由密码学随机数生成的不透明 token，以及以**毫秒**为单位的 Unix 时间戳 `expiresAt`。它绑定项目与账号，从签发起有效 60 秒，只保存在内存。用它执行一次写入：

```http
PUT /MyGame/api/v1/accounts/player-1/categories/profile/fields/score
Authorization: Bearer <token>
Content-Type: application/json

{"value":100}
```

请求体必须是具有自身 `value` 成员的 JSON 对象。值可以是 JSON null、布尔值、数字、字符串、数组或对象。Null 是存储值，不表示删除。后端校验请求后同步消费匹配的 token，再检查写入间隔、等待存储，因此并发使用同一 token 不会同时成功。存储失败或 HTTP 429 都会消费 token；账号或项目不匹配的请求不会消费属于其他目标的有效 token。后端重启后所有 token 失效。

请求体与完整分类文件均限制为 **1 MiB**，文件上限包括末尾换行。同一文件的更新顺序执行，多个字段的并发写入不会互相覆盖；保存通过替换临时文件完成，不同文件可以独立更新。不提供跨文件事务、删除接口或自动备份。若写入响应丢失，应先读取，仅在确实需要再次写入时申请新 token。

Node.js 24 读取原始数字字面量，并通过原生 `JSON.rawJSON` 序列化。后端保留整数的精确数字，包括有符号和无符号 64 位数值。查看器在浏览器支持时使用同一原生能力，否则直接显示原始 JSON 文本，避免展示经过舍入的值。`rawJSON`、`isLosslessNumber`、`__proto__` 等普通对象字段仍然是普通数据。

### 写入限频

默认按**项目与 TCP 客户端 IP**设置 180 秒的写入间隔，该项目所有账号和分类的 PUT 请求共用额度。IPv4 映射的 IPv6 地址会统一为 IPv4，忽略 `X-Forwarded-For`，因此同一 NAT 或反向代理后的客户端可能共用写入额度。按[部署与编辑器设置](<部署与编辑器设置.md#配置写入间隔>)配置 `ipWriteIntervalSeconds`，设为 `0` 可关闭间隔限制。

请求在准入前因参数或认证失败，不开始计时。一旦获准写入，便会在存储前开始间隔，即使后续存储失败也占用额度；被拒绝的写入不延长原截止时间。读取、列表、账号检查、健康检测、签发 token 与管理请求均不计入。截止时间仅保存在内存，后端重启后清空。

在间隔内携带有效 token 的 PUT 返回：

```http
HTTP/1.1 429 Too Many Requests
Content-Type: application/json
Retry-After: 120

{"code":"RateLimited","message":"This IP address must wait before writing to this project again.","retryAfterSeconds":120}
```

JSON `retryAfterSeconds` 与 `Retry-After` 响应头携带相同的剩余等待时间，向上取整到秒。该可选 JSON 属性只在 `RateLimited` 时提供。有效 token 在限频检查之前已经消费。至少等待该时间，再在准备写入时申请**新 token**；不能复用旧 token，也不要先申请 token 再等待很长时间。客户端不会自动重试。

## 只读管理接口

管理路径位于 `<项目名>/api/v1/admin` 下。3333 端口的 Vite 将请求代理到 7777，认证由后端执行，不启用 CORS。

| `/admin` 之后的方法与路径 | 要求 | 成功响应 |
|---|---|---|
| `POST /login` | JSON `{"key":"...","password":"..."}`，以及 `X-Ludork-Admin: 1` | `{"project":"MyGame"}`，并设置会话 Cookie |
| `GET /session` | 会话 Cookie | `{"project":"MyGame"}` |
| `POST /logout` | 会话 Cookie 与 `X-Ludork-Admin: 1` | `{}`，清除会话 Cookie |
| `GET /accounts` | 会话 Cookie | `{"accounts":["player-1"]}` |
| `GET /accounts/:account/categories` | 会话 Cookie | `{"categories":["profile"]}` |
| `GET /accounts/:account/categories/:category` | 会话 Cookie | 分类字典 |

会话绑定项目，八小时后过期，退出登录或后端重启后失效。Cookie 为 HttpOnly、SameSite=Strict，路径限定在该项目的管理 API 下。管理接口不提供写入或删除。响应使用 `Cache-Control: no-store`，前端不会将凭据保存到浏览器存储。

## 错误与信任边界

非 2xx 响应包含稳定错误码和诊断文本：

```json
{"code":"AuthenticationFailed","message":"Project credentials are invalid."}
```

| HTTP 状态 | 错误码与常见原因 |
|---|---|
| `400` | `InvalidArgument`：名称、JSON、编码或大小不合法。 |
| `401` | `AuthenticationFailed` 或 `InvalidWriteToken`。 |
| `403` | `AuthenticationFailed`：缺少管理请求头。 |
| `404` | `InvalidArgument`：项目/接口不存在，或管理请求的分类不存在。 |
| `429` | `RateLimited`：项目/IP 的写入间隔尚未结束，附带 `retryAfterSeconds` 和 `Retry-After`。 |
| `500` | `StorageFailure` 或 `ServerFailure`。 |

协议通过 HTTP 明文传输密钥、token、Cookie 与数据。项目密钥能访问项目中的所有账号，不是账号认证；把密钥分发在客户端中也不能防止提取。向可信网络之外开放前，请阅读[部署边界](<部署与编辑器设置.md#管理与运行边界>)。
