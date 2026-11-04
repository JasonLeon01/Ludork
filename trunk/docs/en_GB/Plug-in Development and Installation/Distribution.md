# Distribution

Distribute a single source plug-in directory that users can import into either the global or current-project scope.

## Directory contents

Include `plugin.json`, the required `.cs` files, plug-in resources and licence notices. State the supported editor versions and any setup required after import. The manifest's `minimumEditorVersion` must be at least `1.0.0`.

Exclude build outputs, local settings, credentials and `.data`. Do not include the surrounding `__ProjectPlugins` tree or its registry. Recipients choose their own installation scope; project plug-ins are not included in the official editor distribution.

Source compilation and the optional IDE project's role are described in [Manifest and Minimal Plug-in](<Manifest and Minimal Plug-in.md#source-directory>).

## Installation

Recipients import the complete source directory and restart Ludork. See [Installing and Managing Plug-ins](<Installing and Managing Plug-ins.md>) for scope, updates and retained data.

## Related pages

- [Registration and Hook Reference](<Registration and Hook Reference.md>)
