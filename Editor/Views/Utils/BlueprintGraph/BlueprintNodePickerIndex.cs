using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.Views.Utils.BlueprintGraph;

public sealed class BlueprintNodePickerIndex
{
    private readonly LuaMetadataTypeSnapshot types;
    private readonly BlueprintNodePickerEntry[] entries;
    private readonly Dictionary<string, List<InputPin>> parameterInputs = new(StringComparer.Ordinal);
    private readonly Dictionary<string, BlueprintNodePickerCandidates> parameterQueries = new(StringComparer.Ordinal);
    private readonly BlueprintNodePickerCandidates executionCandidates;

    public BlueprintNodePickerIndex(
        IReadOnlyList<BlueprintGraphNodeDefinition> definitions,
        LuaMetadataTypeSnapshot types)
    {
        this.types = types;
        entries = definitions.Distinct()
            .Select(definition => new BlueprintNodePickerEntry(definition))
            .OrderBy(entry => entry.Title, StringComparer.Ordinal)
            .ThenBy(entry => entry.Definition.RuntimePath, StringComparer.Ordinal)
            .ToArray();
        int[] executionInputs = createTargetPins();
        int executionCount = 0;
        for (int entryIndex = 0; entryIndex < entries.Length; entryIndex++)
        {
            BlueprintNodePickerEntry entry = entries[entryIndex];
            foreach (BlueprintGraphPortDefinition port in entry.Definition.Ports)
            {
                if (port.Direction != BlueprintGraphPortDirection.Input)
                    continue;
                if (port.Kind == BlueprintGraphPortKind.Exec)
                {
                    addInput(executionInputs, new InputPin(entryIndex, port.PinIndex), ref executionCount);
                    continue;
                }
                if (!parameterInputs.TryGetValue(port.TypeName, out List<InputPin>? inputs))
                {
                    inputs = [];
                    parameterInputs.Add(port.TypeName, inputs);
                }
                inputs.Add(new InputPin(entryIndex, port.PinIndex));
            }
        }
        All = new BlueprintNodePickerCandidates(entries);
        executionCandidates = All.Filter(executionInputs, executionCount);
    }

    public BlueprintNodePickerCandidates All { get; }

    public BlueprintNodePickerCandidates QueryOutput(BlueprintGraphPortKind kind, string sourceType)
    {
        if (kind == BlueprintGraphPortKind.Exec)
            return executionCandidates;
        if (parameterQueries.TryGetValue(sourceType, out BlueprintNodePickerCandidates? cached))
            return cached;

        int[] targetPins = createTargetPins();
        int count = 0;
        bool[]? exactMatches = null;
        if (parameterInputs.TryGetValue(sourceType, out List<InputPin>? exactInputs))
        {
            exactMatches = new bool[entries.Length];
            foreach (InputPin input in exactInputs)
            {
                addInput(targetPins, input, ref count);
                exactMatches[input.EntryIndex] = true;
            }
        }
        foreach (KeyValuePair<string, List<InputPin>> bucket in parameterInputs)
        {
            if (string.Equals(bucket.Key, sourceType, StringComparison.Ordinal)
                || !types.IsTypeAssignable(sourceType, bucket.Key))
            {
                continue;
            }
            foreach (InputPin input in bucket.Value)
            {
                if (exactMatches is null || !exactMatches[input.EntryIndex])
                    addInput(targetPins, input, ref count);
            }
        }
        BlueprintNodePickerCandidates result = All.Filter(targetPins, count);
        parameterQueries.Add(sourceType, result);
        return result;
    }

    private int[] createTargetPins()
    {
        int[] result = new int[entries.Length];
        Array.Fill(result, -1);
        return result;
    }

    private static void addInput(int[] targetPins, InputPin input, ref int count)
    {
        int existing = targetPins[input.EntryIndex];
        if (existing < 0)
            count++;
        if (existing < 0 || input.PinIndex < existing)
            targetPins[input.EntryIndex] = input.PinIndex;
    }

    private readonly record struct InputPin(int EntryIndex, int PinIndex);
}
