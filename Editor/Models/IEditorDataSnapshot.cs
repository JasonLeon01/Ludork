using System.Text.Json.Nodes;

namespace Ludork.Models;

public interface IEditorDataSnapshot
{
    JsonObject ToJson();
}
