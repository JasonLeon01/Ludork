using Ludork.Plugin.Abstractions;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Threading;

namespace Ludork.Plugins.OfficialGeneralDataBlueprintExport;

internal sealed record GeneralDataBlueprintDeclaration(string Type, string Parent, string MemberField);

internal sealed record GeneralDataBlueprintMark(string Location, string? Type,
    GeneralDataBlueprintDeclaration? Declaration, GeneralDataBlueprintIssue? Error);

internal static class GeneralDataBlueprintDeclarations
{
    private const string Marker = "@general-data-blueprint";

    internal static IReadOnlyList<GeneralDataBlueprintMark> Read(string projectPath, string path, CancellationToken token)
    {
        string source = File.ReadAllText(path);
        if (!source.Contains(Marker, StringComparison.Ordinal))
            return [];
        string relativePath = Path.GetRelativePath(projectPath, path).Replace('\\', '/');
        List<GeneralDataBlueprintMark> marks = [];
        try
        {
            LuaSourceTokens.Read(source, token, (start, end) =>
            {
                ReadOnlySpan<char> comment = source.AsSpan(start + 2, end - start - 2).TrimStart();
                if (!comment.StartsWith(Marker, StringComparison.Ordinal)
                    || comment.Length > Marker.Length && !char.IsWhiteSpace(comment[Marker.Length]))
                    return;
                int lineStart = source.AsSpan(0, start).LastIndexOfAny('\r', '\n') + 1;
                if (!source.AsSpan(lineStart, start - lineStart).IsWhiteSpace())
                    return;
                int line = source.AsSpan(0, start).Count('\n') + 1;
                string location = $"{relativePath}:{line}";
                marks.Add(parse(location, comment[Marker.Length..].Trim().ToString()));
            });
        }
        catch (InvalidDataException exception)
        {
            throw new InvalidDataException($"{relativePath}: {exception.Message}", exception);
        }
        return marks;
    }

    private static GeneralDataBlueprintMark parse(string location, string json)
    {
        try
        {
            using JsonDocument document = JsonDocument.Parse(json);
            JsonElement root = document.RootElement;
            string? type = readName(root, "type");
            string? parent = readName(root, "parent");
            string? field = readName(root, "memberField");
            if (type is null || parent is null || field is null
                || !parent.Split('.').All(LuaIdentifier.IsValid) || !LuaIdentifier.IsValid(field))
                return new(location, type, null, new(GeneralDataBlueprintIssueKind.InvalidAnnotation, location));
            return new(location, type, new GeneralDataBlueprintDeclaration(type, parent, field), null);
        }
        catch (JsonException exception)
        {
            return new(location, null, null,
                new(GeneralDataBlueprintIssueKind.InvalidAnnotationJson, location, exception.Message));
        }
    }

    private static string? readName(JsonElement root, string key)
    {
        return root.ValueKind == JsonValueKind.Object && root.TryGetProperty(key, out JsonElement field)
            && field.ValueKind == JsonValueKind.String && field.GetString() is string value
            && !string.IsNullOrWhiteSpace(value) ? value : null;
    }
}
