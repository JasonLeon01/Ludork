# Distribution

Package a third-party plug-in as a single self-contained source directory that recipients can import into either the global or current-project scope.

## Directory contents

- **Include:** `plugin.json` (with `minimumEditorVersion` set to at least `1.0.0`), all required `.cs` source files, plug-in resources (such as locale JSON files) and licence notices. Document the supported editor versions and any post-import setup required.
- **Exclude:** Build outputs (`bin/`, `obj/`), local settings, credentials, `.data`, and any enclosing `__ProjectPlugins` tree or `plugins.json` registry. Recipients select their own scope during import; official editor distributions never bundle project plug-ins.

For source compilation rules and the role of an optional `.csproj` file, see [Source directory](<Manifest and Minimal Plug-in.md#source-directory>).

## Installation

Recipients import the complete source directory and restart Ludork. See [Installing and Managing Plug-ins](<Installing and Managing Plug-ins.md>) for scope selection, updates and retained data behaviour.
