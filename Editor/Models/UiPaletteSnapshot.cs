using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class UiPaletteSnapshot(JsonObject source) : EditorJsonSnapshot(source)
{
    public bool Exposed => ReadBoolean("exposed");
    public string? DisplayName => ReadString("displayName");
    public string? Category => ReadString("category");
}
