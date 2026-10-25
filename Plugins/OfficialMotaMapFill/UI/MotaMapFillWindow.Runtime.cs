using Avalonia.Controls;
using Avalonia.Platform.Storage;
using Ludork.Plugin.Abstractions;
using Ludork.Plugins.OfficialMotaMapFill.Runtime;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;

namespace Ludork.Plugins.OfficialMotaMapFill.UI;

internal sealed partial class MotaMapFillWindow
{
    private async Task chooseSaveAsync()
    {
        string? directory = string.IsNullOrEmpty(savePath.Text)
            ? await host.GetSaveDirectoryAsync(cancellation.Token)
            : Path.GetDirectoryName(savePath.Text);
        IStorageFolder? startLocation = null;
        while (!string.IsNullOrEmpty(directory))
        {
            startLocation = await StorageProvider.TryGetFolderFromPathAsync(directory);
            if (startLocation is not null)
                break;
            directory = Path.GetDirectoryName(Path.TrimEndingDirectorySeparator(directory));
        }
        if (closed)
            return;
        IReadOnlyList<IStorageFile> files = await StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = localizer.Text("chooseSave"),
            AllowMultiple = false,
            SuggestedStartLocation = startLocation,
            FileTypeFilter = [new FilePickerFileType(localizer.Text("saveFiles")) { Patterns = ["*.json", "*.ldc"] }],
        });
        if (files.Count == 0 || files[0].TryGetLocalPath() is not string path)
            return;
        savePath.Text = path;
        await loadSaveAsync();
    }

    private IReadOnlyList<string> runtimeReferences(IEnumerable<string>? additions = null)
    {
        return selected.Concat(additions ?? Enumerable.Empty<string>())
            .Concat(currentMap?.Layers.SelectMany(layer => layer.Actors).Select(actor => actor.BlueprintReference)
                ?? Enumerable.Empty<string>())
            .Where(reference => !string.IsNullOrWhiteSpace(reference))
            .Distinct(StringComparer.Ordinal).Order(StringComparer.Ordinal).ToArray();
    }

    private async Task loadSaveAsync()
    {
        if (currentMap is null || string.IsNullOrEmpty(savePath.Text))
            return;
        string? previousPlayer = (playerSelector.SelectedItem as ComboBoxItem)?.Tag as string;
        evaluation = null;
        attributes.Text = string.Empty;
        status.Text = localizer.Text("loadingSave");
        blueprints = host.ListActorBlueprints();
        MotaRuntimeResult inspected = await runtime.InspectAsync(savePath.Text, runtimeReferences(), cancellation.Token);
        if (closed)
            return;
        refreshing = true;
        playerSelector.Items.Clear();
        if (inspected.Success)
        {
            foreach (MotaPlayerInfo player in inspected.Players)
                playerSelector.Items.Add(new ComboBoxItem { Content = $"{player.Name} ({player.Key})", Tag = player.Key });
            playerSelector.SelectedItem = playerSelector.Items.OfType<ComboBoxItem>()
                .FirstOrDefault(item => string.Equals(item.Tag as string, previousPlayer, StringComparison.Ordinal))
                ?? playerSelector.Items.OfType<ComboBoxItem>().FirstOrDefault();
        }
        refreshing = false;
        if (!inspected.Success)
        {
            status.Text = localizer.Format("operationError", inspected.Error);
            return;
        }
        await evaluatePlayerAsync();
    }

    private async Task evaluatePlayerAsync()
    {
        evaluation = null;
        attributes.Text = string.Empty;
        if (string.IsNullOrEmpty(savePath.Text) || (playerSelector.SelectedItem as ComboBoxItem)?.Tag is not string playerKey)
            return;
        status.Text = localizer.Text("evaluating");
        MotaRuntimeResult result = await runtime.EvaluateAsync(savePath.Text, playerKey, runtimeReferences(), cancellation.Token);
        if (closed)
            return;
        if (!result.Success)
        {
            status.Text = localizer.Format("operationError", result.Error);
            return;
        }
        applyEvaluation(result);
    }

    private void applyEvaluation(MotaRuntimeResult result)
    {
        evaluation = result;
        MotaPlayerInfo player = result.Players.First(player => player.Key == result.SelectedPlayerKey);
        attributes.Text = localizer.Format("attributes", player.Hp, player.Attack, player.Defense);
        selected.IntersectWith(result.Blueprints.Where(blueprint => isSelectable(blueprint)
            && blueprints.Any(candidate => candidate.Key == blueprint.Reference)).Select(blueprint => blueprint.Reference));
        status.Text = localizer.Text(choosingActors ? "chooseBlueprintsHint" : "evaluated");
        if (choosingActors)
            rebuildChoices();
    }
}
