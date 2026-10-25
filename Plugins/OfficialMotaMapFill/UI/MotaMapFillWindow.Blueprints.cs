using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Platform.Storage;
using Ludork.Plugin.Abstractions;
using Ludork.Plugin.Avalonia;
using Ludork.Plugins.OfficialMotaMapFill.Runtime;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;

namespace Ludork.Plugins.OfficialMotaMapFill.UI;

internal sealed partial class MotaMapFillWindow
{
    private async Task chooseBlueprintsAsync()
    {
        if (string.IsNullOrEmpty(savePath.Text) || (playerSelector.SelectedItem as ComboBoxItem)?.Tag is not string playerKey)
            return;
        string root = Path.GetFullPath(Path.Combine(host.ProjectPath, "Data", "Blueprints"));
        IStorageFolder? folder = await StorageProvider.TryGetFolderFromPathAsync(root);
        IReadOnlyList<IStorageFile> files = await StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = localizer.Text("chooseBlueprints"),
            AllowMultiple = true,
            SuggestedStartLocation = folder,
            FileTypeFilter = [new FilePickerFileType(localizer.Text("blueprintFiles")) { Patterns = ["*.json"] }],
        });
        if (closed || files.Count == 0)
            return;
        blueprints = host.ListActorBlueprints();
        Dictionary<string, PluginActorBlueprintSummary> catalog = blueprints.ToDictionary(blueprint => blueprint.Key, StringComparer.Ordinal);
        HashSet<string> additions = new(StringComparer.Ordinal);
        foreach (IStorageFile file in files)
        {
            string? reference = getBlueprintReference(root, file.TryGetLocalPath());
            if (reference is null || !catalog.ContainsKey(reference))
            {
                status.Text = localizer.Format("invalidBlueprintFile", file.Name);
                return;
            }
            if (!selected.Contains(reference))
                additions.Add(reference);
        }
        if (additions.Count == 0)
            return;
        status.Text = localizer.Text("evaluating");
        MotaRuntimeResult result = await runtime.EvaluateAsync(savePath.Text, playerKey, runtimeReferences(additions), cancellation.Token);
        if (closed)
            return;
        if (!result.Success)
        {
            status.Text = localizer.Format("operationError", result.Error);
            return;
        }
        Dictionary<string, MotaBlueprintInfo> definitions = result.Blueprints.ToDictionary(blueprint => blueprint.Reference, StringComparer.Ordinal);
        foreach (string reference in additions)
        {
            if (!definitions.TryGetValue(reference, out MotaBlueprintInfo? blueprint) || !isSelectable(blueprint))
            {
                status.Text = localizer.Format("unsupportedBlueprint", catalog[reference].DisplayName);
                return;
            }
        }
        selected.UnionWith(additions);
        applyEvaluation(result);
        MotaActorCategory firstCategory = definitions[additions.First()].Category;
        categories.SelectedIndex = firstCategory switch
        {
            MotaActorCategory.Door => 0,
            MotaActorCategory.Enemy => 1,
            _ => 2,
        };
    }

    private static string? getBlueprintReference(string root, string? path)
    {
        if (string.IsNullOrEmpty(path) || !string.Equals(Path.GetExtension(path), ".json", StringComparison.OrdinalIgnoreCase))
            return null;
        string relative = Path.GetRelativePath(root, Path.GetFullPath(path)).Replace('\\', '/');
        if (Path.IsPathRooted(relative) || relative == ".." || relative.StartsWith("../", StringComparison.Ordinal))
            return null;
        return "Data.Blueprints." + relative[..^5].Replace('/', '.');
    }

    private static bool isSelectable(MotaBlueprintInfo blueprint) => blueprint.Category is
        MotaActorCategory.Enemy or MotaActorCategory.Item
        || blueprint.Category == MotaActorCategory.Door && blueprint.DoorKeyId is "KEY_Y" or "KEY_B";

    private void rebuildChoices()
    {
        if (!choosingActors || evaluation is null)
            return;
        int selectedTab = Math.Max(0, categories.SelectedIndex);
        categories.Items.Clear();
        Dictionary<string, string> names = blueprints.ToDictionary(blueprint => blueprint.Key, blueprint => blueprint.DisplayName, StringComparer.Ordinal);
        foreach ((MotaActorCategory category, string title) in new[]
        {
            (MotaActorCategory.Door, "doors"), (MotaActorCategory.Enemy, "enemies"), (MotaActorCategory.Item, "items"),
        })
        {
            ListBox list = new();
            foreach (MotaBlueprintInfo blueprint in evaluation.Blueprints.Where(blueprint => blueprint.Category == category
                && selected.Contains(blueprint.Reference) && names.ContainsKey(blueprint.Reference))
                .OrderBy(blueprint => blueprint.Reference, StringComparer.Ordinal))
            {
                string name = names[blueprint.Reference];
                ListBoxItem item = new()
                {
                    Tag = blueprint,
                    HorizontalContentAlignment = HorizontalAlignment.Stretch,
                    Content = new StackPanel
                    {
                        Spacing = 2,
                        Children =
                        {
                            new TextBlock { Text = name, TextWrapping = TextWrapping.Wrap },
                            new TextBlock { Text = describe(blueprint), FontSize = 12, Foreground = PluginTheme.Brush("TextMuted"), TextWrapping = TextWrapping.Wrap },
                        },
                    },
                };
                MenuItem remove = new() { Header = localizer.Text("removeBlueprint") };
                remove.Click += (_, _) => removeBlueprint(blueprint.Reference);
                item.ContextMenu = new ContextMenu { Items = { remove } };
                ToolTip.SetTip(item, blueprint.Reference);
                item.PointerEntered += (_, _) => showBlueprint(blueprint, name);
                list.Items.Add(item);
            }
            list.SelectionChanged += (_, _) =>
            {
                if (list.SelectedItem is ListBoxItem { Tag: MotaBlueprintInfo blueprint })
                    showBlueprint(blueprint, names[blueprint.Reference]);
            };
            list.KeyDown += (_, args) =>
            {
                if (args.Key is Key.Delete or Key.Back && args.KeyModifiers == KeyModifiers.None
                    && list.SelectedItem is ListBoxItem { Tag: MotaBlueprintInfo blueprint })
                {
                    removeBlueprint(blueprint.Reference);
                    args.Handled = true;
                }
            };
            categories.Items.Add(new TabItem
            {
                Header = localizer.Text(title),
                Content = list.Items.Count > 0 ? list : new Border { Padding = new Avalonia.Thickness(12), Child = hint("emptyBlueprints") },
            });
        }
        categories.SelectedIndex = Math.Min(selectedTab, categories.Items.Count - 1);
    }

    private void removeBlueprint(string reference)
    {
        if (busy || !selected.Remove(reference))
            return;
        blueprintCanvas.SetMap(null, null, Array.Empty<(int, int)>(), false);
        blueprintLabel.Text = string.Empty;
        rebuildChoices();
        updateControls();
    }

    private string describe(MotaBlueprintInfo blueprint)
    {
        if (blueprint.Category == MotaActorCategory.Door)
            return localizer.Format("doorCost", blueprint.DoorKeyId, blueprint.DoorKeyCount);
        if (blueprint.Category != MotaActorCategory.Enemy)
            return blueprint.Reference;
        if (!blueprint.CanWin)
            return localizer.Text("unbeatable");
        double hp = evaluation?.Players.FirstOrDefault(player => player.Key == evaluation.SelectedPlayerKey)?.Hp ?? 0;
        return localizer.Format(blueprint.Damage > hp / 2 ? "highDamage" : "damage", blueprint.Damage ?? 0);
    }

    private void showBlueprint(MotaBlueprintInfo blueprint, string name)
    {
        PluginMapActorSnapshot actor;
        try
        {
            actor = host.ReadActorBlueprintPreview(blueprint.Reference) with { X = 1, Y = 1 };
        }
        catch (Exception exception) when (exception is ArgumentException or KeyNotFoundException or IOException or InvalidOperationException)
        {
            blueprintCanvas.SetMap(null, null, Array.Empty<(int, int)>(), false);
            blueprintLabel.Text = name;
            status.Text = localizer.Format("operationError", exception.Message);
            return;
        }
        IReadOnlyList<int?>[] tiles = Enumerable.Range(0, 3).Select(_ => (IReadOnlyList<int?>)new int?[3]).ToArray();
        IReadOnlyList<string?>[] autoTiles = Enumerable.Range(0, 3).Select(_ => (IReadOnlyList<string?>)new string?[3]).ToArray();
        PluginMapLayerSnapshot layer = new("preview", string.Empty, tiles, autoTiles) { Actors = [actor] };
        blueprintCanvas.SetMap(new PluginMapSnapshot("preview", name, 3, 3, string.Empty, [layer]), "preview", Array.Empty<(int, int)>(), false);
        blueprintLabel.Text = $"{name}\n{describe(blueprint)}";
    }
}
