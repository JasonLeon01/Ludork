using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;

namespace Ludork.Models;

public sealed class BlueprintNodeDefinitionSet
{
    public BlueprintNodeDefinitionSet(
        IReadOnlyList<BlueprintGraphNodeDefinition> definitions,
        IReadOnlyDictionary<string, BlueprintGraphNodeDefinition> runtimeLookup,
        IReadOnlyDictionary<string, IReadOnlyList<BlueprintGraphEventParameterDefinition>> eventParameters)
    {
        Definitions = definitions;
        RuntimeLookup = runtimeLookup;
        Dictionary<string, IReadOnlyList<BlueprintGraphEventParameterDefinition>> parameters =
            new(StringComparer.Ordinal);
        foreach (KeyValuePair<string, IReadOnlyList<BlueprintGraphEventParameterDefinition>> pair in eventParameters)
            parameters[pair.Key] = pair.Value;
        EventParameters = new ReadOnlyDictionary<string, IReadOnlyList<BlueprintGraphEventParameterDefinition>>(parameters);
    }

    public IReadOnlyList<BlueprintGraphNodeDefinition> Definitions { get; }
    public IReadOnlyDictionary<string, BlueprintGraphNodeDefinition> RuntimeLookup { get; }
    public IReadOnlyDictionary<string, IReadOnlyList<BlueprintGraphEventParameterDefinition>> EventParameters { get; }
}
