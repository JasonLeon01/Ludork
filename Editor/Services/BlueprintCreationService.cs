using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed class BlueprintCreationService
{
    private readonly ProjectDataStore gameData;
    private readonly LuaMetadataService metadataService;
    private readonly BlueprintClassResolver classResolver;

    public BlueprintCreationService(
        ProjectDataStore gameData,
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
        string key = BlueprintReference.NormalizeKey(relativePath);
        if (key.Length == 0 || key.StartsWith("../", StringComparison.Ordinal)
            || File.Exists(fullPath)
            || gameData.Blueprints.BlueprintsData.ContainsKey(key))
        {
            return new BlueprintCreationResult(
                false,
                key,
                File.Exists(fullPath) || gameData.Blueprints.BlueprintsData.ContainsKey(key)
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
        if (!BlueprintReference.IsReference(parent) && resolved.RootType is not null)
            parent = metadataService.GetRuntimeClassReference(resolved.RootType);
        JsonObject attrs = [];
        if (!BlueprintReference.IsReference(parent))
        {
            HashSet<string> invalidVars = new(resolved.InvalidVars, StringComparer.Ordinal);
            foreach (ResolvedBlueprintField field in resolved.Fields)
            {
                if (!field.HasBlueprintDefaultValue
                    || field.Name.StartsWith('_')
                    || invalidVars.Contains(field.Name))
                {
                    continue;
                }
                attrs[field.Name] = field.Value?.DeepClone();
            }
        }

        BlueprintNodeDefinitionCatalog catalog = new(metadataService, classResolver);
        JsonObject nodeGraph = [];
        JsonObject startNodes = [];
        foreach (BlueprintGraphNodeDefinition parentEvent in catalog.GetParentEventDefinitions(parent))
        {
            BlueprintGraphSaveResult initial = BlueprintEventGraphInitializer.Create(parentEvent);
            nodeGraph[parentEvent.MemberName] = initial.EventGraph;
            startNodes[parentEvent.MemberName] = initial.StartNode?.DeepClone();
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
        bool created = gameData.Blueprints.CreateBlueprint(key, blueprint);
        return new BlueprintCreationResult(
            created,
            key,
            created ? BlueprintCreationFailure.None : BlueprintCreationFailure.AlreadyExists);
    }

    private bool isValidParent(string parentClass)
    {
        return classResolver.IsDerivedFrom(parentClass, "Engine.Actor");
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
