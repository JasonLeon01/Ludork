using System.Text.Json.Nodes;

namespace Ludork.Services;

internal static class ReferenceIdentity
{
    public static string NodeId(string type, string key) => type + ":" + key.Replace('\\', '/');

    public static string BlueprintNodeId(string key)
        => NodeId("blueprint", BlueprintReference.ToReference(key));

    public static string GeneralMemberNodeId(string typeKey, string memberKey)
        => NodeId("generalMember", $"{typeKey}/{memberKey}");

    public static string? NormalizeParameter(JsonNode? value)
    {
        string? text = JsonScalar.String(value)?.Trim();
        if (string.IsNullOrWhiteSpace(text))
            return null;
        if (text.Length >= 2 && text[0] == text[^1] && text[0] is '\'' or '"')
            text = text[1..^1].Trim();
        return text.Length == 0 ? null : text;
    }
}
