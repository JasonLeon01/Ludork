# Official Blueprint AI

Configure the official Blueprint AI plug-in, discuss changes to one Blueprint and review its proposal before applying it.

The plug-in ID is `Ludork.OfficialBlueprintAI`; its repository source is `Plugins/OfficialBlueprintAI`. Open it through **Game → Blueprint AI**.

## Workflow

1. Install the plug-in as described in [Installing and Managing Plug-ins](<../Installing and Managing Plug-ins.md#official-release-plug-ins>).
2. Open a project and select **Game → Blueprint AI**.
3. Configure the provider, model, endpoint and secret from **Settings**.
4. Create a conversation for one existing Blueprint and review the staged proposal and generated diff.
5. Select **Apply** only after validation succeeds, then use the normal project Save command when the change is ready for disk.

## Local data

Settings and conversation history are stored in `.data/Ludork.OfficialBlueprintAI` beneath the global plug-in root. Uninstalling the plug-in retains this data. See [Data and uninstall](<../Installing and Managing Plug-ins.md#data-and-uninstall>) for storage locations and removal.

## Scope and permissions

Each conversation is bound to one project and one existing Blueprint. The assistant can inspect other Blueprints, Lua metadata and allowlisted Lua/C/C++ source files to understand the available APIs, and search and read official engine documentation. It cannot create, rename or delete Blueprints, modify more than one Blueprint, write source code, run a shell or start processes.

The model can only stage a Blueprint proposal. The editor validates the candidate and shows a generated diff. Nothing changes until the user selects **Apply**. Apply re-reads the editor working copy and compares its revision with the proposal base. A successful Apply creates one normal Undo step, leaves the project dirty and refreshes every open editor for that Blueprint. The Blueprint is not written to disk until the normal project Save operation.

Plug-ins are trusted in-process code and are not an operating-system sandbox. The official assistant's model-facing capability surface and the editor's session and Apply boundaries enforce the restrictions above.

## Provider settings and credentials

Open **Settings** in the Blueprint AI window to choose a provider, model and endpoint. OpenAI uses the Responses API. DeepSeek, Google OpenAI-compatible and Custom profiles use an OpenAI-compatible Chat endpoint. The model and endpoint fields remain editable, so a plug-in update is not required for every model release.

An endpoint must be an absolute HTTPS URL. Plain HTTP is accepted only for a loopback address used during local development. Credentials, query strings and fragments are not accepted in endpoint URLs.

API keys use the [host secret store](<../Avalonia UI Secrets and Localisation.md#store-secrets>) and are not written to `settings.json`. The settings window displays a stored key as its first three characters, a fixed mask and its final three characters. Short keys are fully hidden. The credential changes only when a new value is entered or removal is explicitly selected. Provider availability is external to Ludork.

## Local conversations and provider state

The local index and the append-only JSONL records under the plug-in data directory are the authoritative conversation history. They contain messages, model snapshots, compact tool activity and proposal decisions. They never contain the API key or complete source-file reads.

OpenAI Responses requests use `store:false`, and the plug-in does not rely on provider-side conversation storage. For a long conversation, only a rolling summary and recent messages are placed in the next provider request. This request-context compaction does not delete or replace the complete local history.

An unfinished request is recorded as interrupted. Its completed local records remain available after a restart, but the in-flight request is not resumed automatically.

## Project source inspection

Project search and file reading only accept project-relative paths under the configured runtime and API source roots, with supported text extensions. Build outputs, logs, VCS data, INI files, plug-in data, credentials and paths outside the project are rejected. Symbolic links, broken links and reparse points are rejected throughout the path, including the project root. File-system failures during reading or searching are returned as tool errors. File contents are treated as data, not as instructions that can expand assistant permissions.

## Official documentation

The assistant can search and read official documentation for the editor's current major.minor version and language. For example, editor `1.0.7` uses documentation `1.0`. Results include links to the source pages.

If the matching documentation is unavailable, the assistant reports the problem without using a different version or language. The current project's API catalogue and metadata remain authoritative for concrete nodes, types, serialised names and pins. Public documentation methods are listed in [Registration and Hook Reference](<../Registration and Hook Reference.md#blueprint-assistants>).

## Troubleshooting

- **Blueprint AI is missing from Game:** follow [Installing and Managing Plug-ins](<../Installing and Managing Plug-ins.md#official-release-plug-ins>), restart the editor and inspect **Plugins → Manage Plugins** for registration or load diagnostics.
- **No project or Blueprint is available:** open a project first. The host bridge is unavailable outside a project window. A project also needs at least one existing Blueprint. Opening the plug-in does not choose a target; the selector appears only after selecting **New**.
- **The target was renamed or deleted:** the old conversation remains readable, but its target cannot be switched silently. Create a new conversation for an existing Blueprint.
- **The API key is missing or the server returns 401:** replace the key in Settings and check that it belongs to the selected provider or profile. Also verify the endpoint and any OpenAI organisation or project values. The UI shows only a masked value, never the complete stored key.
- **Connection or endpoint errors:** use an absolute HTTPS base endpoint without embedded credentials, a query or a fragment. Check DNS, proxy, firewall and TLS settings. Plain HTTP is intentionally limited to loopback development endpoints.
- **Apply reports a revision conflict:** the Blueprint changed after the proposal recorded its base revision. The editor refuses to overwrite it. Discard the proposal, review the current Blueprint and start a new conversation or request.
- **History shows interrupted:** the editor, window or request was stopped before completion. The local history remains authoritative. Send a new request when ready instead of expecting the previous network stream to resume.

## Related pages

- [Distribution](<../Distribution.md>)
