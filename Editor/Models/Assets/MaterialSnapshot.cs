using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class MaterialSnapshot(JsonObject data) : EditorDataSnapshot(data)
{
}
