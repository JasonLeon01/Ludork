# Official Blueprint AI

Configure the official Blueprint AI plug-in (`Ludork.OfficialBlueprintAI`, source in `Plugins/OfficialBlueprintAI`) to inspect your project, discuss changes to one existing Blueprint and review a validated diff before applying it. Open the assistant from **Game → Blueprint AI**.

## Workflow

1. Install the official plug-ins as described in [Official release plug-ins](<../Installing and Managing Plug-ins.md#official-release-plug-ins>).
2. Open a project and select **Game → Blueprint AI**.
3. Open **Settings** to configure your provider, model, endpoint and API key.
4. Select **New** to create a conversation bound to one existing Blueprint, then review any staged proposal and generated diff.
5. Select **Apply** after validation succeeds, then use the normal project Save command when ready to write the change to disk.

## Local data

Settings and conversation history are stored under `~/Ludork/Plugins/.data/Ludork.OfficialBlueprintAI`. Uninstalling the plug-in retains this directory; see [Data and uninstall](<../Installing and Managing Plug-ins.md#data-and-uninstall>) for storage paths and explicit removal.

## Scope and permissions

- **Read scope:** Each conversation binds to one project and one existing Blueprint. The assistant can inspect other Blueprints, Lua metadata, allowlisted Lua/C/C++ source files and official Ludork documentation to understand available APIs.
- **Write boundaries:** The assistant cannot create, rename or delete Blueprints, edit multiple Blueprints, modify source code, run shell commands or start processes.
- **Review and Apply:** The model can only stage a Blueprint proposal; the editor validates the candidate and renders a generated diff, changing nothing until you select **Apply**. Applying re-reads the editor's working copy, verifies its base revision, records one normal Undo step, marks the project dirty and refreshes all open editors for that Blueprint without saving to disk.

> **Note:** Plug-ins run as trusted in-process code rather than inside an operating-system sandbox. The restrictions above are enforced by the official assistant's model-facing tool surface together with the editor's session and Apply boundaries.

## Provider settings and credentials

Open **Settings** in the Blueprint AI window to configure a provider profile, model and endpoint:

- **Supported profiles:** OpenAI uses the Responses API; DeepSeek, Google OpenAI-compatible and Custom profiles use an OpenAI-compatible Chat Completions endpoint. Model and endpoint fields remain editable so new model releases need no plug-in update.
- **Endpoint validation:** Endpoints must be absolute `https://` URLs without embedded credentials, query strings or fragments. Plain `http://` is permitted only for loopback addresses during local development.
- **API keys:** Keys are stored in the [host secret store](<../Avalonia UI Secrets and Localisation.md#store-secrets>) and never written to `settings.json`. The UI shows only the first three and last three characters separated by a fixed mask (short keys are completely hidden), and updates a stored credential only when you enter a new value or explicitly remove it. Provider availability is outside Ludork's control.

## Local conversations and provider state

The local index and append-only JSONL files in the plug-in data directory are the authoritative conversation history. They store messages, model snapshots, compact tool activity and proposal decisions, and never record API keys or full source-file reads.

OpenAI Responses requests set `store:false`, avoiding reliance on provider-side conversation state. For long conversations, only a rolling summary and recent messages are sent in subsequent requests; this context compaction never deletes or alters the complete local history.

An interrupted request is recorded as interrupted: completed local entries survive restart, while the in-flight network request is never resumed automatically.

## Project source inspection

Project search and file reads accept only project-relative paths under the configured runtime and API source roots with supported text extensions. Build outputs, logs, version-control metadata, INI/plist configs, plug-in data, credentials and out-of-project paths are rejected, as are symbolic links, broken links and reparse points anywhere in the path (including the project root). Filesystem failures return tool errors, and file contents are treated strictly as data rather than instructions that can expand permissions.

## Official documentation

The assistant searches and reads the development or release documentation selected by the editor in the editor's active language, matching the [online documentation](<../../Editor User Guide/Start Page Workspace and Shortcuts.md#online-documentation>) version rules and returning links to the source pages.

If the matching documentation index is unavailable, the assistant reports the failure without falling back to another version or language. The current project's API catalogue and metadata remain authoritative for concrete nodes, types, serialised names and pins. See [Blueprint assistants](<../Registration and Hook Reference.md#blueprint-assistants>) for the underlying host API.

## Troubleshooting

- **Blueprint AI is missing from Game:** Install the [official release plug-ins](<../Installing and Managing Plug-ins.md#official-release-plug-ins>), restart Ludork and inspect **Plug-ins → Manage Plug-ins** for registration or load diagnostics.
- **No project or Blueprint is available:** Open a project containing at least one existing Blueprint (the host bridge is unavailable outside a project window). Opening the window does not select a target automatically; click **New** to choose one.
- **The target was renamed or deleted:** The existing conversation stays readable, but its target cannot be reassigned silently. Start a new conversation for an existing Blueprint.
- **Missing API key or HTTP 401:** Re-enter the API key in **Settings** and confirm it matches the chosen provider, endpoint and any OpenAI **Organisation** or **Project** values. The UI never displays the full stored key.
- **Connection or endpoint errors:** Supply an absolute `https://` base URL without embedded credentials, query parameters or fragments, and check DNS, proxy, firewall and TLS settings (plain `http://` works only for loopback endpoints).
- **Apply reports a revision conflict:** The Blueprint changed after the proposal captured its base revision. Discard the proposal, inspect the updated Blueprint and start a new request or conversation.
- **History shows interrupted:** The request, window or editor stopped before completion. Local history remains intact; send a new request rather than expecting the previous stream to resume.
