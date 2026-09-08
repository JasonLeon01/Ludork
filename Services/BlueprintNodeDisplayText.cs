using Ludork.Models;
using System;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public static class BlueprintNodeDisplayText
{
    public static string GetTitle(BlueprintGraphNodeDefinition definition)
    {
        return getExplicitDisplayName(definition)
            ?? formatMemberName(definition.MemberName, definition.IsParent);
    }

    public static string GetGraphTitle(
        string nodeFunction,
        BlueprintGraphNodeDefinition? definition)
    {
        if (getExplicitDisplayName(definition) is string displayName)
            return displayName;
        int separator = nodeFunction.LastIndexOf('.');
        string memberName = separator >= 0 && separator < nodeFunction.Length - 1
            ? nodeFunction[(separator + 1)..]
            : nodeFunction;
        return formatMemberName(
            memberName,
            !nodeFunction.Contains('.', StringComparison.Ordinal)
                && !string.IsNullOrWhiteSpace(memberName));
    }

    private static string formatMemberName(string memberName, bool isParent)
    {
        string displayName = EditorDisplayName.Format(memberName);
        return isParent ? $"(parent){displayName}" : displayName;
    }

    private static string? getExplicitDisplayName(BlueprintGraphNodeDefinition? definition)
    {
        return definition?.Meta["DisplayName"] is JsonValue value
            && value.TryGetValue(out string? displayName)
                ? displayName
                : null;
    }
}
