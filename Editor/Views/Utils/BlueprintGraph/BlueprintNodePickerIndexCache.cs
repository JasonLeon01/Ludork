using Ludork.Models;
using Ludork.Services;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace Ludork.Views.Utils.BlueprintGraph;

public sealed class BlueprintNodePickerIndexCache
{
    private readonly ConditionalWeakTable<IReadOnlyList<BlueprintGraphNodeDefinition>, BlueprintNodePickerIndex> indexes = new();
    private LuaMetadataTypeSnapshot? types;
    private string? language;

    public BlueprintNodePickerIndex? Get(IReadOnlyList<BlueprintGraphNodeDefinition> definitions, BlueprintNodeSearchService? search)
    {
        if (search?.IsCurrent != true || search.Types is not LuaMetadataTypeSnapshot snapshot)
            return null;
        if (!ReferenceEquals(types, snapshot) || language != LocaleService.CurrentLanguage)
        {
            Clear();
            types = snapshot;
            language = LocaleService.CurrentLanguage;
        }
        if (!indexes.TryGetValue(definitions, out BlueprintNodePickerIndex? index))
        {
            index = new BlueprintNodePickerIndex(definitions, snapshot);
            indexes.Add(definitions, index);
        }
        return index;
    }

    public void Clear() => indexes.Clear();
}
