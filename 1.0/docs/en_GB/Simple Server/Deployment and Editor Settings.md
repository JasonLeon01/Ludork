# Simple Server: Deployment and Editor Settings

Ludork Server is an optional HTTP data store. Each project contains accounts; each account contains category files, and each file is a JSON dictionary. The React/TypeScript management page lists accounts and files and displays their contents. It is read-only. The server provides neither multiplayer synchronization nor RPC, and it does not replace the game's local save system.

Deploy the backend and frontend on a server or a separate test instance. A game client only needs a server URL and project key; it does not install Node.js or deploy the server. Client integration requires a **C++ Source** project. Standalone hides both server configuration entries.

## Obtain the source

Open [Ludork Releases](https://github.com/JasonLeon01/Ludork/releases) and download `LudorkServer-source-<version>.zip` from the release's **Assets**. Its version matches the editor packages in the same release; for example, editor version `1.0.0` uses `LudorkServer-source-1.0.0.zip`.

The archive contains the backend, frontend and deployment scripts. Extract it on the server, open the extracted directory and follow the deployment steps below.

## Deploy and start

Install **Node.js 24 with npm** on the server. In the extracted directory, run:

```sh
sh deploy.sh MyGame 'project-key' 'management-password'
sh start.sh
```

Windows Command Prompt uses the equivalent scripts:

```bat
deploy.bat MyGame "project-key" "management-password"
start.bat
```

The three deployment arguments are the project name, project key and management password. Quote and escape them for the shell, especially `$`, `%`, `!`, `&` and quotation marks. Arguments can remain in shell history and process listings; do not place real credentials in checked-in scripts.

Deployment runs `npm ci`, saves credential hashes and generates the start scripts. Node.js runs the backend TypeScript directly; Vite runs the React/TypeScript frontend. **There is no frontend production build.** Start launches both services, fails if either port is occupied and stops the other service when a child fails. Ctrl+C stops both. Deployment does not install a system service or configure automatic startup.

| Purpose | Address |
|---|---|
| Management website | `http://IP:3333/MyGame/` |
| Game client URL | `http://IP:7777/MyGame` |

All projects in one server instance share ports **3333** and **7777**. Run deploy again with another project name to add it; redeploying the same name changes its credentials while preserving data. Deploy sequentially and stop the instance while reinstalling dependencies. Restart it to load configuration changes. Projects have separate keys, passwords, data, tokens and management sessions.

IP addresses work directly. To access Vite by a domain, set `LUDORK_ALLOWED_HOSTS=games.example.com` before starting; multiple permitted hostnames are comma-separated, without scheme or port. Configure DNS and network/firewall access separately.

Project names contain 1–64 UTF-8 bytes, excluding control characters, `/`, `\` and the exact names `.` and `..`. Encode the project as one URL path segment; Chinese names are supported. Keys contain 1–4096 visible ASCII characters without spaces. Passwords contain 1–4096 UTF-8 bytes without control characters. No default credentials are supplied.

## Configure the write interval

Each project has an `ipWriteIntervalSeconds` field in `config/projects.json`, under `projects["MyGame"]` for the example project. New deployments set it to **180 seconds**; omitting it also selects 180. Edit this field in the existing project object, preserving its credential hashes and other projects:

```json
{"ipWriteIntervalSeconds":180}
```

This is a project-object fragment, not a replacement configuration file. Set the value to `0` for unlimited writes, or a nonnegative integer up to `9007199254740991` to choose another interval. Fractional values, strings, null and negative numbers are rejected. Redeploying a project preserves its existing interval and data. **Restart the server after editing the setting**; the current process does not reload it automatically.

The interval is shared by all writes from the same TCP client IP to the same project, across accounts and categories. IPv4-mapped IPv6 addresses are normalized; `X-Forwarded-For` is ignored. Users sharing a NAT or reverse proxy may therefore share one write allowance. Other projects have independent allowances. Reads, list queries, token requests and management traffic do not use it.

The interval starts when a validated, authenticated write is admitted, before storage; a later storage failure still uses the allowance. Requests rejected before admission do not start it, and HTTP 429 does not extend an existing interval. Deadlines are kept in memory and reset on restart. A rate-limited write consumes its valid token, so wait for `retryAfterSeconds`, then acquire a fresh token before trying again. See the [HTTP rate-limit response](<HTTP Protocol.md#write-rate-limits>) and [native result handling](<Native Client API.md#token-and-timeout-behavior>).

## Configure a test client

In a C++ Source project, open **Game → Test Server Settings**. Enable Ludork Server and enter the test URL and key. The initial URL is `http://localhost:7777/<project-directory-name>`, with the directory name URL-encoded. Confirm saves; Cancel leaves the saved configuration unchanged. This window configures the client only; deploy and start the test server yourself.

The local file is:

- Windows: `.localserver/<project-directory-name>/test.json` beside `Ludork.ini`.
- Other editor platforms: `~/Ludork/.localserver/<project-directory-name>/test.json`.

```json
{
  "enabled": true,
  "url": "http://localhost:7777/MyGame",
  "key": "test-project-key"
}
```

These are local settings, including a plaintext test key. They are ignored by Git and are not written to `Main.proj` or Lua. Projects with the same directory name share this local settings location. A missing file or `enabled: false` disables the integration.

**Construct** builds Debug and **Play** runs Debug. Changing the effective test settings makes the native build stale, so construct again or accept Build and Play. Debug build and native-state checks read the file through `LUDORK_SERVER_CONFIG_FILE`; a prewarmed checker reads its current contents on each check.

## Configure a packaged client

In **File → Pack Project**, select **Enable Ludork Server**, then enter the production URL and key. Each newly opened Pack window starts disabled with empty fields. Production settings are not cached or copied from test settings. Use `http://host:7777/project`, without `/api/v1`, credentials, query parameters or a fragment. HTTPS is not supported by this client integration.

Pack always builds **Release**, including internal test packages marked dev. Release ignores `LUDORK_SERVER_CONFIG_FILE`. For command-line packaging or CI, supply `LUDORK_SERVER_ENABLED=1`, `LUDORK_SERVER_URL` and `LUDORK_SERVER_KEY` in that process's environment; omit enablement or set it to `0` to disable the feature. Obtain production values from your own secret store instead of putting literals in version-controlled commands.

For example, attach these environment values to your normal GitHub Actions game-packaging step after configuring the two repository secrets. This Windows example uses the existing packaging command; do not print the secret values:

```yaml
- name: Package game
  shell: cmd
  env:
    LUDORK_SERVER_ENABLED: '1'
    LUDORK_SERVER_URL: ${{ secrets.LUDORK_SERVER_URL }}
    LUDORK_SERVER_KEY: ${{ secrets.LUDORK_SERVER_KEY }}
  run: tools\pack_project.bat --release Game
```

An enabled build defines `LUDORK_SERVER_AVAILABLE` and compiles the transport with its URL/key. A disabled build leaves the macro undefined and excludes the network implementation; the Lua entry points remain available, print a disabled message and complete with `ServerErrorCode.Disabled`. Packaging removes the generated Release configuration header after execution, including editor cancellation. The resulting binary necessarily contains the credentials it needs.

## Management and operational limits

Open the website and enter both the project key and management password. It supports account filtering, file selection, JSON display, refresh and logout. Sessions use a project-specific HttpOnly, SameSite=Strict cookie, expire after eight hours and become invalid on backend restart.

Generated `config/`, user `data/`, logs, dependencies and start scripts are excluded from source archives. Back up `config/` and `data/` yourself and restrict local access to them. The backend holds a data-directory instance lock. A stale lock whose process has exited can be recovered automatically; for an incomplete lock or reused PID, first confirm that no instance is running before removing `data/.server.lock` manually.

Use a trusted network: **HTTP sends credentials and data in plaintext**. The shared project key grants access to every account in that project; an account ID is a storage namespace, not a separately authenticated user. A key embedded in a distributed binary can be extracted. One-use tokens limit replay but do not make an untrusted client authoritative. Network access controls or a separately managed proxy are deployment responsibilities; this project supplies no TLS setup or account-authentication system.

Continue with the [Native Client API](<Native Client API.md>) and [HTTP Protocol](<HTTP Protocol.md>).
