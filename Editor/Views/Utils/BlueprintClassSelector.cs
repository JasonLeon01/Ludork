using Avalonia.Controls;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

public enum BlueprintClassSelectorMode
{
    Parent,
    NodeParameter,
}

public static class BlueprintClassSelector
{
    private const string ActorRoot = "Engine.Actor";

    public static Task<string?> ShowAsync(
        Window owner,
        ProjectDataStore gameData,
        LuaMetadataService metadataService,
        BlueprintClassResolver classResolver,
        string current,
        string? blueprintKey,
        BlueprintClassSelectorMode mode)
    {
        BlueprintClassOptions options = buildOptions(
            gameData,
            metadataService,
            classResolver,
            blueprintKey,
            mode);
        string initial = BlueprintReference.IsReference(current) ? BlueprintReference.ToReference(current) : current;
        if (!BlueprintReference.IsReference(current))
        {
            ResolvedBlueprintClass resolved = classResolver.Resolve(current);
            if (resolved.RootType is not null)
                initial = metadataService.GetRuntimeClassReference(resolved.RootType);
        }
        return SearchSelectorDialog.ShowGroupedAsync(
            owner,
            LocaleService.Get("CLASS_SELECTOR"),
            [
                new SearchSelectorGroup(LocaleService.Get("PROJECT_CLASSES"), options.Classes),
                new SearchSelectorGroup(LocaleService.Get("PROJECT_BLUEPRINT"), options.Blueprints),
            ],
            initial);
    }

    private static BlueprintClassOptions buildOptions(
        ProjectDataStore gameData,
        LuaMetadataService metadataService,
        BlueprintClassResolver classResolver,
        string? blueprintKey,
        BlueprintClassSelectorMode mode)
    {
        HashSet<string> classes = new(StringComparer.Ordinal);
        foreach (string typeName in BlueprintCompatibilityCatalog.GetTypeNames())
            addClass(classes, typeName, metadataService, classResolver);
        foreach (LuaTypeMetadata metadata in metadataService.EnumerateTypes())
            addClass(classes, metadata.Type.QualifiedName, metadataService, classResolver);

        List<string> blueprints = [];
        foreach (string key in gameData.Blueprints.BlueprintsData.Keys.OrderBy(value => value, StringComparer.Ordinal))
        {
            if (mode == BlueprintClassSelectorMode.Parent
                && (string.Equals(key, blueprintKey, StringComparison.Ordinal)
                    || createsCycle(gameData, key, blueprintKey)))
            {
                continue;
            }
            string reference = BlueprintReference.ToReference(key);
            if (isBlueprintClass(reference, classResolver))
                blueprints.Add(reference);
        }
        return new BlueprintClassOptions(
            classes.OrderBy(value => value, StringComparer.Ordinal).ToArray(),
            blueprints);
    }

    private static void addClass(
        ISet<string> classes,
        string typeName,
        LuaMetadataService metadataService,
        BlueprintClassResolver classResolver)
    {
        ResolvedBlueprintClass resolved = classResolver.Resolve(typeName);
        string qualifiedName = resolved.RootType is not null
            ? metadataService.GetRuntimeClassReference(resolved.RootType)
            : typeName;
        LuaTypeReference type = LuaTypeReference.Parse(qualifiedName);
        if (!type.TypeName.StartsWith('_') && isBlueprintClass(qualifiedName, classResolver))
            classes.Add(qualifiedName);
    }

    private static bool isBlueprintClass(
        string reference,
        BlueprintClassResolver classResolver)
    {
        return classResolver.IsDerivedFrom(reference, ActorRoot);
    }

    private static bool createsCycle(
        ProjectDataStore gameData,
        string candidateKey,
        string? currentBlueprintKey)
    {
        if (string.IsNullOrWhiteSpace(currentBlueprintKey))
            return false;
        HashSet<string> visited = new(StringComparer.Ordinal);
        string? key = candidateKey;
        while (!string.IsNullOrWhiteSpace(key) && visited.Add(key))
        {
            if (string.Equals(key, currentBlueprintKey, StringComparison.Ordinal))
                return true;
            if (!gameData.Blueprints.BlueprintsData.TryGetValue(key, out BlueprintDefinitionSnapshot? blueprint)
                || string.IsNullOrWhiteSpace(blueprint.Parent)
                || !BlueprintReference.IsReference(blueprint.Parent))
            {
                return false;
            }
            key = BlueprintReference.NormalizeKey(blueprint.Parent);
        }
        return false;
    }
}

internal sealed record BlueprintClassOptions(
    IReadOnlyList<string> Classes,
    IReadOnlyList<string> Blueprints);
