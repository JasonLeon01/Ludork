using Ludork.Models;
using Ludork.Services;
using System;
using System.Linq;

namespace Ludork.Views.Utils.BlueprintGraph;

internal sealed class BlueprintNodePickerEntry
{
    public BlueprintNodePickerEntry(BlueprintGraphNodeDefinition definition)
    {
        Definition = definition;
        Title = BlueprintNodeDisplayText.GetTitle(definition);
        string[] parts = definition.MetadataPath.Split('.', StringSplitOptions.RemoveEmptyEntries);
        GroupPath = definition.IsParent ? [LocaleService.Get("PARENT")] : parts.Length <= 1 ? [] : parts[..^1];
        string hierarchy = string.Join('.', GroupPath.Select(EditorDisplayName.Format));
        SearchTitle = string.IsNullOrWhiteSpace(hierarchy) ? Title : $"{Title} ({hierarchy})";
    }

    public BlueprintGraphNodeDefinition Definition { get; }
    public string Title { get; }
    public string SearchTitle { get; }
    public string[] GroupPath { get; }
}
