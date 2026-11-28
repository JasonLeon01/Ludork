using Ludork.Models;
using Ludork.Plugin.Abstractions;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json.Nodes;
using System.Threading;

namespace Ludork.Services.Plugins;

internal static class PluginBlueprintCreationBatch
{
    internal sealed record Output(string Path, string Key, JsonObject Data, byte[] Content);
    private sealed record Definition(string Path, string Key, string Parent, JsonObject Data);
    private sealed record Template(string Parent, JsonObject Graph);

    internal static Output[] Prepare(PluginBlueprintSnapshot snapshot, IReadOnlyList<PluginBlueprintCreation> creations,
        CancellationToken token)
    {
        Definition[] definitions = EditorValidationBatch.RunFiles(creations, creation =>
        {
            if (snapshot.ResolveDestination(creation.Path, out string path) != PluginBlueprintDestination.Available)
                throw new IOException($"The Blueprint destination is unavailable: {creation.Path}");
            string parent = creation.Parent.Trim();
            if (!BlueprintReference.IsReference(parent) && !parent.Split('.').All(LuaIdentifier.IsValid))
                throw new InvalidDataException($"Invalid Blueprint parent: {parent}");
            return new Definition(path, snapshot.GetKey(path), parent, new JsonObject
            {
                ["parent"] = parent,
                ["attrs"] = creation.Attributes,
            });
        }, token);

        HashSet<string> destinations = new(snapshot.PathComparer);
        Dictionary<string, JsonObject> parents = new(snapshot.Blueprints, StringComparer.Ordinal);
        foreach (Definition definition in definitions)
        {
            if (!destinations.Add(definition.Path))
                throw new IOException($"More than one Blueprint targets '{definition.Path}'.");
            parents.Add(definition.Key, definition.Data);
        }
        string[] references = definitions.Select(definition => definition.Parent).Distinct(StringComparer.Ordinal).ToArray();
        Template[] templates = EditorValidationBatch.RunFiles(references, () => snapshot.CreateWorker(parents),
            (worker, parent) =>
            {
                ResolvedBlueprintClass resolved = worker.Resolver.Resolve(parent);
                if (resolved.RootType is null || resolved.TerminalReference is not string terminal
                    || worker.Metadata.GetRuntimeClassType(terminal) is null || !worker.Resolver.IsDerivedFrom(resolved, "Engine.Actor"))
                    throw new InvalidDataException($"Blueprint parent '{parent}' must derive from Engine.Actor.");
                string reference = BlueprintReference.IsReference(parent) ? BlueprintReference.ToReference(parent)
                    : worker.Metadata.GetRuntimeClassReference(resolved.RootType);
                return new Template(reference, BlueprintCreationService.CreateInitialGraph(worker.Metadata, worker.Resolver, reference));
            }, worker => worker.Dispose(), token);
        Dictionary<string, Template> byParent = references.Zip(templates).ToDictionary(pair => pair.First,
            pair => pair.Second, StringComparer.Ordinal);

        return EditorValidationBatch.RunFiles(definitions, () => snapshot.CreateWorker(parents),
            (worker, definition) =>
            {
                Template template = byParent[definition.Parent];
                JsonObject data = (JsonObject)definition.Data.DeepClone();
                data["parent"] = template.Parent;
                data["graph"] = template.Graph.DeepClone();
                ResolvedBlueprintClass resolved = worker.Resolver.ResolveBlueprint(data, definition.Key);
                if (resolved.ScriptMixinError is string error)
                    throw new InvalidDataException($"Invalid Blueprint '{definition.Key}': {error}");
                data["type"] = "blueprint";
                byte[] content = Encoding.UTF8.GetBytes(data.ToJsonString(ProjectDataStore.WriteOptions) + "\n");
                data.Remove("type");
                return new Output(definition.Path, definition.Key, data, content);
            }, worker => worker.Dispose(), token);
    }
}
