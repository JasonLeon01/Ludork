using Ludork.Plugin.Abstractions;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading;

namespace Ludork.Plugins.OfficialGeneralDataBlueprintExport;

internal sealed record GeneralDataBlueprintScanData(
    IReadOnlyDictionary<string, GeneralDataBlueprintDeclaration> Types,
    IReadOnlySet<(string Type, string Member)> Existing, IReadOnlyList<GeneralDataBlueprintIssue> Issues);

internal static class GeneralDataBlueprintScanner
{
    internal static GeneralDataBlueprintScanData Scan(string projectPath, IPluginBlueprintSnapshot snapshot, CancellationToken token)
    {
        string scripts = Path.Combine(projectPath, "Scripts");
        string[] scriptsToRead = findFiles(scripts, "*.lua", token, Path.Combine(scripts, "stub"))
            .Where(path => !path.EndsWith("_meta.lua", StringComparison.OrdinalIgnoreCase)
                && !path.EndsWith(".d.lua", StringComparison.OrdinalIgnoreCase)).ToArray();
        GeneralDataBlueprintMark[] marks = FileBatch.Run(scriptsToRead,
            path => GeneralDataBlueprintDeclarations.Read(projectPath, path, token), token).SelectMany(entries => entries).ToArray();
        GeneralDataBlueprintMark[] validated = FileBatch.Run(marks, mark => validateDeclaration(snapshot, mark), token);
        List<GeneralDataBlueprintIssue> issues = validated.Select(mark => mark.Error).OfType<GeneralDataBlueprintIssue>().ToList();
        Dictionary<string, GeneralDataBlueprintDeclaration> types = new(StringComparer.Ordinal);
        foreach (IGrouping<string, GeneralDataBlueprintMark> group in validated.Where(mark => mark.Type is not null)
                     .GroupBy(mark => mark.Type!, StringComparer.Ordinal))
        {
            GeneralDataBlueprintMark first = group.First();
            if (group.Skip(1).Any())
                issues.Add(new(GeneralDataBlueprintIssueKind.DuplicateType,
                    group.Key, string.Join(", ", group.Select(mark => mark.Location))));
            else if (first.Declaration is not null && first.Error is null)
                types.Add(group.Key, first.Declaration);
        }
        HashSet<(string Type, string Member)> existing = [];
        if (types.Count == 0)
            return new(types, existing, issues);

        string[] dataToRead = findFiles(Path.Combine(projectPath, "Data"), "*.json", token)
            .Where(path => !snapshot.ManagedBlueprintPaths.Contains(path)).ToArray();
        PluginBlueprintSource[] additional = FileBatch.Run(dataToRead, readBlueprint, token)
            .OfType<PluginBlueprintSource>().ToArray();
        string[] fields = types.Values.Select(type => type.MemberField).Distinct(StringComparer.Ordinal).ToArray();
        ILookup<string, GeneralDataBlueprintDeclaration> byParent = types.Values.ToLookup(type => type.Parent, StringComparer.Ordinal);
        foreach (PluginResolvedBlueprint blueprint in snapshot.ResolveBlueprints(additional, fields, token))
        {
            token.ThrowIfCancellationRequested();
            foreach (GeneralDataBlueprintDeclaration declaration in blueprint.Ancestors.SelectMany(parent => byParent[parent]))
            {
                if (blueprint.Values.TryGetValue(declaration.MemberField, out JsonNode? field)
                    && field is JsonValue value && value.TryGetValue(out string? member) && !string.IsNullOrEmpty(member))
                    existing.Add((declaration.Type, member));
            }
        }
        return new(types, existing, issues);
    }

    private static IReadOnlyList<string> findFiles(string root, string pattern, CancellationToken token, string? excludedDirectory = null)
    {
        List<string> files = [];
        List<string> directories = [root];
        while (directories.Count != 0)
        {
            (string[] Files, string[] Directories)[] contents = FileBatch.Run(directories, path =>
                (Directory.GetFiles(path, pattern), new DirectoryInfo(path).EnumerateDirectories()
                    .Where(child => child.FullName != excludedDirectory && (child.Attributes & FileAttributes.ReparsePoint) == 0)
                    .Select(child => child.FullName).ToArray()), token);
            directories.Clear();
            foreach ((string[] found, string[] children) in contents)
            {
                files.AddRange(found);
                directories.AddRange(children);
            }
        }
        files.Sort(StringComparer.Ordinal);
        return files;
    }

    private static GeneralDataBlueprintMark validateDeclaration(IPluginBlueprintSnapshot snapshot, GeneralDataBlueprintMark mark)
    {
        if (mark.Declaration is not GeneralDataBlueprintDeclaration declaration)
            return mark;
        if (!snapshot.GeneralData.TryGetValue(declaration.Type, out PluginGeneralDataType? type))
            return mark with { Error = new(GeneralDataBlueprintIssueKind.UnknownType, mark.Location, declaration.Type) };
        try
        {
            PluginBlueprintClass? parent = snapshot.DescribeClass(declaration.Parent);
            if (parent is null || !parent.Ancestors.Contains("Engine.Actor"))
                return mark with { Error = new(GeneralDataBlueprintIssueKind.InvalidParent, mark.Location, declaration.Parent) };
            if (declaration.MemberField == "texturePath"
                || !parent.EditableFields.TryGetValue(declaration.MemberField, out JsonNode? schema)
                || !isMemberField(schema, type.EnumModule))
                return mark with { Error = new(GeneralDataBlueprintIssueKind.InvalidMemberField,
                    mark.Location, declaration.Parent, declaration.MemberField, type.EnumModule) };
            return mark with { Declaration = declaration with { Parent = parent.Reference } };
        }
        catch (InvalidDataException exception)
        {
            return mark with { Error = new(GeneralDataBlueprintIssueKind.InvalidMetadata, mark.Location, exception.Message) };
        }
    }

    private static bool isMemberField(JsonNode schema, string enumModule) =>
        schema is JsonValue scalar && scalar.TryGetValue(out string? name) && name == "string"
        || schema is JsonObject map && map["enum"] is JsonValue module && module.TryGetValue(out string? enumName)
        && enumName == enumModule;

    private static PluginBlueprintSource? readBlueprint(string path)
    {
        try
        {
            using FileStream stream = File.OpenRead(path);
            using JsonDocument document = JsonDocument.Parse(stream);
            JsonElement root = document.RootElement;
            return root.ValueKind == JsonValueKind.Object && root.TryGetProperty("type", out JsonElement type)
                && type.ValueKind == JsonValueKind.String && type.ValueEquals("blueprint")
                ? new PluginBlueprintSource(path, JsonObject.Create(root.Clone())!) : null;
        }
        catch (JsonException exception)
        {
            throw new InvalidDataException($"{path}: {exception.Message}", exception);
        }
    }
}
