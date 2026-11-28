using Ludork.Plugin.Abstractions;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.Json.Nodes;
using System.Threading;

namespace Ludork.Plugins.OfficialGeneralDataBlueprintExport;

internal sealed record GeneralDataBlueprintExportBatch(
    IReadOnlyList<PluginBlueprintCreation> Files, GeneralDataBlueprintExportResult Result)
{
    private sealed record Candidate(GeneralDataBlueprintDeclaration Declaration, string Member, string Path);

    internal static GeneralDataBlueprintExportBatch Prepare(IPluginBlueprintSnapshot snapshot,
        IReadOnlyDictionary<string, string> directories, GeneralDataBlueprintScanData scan, CancellationToken token)
    {
        List<GeneralDataBlueprintIssue> issues = [.. scan.Issues];
        List<Candidate> candidates = [];
        int existing = 0;
        int conflicts = 0;
        foreach ((string type, string directory) in directories.OrderBy(pair => pair.Key, StringComparer.Ordinal))
        {
            if (!scan.Types.TryGetValue(type, out GeneralDataBlueprintDeclaration? declaration))
            {
                issues.Add(new(GeneralDataBlueprintIssueKind.TypeMissing, type));
                continue;
            }
            string? destination = normalizeDirectory(directory);
            foreach (string member in snapshot.GeneralData[type].Members)
            {
                token.ThrowIfCancellationRequested();
                if (scan.Existing.Contains((type, member)))
                {
                    existing++;
                    continue;
                }
                string name = "BP_" + member;
                if (destination is null || !isBlueprintSegment(name) || Encoding.UTF8.GetByteCount(name + ".json") > 255)
                {
                    issues.Add(destination is null
                        ? new(GeneralDataBlueprintIssueKind.InvalidDirectory, type, member, directory)
                        : new(GeneralDataBlueprintIssueKind.InvalidMemberName, type, member));
                    conflicts++;
                    continue;
                }
                candidates.Add(new Candidate(declaration, member, $"{destination}/{name}.json"));
            }
        }

        HashSet<string> collisions = candidates.GroupBy(candidate => candidate.Path, snapshot.PathComparer)
            .Where(group => group.Count() > 1).Select(group => group.Key).ToHashSet(snapshot.PathComparer);
        GeneralDataBlueprintIssue?[] errors = FileBatch.Run<Candidate, GeneralDataBlueprintIssue?>(candidates, candidate =>
        {
            string type = candidate.Declaration.Type;
            if (collisions.Contains(candidate.Path))
                return new(GeneralDataBlueprintIssueKind.TargetCollision, type, candidate.Member, candidate.Path);
            return snapshot.CheckDestination(candidate.Path) switch
            {
                PluginBlueprintDestination.Available => null,
                PluginBlueprintDestination.Occupied => new(GeneralDataBlueprintIssueKind.FileExists, type, candidate.Member, candidate.Path),
                _ => new(GeneralDataBlueprintIssueKind.InvalidDirectory, type, candidate.Member, candidate.Path),
            };
        }, token);
        List<PluginBlueprintCreation> files = [];
        for (int index = 0; index < candidates.Count; index++)
        {
            Candidate candidate = candidates[index];
            if (errors[index] is GeneralDataBlueprintIssue issue)
            {
                conflicts++;
                issues.Add(issue);
                continue;
            }
            files.Add(new PluginBlueprintCreation(candidate.Path, candidate.Declaration.Parent, new JsonObject
            {
                [candidate.Declaration.MemberField] = candidate.Member,
                ["texturePath"] = "",
            }));
        }
        return new(files, new GeneralDataBlueprintExportResult(files.Count, existing, conflicts, issues));
    }

    private static string? normalizeDirectory(string directory)
    {
        string normalized = directory.Replace('\\', '/').TrimEnd('/');
        string[] parts = normalized.Split('/');
        return parts.Length >= 2 && parts[0] == "Data" && parts[1] == "Blueprints"
            && parts.Skip(2).All(isBlueprintSegment) ? normalized : null;
    }

    private static bool isBlueprintSegment(string value) => value.Length != 0 && !value.EndsWith(' ')
        && value.IndexOfAny(['.', '/', '\\', ':', '*', '?', '"', '<', '>', '|']) < 0 && !value.Any(char.IsControl);
}
