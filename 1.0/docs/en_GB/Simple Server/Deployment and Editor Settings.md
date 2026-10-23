# Simple Server: Deployment and Editor Settings

Ludork Server is an optional Node.js service with a Lua logic layer. Projects expose explicit data snapshots, void RPCs and online events over WebSocket. Stored account data uses data-only `.ldata` category files, and the read-only management website displays typed Lua values. Gameplay decides what to fetch, refresh or write; this service does not automatically synchronize multiplayer state or replace local saves.

Deploy the backend and frontend on a server or a separate test instance. A game client only needs a server URL and project key; it does not install Node.js or deploy the server. Configuring client integration requires a **C++ Source** project. A converted Standalone project can retain server settings compiled into its runtime, but hides both server configuration entries.

## Obtain the source

Open [Ludork Releases](https://github.com/JasonLeon01/Ludork/releases) and download `LudorkServer-source-<version>.tar.gz` from the release's **Assets**. Its version matches the editor packages in the same release; for example, editor version `1.0.0` uses `LudorkServer-source-1.0.0.tar.gz`.

The archive contains the backend, frontend and deployment scripts under `LudorkServer/`. On Linux, extract it and enter that directory:

```sh
tar -xzf LudorkServer-source-1.0.0.tar.gz
cd LudorkServer
```

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

Deployment runs `npm ci`, saves credential hashes, creates a project Lua entry and generates the start scripts. The pinned Wasmoon dependency contains its Lua 5.4.5 WebAssembly runtime; no system Lua installation or native compiler is needed. Node.js runs the backend TypeScript directly; Vite runs the React/TypeScript frontend. **There is no frontend production build.** Start launches both services, fails if either port is occupied and stops the other service when a child fails. Ctrl+C stops both. Deployment does not install a system service or configure automatic startup.

| Purpose | Address |
|---|---|
| Management website | `http://IP:3333/MyGame/` |
| Game client URL | `http://IP:7777/MyGame` |
| Derived WebSocket endpoint | `ws://IP:7777/MyGame/api/v1/net` |

All projects in one server instance share ports **3333** and **7777**. Run deploy again with another project name to add it; redeploying the same name changes its credentials while preserving data, Lua source and project settings. Deploy sequentially and stop the instance while reinstalling dependencies. Restart it to load configuration or Lua source changes. Projects have separate keys, passwords, data, Lua processes and management sessions.

IP addresses work directly. To access Vite by a domain, set `LUDORK_ALLOWED_HOSTS=games.example.com` before starting; multiple permitted hostnames are comma-separated, without scheme or port. Configure DNS and network/firewall access separately.

Project names contain 1–64 UTF-8 bytes, excluding control characters, `/`, `\` and the exact names `.` and `..`. Encode the project as one URL path segment; Chinese names are supported. Keys contain 1–4096 visible ASCII characters without spaces. Passwords contain 1–4096 UTF-8 bytes without control characters. No default credentials are supplied.

## Project Lua entry

Deployment creates `projects/<hex-project-name>/Main.lua`, initially registering `Modules.Storage`. The directory name is the lowercase hexadecimal encoding of the project's UTF-8 bytes; deployment prints the exact path. Put project-owned Lua modules there. The server loads framework modules from `lua/` and project modules from this private directory. Project source is never sent to game clients or served by the management website.

Each project object in `config/projects.json` may set `luaEntry`, defaulting to `Main.lua`. It is a relative `.lua` file path within that project's directory, using `/` between directories; absolute paths, empty components, `.`/`..`, backslashes, colons and control characters are rejected. The path may contain at most 512 UTF-8 bytes. Redeployment preserves the setting and an existing entry file. Restart after changing source or the entry path.

Server Lua uses ordinary Lua 5.4, with no client Standard, `Engine` or `GlobalCore` modules. Public declarations are under `lua/stub`, with a server-specific `.emmyrc.json`; declaration files and `stub` directories are excluded from the runtime. See [Server Lua modules](<HTTP Protocol.md#server-lua-modules>) for module registration, context services and execution limits.

## Configure the write interval

Each project has an `ipWriteIntervalSeconds` field in `config/projects.json`, under `projects["MyGame"]` for the example project. New deployments set it to **180 seconds**; omitting it also selects 180. Edit this field in the existing project object, preserving its credential hashes and other projects:

```json
{"ipWriteIntervalSeconds":180}
```

This is a project-object fragment, not a replacement configuration file. Set the value to `0` to disable this interval, or a nonnegative integer up to `9007199254740991` to choose another interval. Fractional values, strings, null and negative numbers are rejected. Redeploying a project preserves its existing interval and data. **Restart the server after editing the setting**; the current process does not reload it automatically.

The interval applies only to the supplied `Storage.WriteField` RPC, shared across its writes from the same TCP client IP to the same project. Custom RPCs implement their own business rules and do not inherit it. Users sharing a NAT or reverse proxy may share one allowance. Other projects have independent allowances.

See [write rate limits](<HTTP Protocol.md#write-rate-limits>) for admission, failures and restart behaviour, and [RPC lifecycle](<Native Client API.md#call-rpcs-and-receive-events>) for handling uncertain outcomes.

## Configure the development client

In a C++ Source project, open **Game → Test Server Settings**. Enable Ludork Server and enter the test URL and key. The initial URL is `http://localhost:7777/<project-directory-name>`, with the directory name URL-encoded. Confirm saves; Cancel leaves the saved configuration unchanged. This window configures the client only; deploy and start the test server yourself.

The local file is `~/Ludork/.localserver/<project-directory-name>/test.json` on Windows and macOS, including repository runs. `~` is the current user's home directory.

```json
{
  "enabled": true,
  "url": "http://localhost:7777/MyGame",
  "key": "test-project-key"
}
```

These are local settings, including a plaintext test key. They are ignored by Git and are not written to `Main.proj` or Lua. Projects with the same directory name share this local settings location. A missing file or `enabled: false` disables the integration.

**Construct** builds Debug and **Play** runs Debug. Changing the effective settings requires a new native build, so construct again or accept Build and Play. Debug builds read the local file through `LUDORK_SERVER_CONFIG_FILE`.

## Configure a packaged client

In **File → Pack Project**, select **Enable Ludork Server**, then enter the production URL and key. Each newly opened Pack window starts disabled with empty fields. Production settings are not cached or copied from test settings. Use `http://host:7777/project`, without `/api/v1`, credentials, query parameters or a fragment. HTTPS is not supported by this client integration.

[Standalone project conversion](<../Editor User Guide/Run Debug and Package.md#convert-source-to-a-standalone-project>) offers the same opt-in Release configuration. The resulting editable project keeps the compiled connection and cannot modify it without another Source build.

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

An enabled build defines `LUDORK_SERVER_AVAILABLE` and includes the transport with its URL and key. When disabled, the networking APIs remain available: `Global.Net` reports the string code `Disabled`, while the native health probe uses `ServerErrorCode.Disabled`. The resulting binary contains the credentials it needs.

## Management and operational limits

Open the website and enter both the project key and management password. It supports account filtering, category selection, typed Lua data display, refresh and logout, including exact integers and escaped byte strings. For cookie scope and session expiry, see [read-only management](<HTTP Protocol.md#http-health-and-read-only-management>).

Generated `config/`, private `projects/` Lua source, user `data/`, logs, dependencies and start scripts are excluded from source archives. Back up `config/`, `projects/` and `data/` yourself and restrict local access to them. The backend holds a data-directory instance lock. A stale lock whose process has exited can be recovered automatically; for an incomplete lock or reused PID, first confirm that no instance is running before removing `data/.server.lock` manually.

Use a trusted network: **HTTP and WebSocket send credentials and data in plaintext**. The shared project key grants access to that project's registered modules; an account ID is a storage namespace, not a separately authenticated user. A key embedded in a distributed binary can be extracted. Project Lua must enforce its own account authorization and business rules. Network access controls or a separately managed proxy are deployment responsibilities; this project supplies no TLS setup or account-authentication system.

Continue with the [Native Client API](<Native Client API.md>) and [Server Lua and Protocol](<HTTP Protocol.md>).
