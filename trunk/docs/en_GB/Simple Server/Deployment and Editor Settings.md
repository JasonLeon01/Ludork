# Simple Server: Deployment and Editor Settings

Ludork Server is an optional Node.js service with a Lua logic layer providing explicit data snapshots, void RPCs and online events over WebSocket. Account data is stored in data-only `.ldata` category files and viewed through a read-only management website; gameplay code decides what to fetch, refresh or write, and the service neither synchronises multiplayer state automatically nor replaces local saves.

Deploy the backend and management frontend on a server or separate test instance; game clients require only a server URL and project key, with no local Node.js installation. Configuring client integration requires a **C++ Source** project. A converted **Standalone** project can use server settings compiled into its runtime, while hiding both server configuration menus.

## Obtain the server package

Open [Ludork Releases](https://github.com/JasonLeon01/Ludork/releases) and download `LudorkServer-<version>.tar.gz` from the release's **Assets**. Its version matches the editor packages in the same release (for example, editor version `1.0.0` uses `LudorkServer-1.0.0.tar.gz`). Temporary CI artefacts use `LudorkServer-<40-character-Server-commit-SHA>.tar.gz`.

The server is distributed independently. Development and release editor packages contain no Server directory or archive; download the archive separately and extract it to a dedicated deployment location. Installing or running the editor does not start the server.

The archive contains compiled backend JavaScript, the production management website, Lua modules and declarations, deployment/start scripts and locked production dependency manifests under `LudorkServer/`. It contains no installed `node_modules`, project credentials or user data. On Linux, extract and enter the directory:

```sh
tar -xzf LudorkServer-1.0.0.tar.gz
cd LudorkServer
```

To build from the `Server` branch yourself, install Node.js 24, run `npm ci` and `npm run build` in that checkout, then copy `dist/LudorkServer` to a separate deployment directory. The build compiles the backend and bundles the management website with Vite. See the [Server README](https://github.com/JasonLeon01/Ludork/blob/Server/README.md) for source development.

> **Important:** Deploy and start scripts refuse to run inside a source checkout, and rebuilding refuses to overwrite output containing `config/`, `data/`, `projects/` or `logs/`. Always keep deployed instances outside `dist/LudorkServer`.

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

The three arguments are the project name, project key and management password. Quote and escape them for your shell, especially `$`, `%`, `!`, `&` and quotation marks. Command arguments can persist in shell history and process listings; never place real credentials in checked-in scripts.

- **Dependencies and runtime:** Deployment runs `npm ci --omit=dev` to install locked production dependencies, stores credential hashes and creates a project Lua entry without compiling the backend or rebuilding the website. The pinned Wasmoon dependency includes Lua 5.4.5 in WebAssembly, so no system Lua installation or native compiler is required.
- **Ports and lifecycle:** Node.js runs the compiled backend on port `6666` and serves the built management website on port `3333`. Start launches both services, fails if either port is occupied and stops the sibling service if one fails; press `Ctrl+C` to stop both. Deployment does not install a system service or configure automatic startup.

| Purpose | Address |
|---|---|
| Management website | `http://IP:3333/MyGame/` |
| Game client URL | `http://IP:6666/MyGame` |
| Derived WebSocket endpoint | `ws://IP:6666/MyGame/api/v1/net` |

All projects in one server instance share ports **3333** and **6666** while keeping separate keys, passwords, data, Lua processes and management sessions. Run deploy again with another project name to add a project, or redeploy an existing name to rotate credentials while preserving its data, Lua source and project settings.

Deploy sequentially, stop the instance while reinstalling dependencies and restart it to load configuration or Lua source changes.

IP addresses, `localhost` and its subdomains work directly, alongside any [bound project domain](#bind-a-project-domain). To access the management website via another domain, set `LUDORK_ALLOWED_HOSTS=games.example.com` before starting (multiple permitted hostnames are comma-separated, exact-matched and omit scheme and port). Configure DNS and firewall rules separately.

| Field | Constraints |
|---|---|
| Project name | 1–64 UTF-8 bytes; no control characters, `/`, `\` or the exact names `.` and `..`. Encode as one URL path segment; Chinese names are supported. |
| Project key | 1–4096 visible ASCII characters without spaces. No default is supplied. |
| Management password | 1–4096 UTF-8 bytes without control characters. No default is supplied. |

## Project Lua entry

Deployment creates `projects/<hex-project-name>/Main.lua`, initially registering `Modules.Storage`. The directory name is the lowercase hexadecimal encoding of the project's UTF-8 bytes (printed during deployment). Place project-owned Lua modules there: the server loads framework modules from `lua/` and project modules from this private directory, and never sends project source to game clients or serves it from the management website.

Each project object in `config/projects.json` may set `luaEntry` (defaulting to `Main.lua`) to a relative `.lua` path within that project's directory using `/` separators. Absolute paths, empty segments, `.` or `..`, backslashes, colons, control characters and paths over 512 UTF-8 bytes are rejected. Redeployment preserves both `luaEntry` and any existing entry file. Restart the server after changing source or `luaEntry`.

Server Lua uses ordinary Lua 5.4 with no client Standard, `Engine` or `GlobalCore` modules. Public declarations live under `lua/stub` with a server-specific `.emmyrc.json`; declaration files and `stub` directories are excluded at runtime. See [Server Lua modules](<HTTP Protocol.md#server-lua-modules>) for module registration, context services and execution limits.

## Configure the write interval

Each project has an `ipWriteIntervalSeconds` field in `config/projects.json` (under `projects["MyGame"]` for the example project). New deployments set it to **180 seconds**, and omitting it selects 180. Edit this field inside the existing project object while preserving credential hashes and other projects:

```json
{"ipWriteIntervalSeconds":180}
```

This is a project-object fragment rather than a replacement file. Set `0` to disable the interval, or a non-negative integer up to `9007199254740991` to choose another duration; fractional values, strings, `null` and negative numbers are rejected. Redeploying a project preserves its interval and data. **Restart the server after editing** because the running process does not reload configuration automatically.

The interval applies only to the supplied `Storage.WriteField` RPC and is shared across writes from the same TCP client IP to the same project (users behind a shared NAT or reverse proxy share one allowance). Custom RPCs implement their own business rules and do not inherit it, and other projects have independent allowances.

See [write rate limits](<HTTP Protocol.md#write-rate-limits>) for admission, failure and restart behaviour, and [RPC lifecycle](<Native Client API.md#call-rpcs-and-receive-events>) for handling uncertain outcomes.

## Bind a project domain

A project object in `config/projects.json` may list hostnames in `hosts` so a domain such as `mygame.example.com` identifies the project without a project path segment:

```json
{"hosts":["mygame.example.com"]}
```

This is a project-object fragment like `ipWriteIntervalSeconds`. Each entry must be an exact lowercase hostname without scheme or port, and a hostname can belong to only one project; invalid or duplicate entries prevent the server from starting. Omitting `hosts` binds no domain. Redeploying preserves the list. **Restart the server after editing.**

For a request whose `Host` header matches a bound hostname, the backend serves `/api/v1/...` (including the WebSocket endpoint) as that project's `/<project>/api/v1/...`, letting game clients use `http://mygame.example.com:6666`. Explicit project paths work on every host, whereas unbound hosts expose no project at `/api/v1`.

The management website accepts bound hostnames at `http://mygame.example.com:3333/MyGame/`. Point the domain's DNS record at the server yourself; domain binding does not alter ports.

## Configure the development client

In a **C++ Source** project, open **Game → Test Server Settings**, enable Ludork Server and enter the test URL and key. The default URL is `http://localhost:6666/<project-directory-name>` with the directory name URL-encoded. Select **Confirm** to save or **Cancel** to discard changes. This dialog configures the client only; deploy and start the test server yourself.

Settings are stored locally at `~/Ludork/.localserver/<project-directory-name>/test.json` on Windows and macOS, including when running from a repository checkout (`~` is the current user's home directory):

```json
{
  "enabled": true,
  "url": "http://localhost:6666/MyGame",
  "key": "test-project-key"
}
```

These local settings (including the plaintext test key) are ignored by Git and never written to `Main.proj` or Lua. Projects sharing the same directory name share this file; a missing file or `"enabled": false` disables integration.

**Construct** builds Debug and **Play** runs Debug. Changing effective settings requires a new native build, so run **Construct** again or select **Build and Play**. Debug builds read the local file through `LUDORK_SERVER_CONFIG_FILE`.

## Configure a packaged client

In **File → Pack Project**, enable **Enable Ludork Server** and enter the production URL and key. Each newly opened Pack window starts disabled with empty fields; production settings are never cached or copied from test settings.

Use `http://host:6666/project` (or `http://host:6666` when `host` is [bound to the project](#bind-a-project-domain)) without `/api/v1`, credentials, query parameters or a fragment. Native clients use HTTP/WS; the WebGL target also accepts HTTPS and derives WSS.

An HTTPS game page requires an HTTPS/WSS server endpoint; the client rejects HTTP/WS (including `localhost`) before sending a request. Terminate TLS through a reverse proxy in front of the HTTP server and forward both project routes and WebSocket upgrades.

Browser health checks are public requests without credentials and allow cross-origin access; management APIs are restricted to their same-origin website. Native and browser clients both authenticate via the first WebSocket message in [WebSocket messages](<HTTP Protocol.md#websocket-messages>). Keep client and server builds aligned with this protocol.

[Standalone project conversion](<../Editor User Guide/Run Debug and Package.md#convert-source-to-a-standalone-project>) offers the same opt-in Release configuration. The converted editable project uses that compiled connection, which cannot be changed without another **C++ Source** build.

Pack always builds **Release** (including internal test packages marked dev) and ignores `LUDORK_SERVER_CONFIG_FILE`. For CLI or CI packaging, supply `LUDORK_SERVER_ENABLED=1`, `LUDORK_SERVER_URL` and `LUDORK_SERVER_KEY` in the process environment; omit `LUDORK_SERVER_ENABLED` or set it to `0` to disable integration. Fetch production values from a secret store rather than embedding literals in version-controlled commands.

For example, pass these environment variables to a GitHub Actions packaging step on Windows using repository secrets (never echo secret values):

```yaml
- name: Package game
  shell: cmd
  env:
    LUDORK_SERVER_ENABLED: '1'
    LUDORK_SERVER_URL: ${{ secrets.LUDORK_SERVER_URL }}
    LUDORK_SERVER_KEY: ${{ secrets.LUDORK_SERVER_KEY }}
  run: tools\pack_project.bat --release Game
```

An enabled build defines `LUDORK_SERVER_AVAILABLE` and embeds the transport, URL and key in the binary. When disabled, server calls print a diagnostic and return immediately; see [Disabled server integration](<Native Client API.md#disabled-server-integration>) for return values and local operations available when disabled.

## Management and operational limits

Open the management website and sign in with both the project key and management password. It supports account filtering, category selection, typed Lua value inspection (including exact integers and escaped byte strings), refresh and logout. See [read-only management](<HTTP Protocol.md#http-health-and-read-only-management>) for cookie scope and session expiry.

Generated `config/`, private `projects/` Lua source, user `data/`, logs and installed dependencies are excluded from the distributed server package. Back up `config/`, `projects/` and `data/` yourself and restrict local access to them.

The backend holds a data-directory instance lock. A stale lock from an exited process recovers automatically; for an incomplete lock or reused PID, confirm no instance is running before removing `data/.server.lock` manually.

> **Warning:** HTTP and WebSocket send credentials and data in plaintext, and a key embedded in a distributed binary can be extracted. The shared project key grants access to all registered modules for that project; an account ID is only a storage namespace, not an authenticated user. Enforce account authorisation and business rules in project Lua, and deploy TLS and network access controls externally through a reverse proxy or trusted network.

Continue with the [Native Client API](<Native Client API.md>) and [Server Lua and Protocol](<HTTP Protocol.md>).
