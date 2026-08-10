using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed class BlueprintCreationService
{
    private const string BlueprintPrefix = "Data.Blueprints.";
    private readonly GameDataService gameData;
    private readonly LuaMetadataService metadataService;
    private readonly BlueprintClassResolver classResolver;

    public BlueprintCreationService(
        GameDataService gameData,
        LuaMetadataService metadataService,
        BlueprintClassResolver classResolver)
    {
        this.gameData = gameData;
        this.metadataService = metadataService;
        this.classResolver = classResolver;
    }

    public BlueprintCreationResult Create(string destinationPath, string parentClass)
    {
        string blueprintsRoot = Path.GetFullPath(
            Path.Combine(gameData.ProjectPath, "Data", "Blueprints"));
        string fullPath = Path.GetFullPath(Path.IsPathRooted(destinationPath)
            ? destinationPath
            : Path.Combine(blueprintsRoot, destinationPath));
        if (!fullPath.StartsWith(
                blueprintsRoot + Path.DirectorySeparatorChar,
                StringComparison.OrdinalIgnoreCase)
            || !string.Equals(
                Path.GetExtension(fullPath),
                DataConfig.DataFileExtension,
                StringComparison.OrdinalIgnoreCase))
        {
            return new BlueprintCreationResult(false, null, BlueprintCreationFailure.InvalidPath);
        }

        string relativePath = Path.GetRelativePath(blueprintsRoot, fullPath);
        string key = Path.ChangeExtension(relativePath, null)!.Replace('\\', '/');
        if (key.Length == 0 || key.StartsWith("../", StringComparison.Ordinal)
            || File.Exists(fullPath)
            || gameData.BlueprintsData.ContainsKey(key))
        {
            return new BlueprintCreationResult(
                false,
                key,
                File.Exists(fullPath) || gameData.BlueprintsData.ContainsKey(key)
                    ? BlueprintCreationFailure.AlreadyExists
                    : BlueprintCreationFailure.InvalidPath);
        }

        string parent = parentClass.Trim();
        if (parent.Length == 0 || !isValidParent(parent))
        {
            return new BlueprintCreationResult(
                false,
                key,
                BlueprintCreationFailure.InvalidParent);
        }

        ResolvedBlueprintClass resolved = classResolver.Resolve(parent);
        JsonObject attrs = [];
        if (!parent.StartsWith(BlueprintPrefix, StringComparison.Ordinal))
        {
            HashSet<string> invalidVars = new(resolved.InvalidVars, StringComparer.Ordinal);
            foreach (ResolvedBlueprintField field in resolved.Fields)
            {
                if (field.Name.StartsWith('_')
                    || invalidVars.Contains(field.Name))
                {
                    continue;
                }
                attrs[field.Name] = field.Value?.DeepClone();
            }
        }

        SortedSet<string> events = new(StringComparer.Ordinal);
        collectBlueprintEvents(parent, events, new HashSet<string>(StringComparer.Ordinal));
        if (resolved.RootType is not null)
        {
            foreach (LuaNodeMemberMetadata member in metadataService.GetNodeMembers(
                resolved.RootType,
                LuaNodeMemberKind.Event))
            {
                events.Add(member.Name);
            }
        }

        JsonObject nodeGraph = [];
        JsonObject startNodes = [];
        foreach (string eventName in events)
        {
            nodeGraph[eventName] = new JsonObject
            {
                ["nodes"] = new JsonArray(),
                ["links"] = new JsonArray(),
            };
            startNodes[eventName] = null;
        }
        JsonObject blueprint = new()
        {
            ["parent"] = parent,
            ["attrs"] = attrs,
            ["graph"] = new JsonObject
            {
                ["nodeGraph"] = nodeGraph,
                ["startNodes"] = startNodes,
            },
        };
        bool created = gameData.CreateBlueprint(key, blueprint);
        return new BlueprintCreationResult(
            created,
            key,
            created ? BlueprintCreationFailure.None : BlueprintCreationFailure.AlreadyExists);
    }

    private bool isValidParent(string parentClass)
    {
        return classResolver.IsDerivedFrom(parentClass, "Engine.Actor")
            || classResolver.IsDerivedFrom(parentClass, "Engine.InfoBase");
    }

    private void collectBlueprintEvents(
        string reference,
        ISet<string> events,
        ISet<string> visited)
    {
        if (!reference.StartsWith(BlueprintPrefix, StringComparison.Ordinal))
            return;
        string key = reference[BlueprintPrefix.Length..].Replace('.', '/');
        if (!visited.Add(key)
            || !gameData.BlueprintsData.TryGetValue(key, out JsonObject? blueprint))
        {
            return;
        }
        string? parent = blueprint["parent"]?.GetValue<string>();
        if (!string.IsNullOrWhiteSpace(parent))
            collectBlueprintEvents(parent, events, visited);
        if (blueprint["graph"]?["nodeGraph"] is not JsonObject nodeGraph)
            return;
        foreach (string eventName in nodeGraph.Select(entry => entry.Key))
            events.Add(eventName);
    }
}

public enum BlueprintCreationFailure
{
    None,
    InvalidPath,
    AlreadyExists,
    InvalidParent,
}

public sealed record BlueprintCreationResult(
    bool Success,
    string? Key,
    BlueprintCreationFailure Failure);
