using Avalonia.Controls;
using Ludork.Plugin.Abstractions;
using Ludork.Plugins.OfficialMotaMapFill.Generation;
using Ludork.Plugins.OfficialMotaMapFill.Runtime;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Security.Cryptography;
using System.Threading.Tasks;

namespace Ludork.Plugins.OfficialMotaMapFill.UI;

internal sealed partial class MotaMapFillWindow
{
    private async Task generateAsync()
    {
        if (currentMap is null || currentLayer is null || selected.Count == 0)
            return;
        await evaluatePlayerAsync();
        if (closed || evaluation is not { Success: true })
            return;
        PluginMapSnapshot map = currentMap;
        PluginMapLayerSnapshot layer = currentLayer;
        if (map.Passage.Count != map.Height || map.Passage.Any(row => row.Count != map.Width))
            throw new InvalidOperationException(localizer.Text("missingPassage"));
        bool[,] walkable = new bool[map.Height, map.Width];
        MotaConnections[,] connections = new MotaConnections[map.Height, map.Width];
        for (int y = 0; y < map.Height; y++)
        {
            for (int x = 0; x < map.Width; x++)
            {
                PluginMapPassage passage = map.Passage[y][x];
                walkable[y, x] = passage.Passable;
                connections[y, x] = (passage.Up ? MotaConnections.Up : MotaConnections.None)
                    | (passage.Right ? MotaConnections.Right : MotaConnections.None)
                    | (passage.Down ? MotaConnections.Down : MotaConnections.None)
                    | (passage.Left ? MotaConnections.Left : MotaConnections.None);
            }
        }
        Dictionary<string, MotaBlueprintInfo> definitions = evaluation.Blueprints.ToDictionary(
            blueprint => blueprint.Reference, StringComparer.Ordinal);
        HashSet<MotaCell> reserved = [];
        HashSet<MotaCell> items = [];
        HashSet<MotaCell> teleporters = [];
        foreach (PluginMapLayerSnapshot existingLayer in map.Layers)
        {
            foreach (PluginMapActorSnapshot actor in existingLayer.Actors)
            {
                if (!definitions.TryGetValue(actor.BlueprintReference, out MotaBlueprintInfo? definition))
                    throw new InvalidOperationException(localizer.Format("unresolvedActor", actor.Tag));
                MotaCell cell = new(actor.X, actor.Y);
                bool teleporter = definition.Category == MotaActorCategory.Teleporter;
                bool retained = existingLayer.Name != layer.Name || !region.Contains(cell.X, cell.Y) || teleporter;
                if (!retained)
                    continue;
                reserved.Add(cell);
                if (teleporter)
                    teleporters.Add(cell);
                if (definition.Category == MotaActorCategory.Item)
                    items.Add(cell);
            }
        }
        List<MotaActorCandidate> candidates = [];
        foreach (MotaBlueprintInfo blueprint in evaluation.Blueprints.Where(blueprint => selected.Contains(blueprint.Reference)))
        {
            MotaCandidateKind? kind = blueprint.Category switch
            {
                MotaActorCategory.Item => MotaCandidateKind.Item,
                MotaActorCategory.Enemy => MotaCandidateKind.Enemy,
                MotaActorCategory.Door when blueprint.DoorKeyId == "KEY_Y" => MotaCandidateKind.YellowDoor,
                MotaActorCategory.Door when blueprint.DoorKeyId == "KEY_B" => MotaCandidateKind.BlueDoor,
                _ => null,
            };
            if (kind is null)
                continue;
            MotaBattleCode code = blueprint.Category != MotaActorCategory.Enemy ? MotaBattleCode.Win
                : blueprint.BattleResultCode switch
                {
                    1 => MotaBattleCode.Win,
                    2 => MotaBattleCode.CannotDamage,
                    3 => MotaBattleCode.Lethal,
                    _ => throw new InvalidOperationException(localizer.Text("invalidBattleResult")),
                };
            candidates.Add(new MotaActorCandidate(blueprint.Reference, kind.Value, code, blueprint.Damage ?? 0));
        }
        MotaPlayerInfo player = evaluation.Players.First(player => player.Key == evaluation.SelectedPlayerKey);
        MotaMapFillRequest request = new(walkable, connections,
            new MotaRegion(region.X, region.Y, region.Width, region.Height), reserved, items, teleporters,
            candidates, player.Hp, RandomNumberGenerator.GetInt32(int.MaxValue));
        status.Text = localizer.Text("generating");
        MotaMapFillResult generated = await Task.Run(() => MotaMapFillGenerator.Generate(request, cancellation.Token), cancellation.Token);
        if (!generated.Succeeded)
        {
            status.Text = localizer.Text(generated.ErrorKey);
            return;
        }
        PluginMapActorRegionWriteRequest write = new(map.Key, layer.Name, map.Revision,
            new PluginMapRegion(region.X, region.Y, region.Width, region.Height), "Source.MapActors.Teleporter",
            generated.Placements.Select(placement => new PluginMapActorPlacement(placement.BlueprintPath, placement.Cell.X, placement.Cell.Y)).ToArray());
        PluginMapWriteResult result = await host.ReplaceActorsInRegionAndSaveAsync(write, cancellation.Token);
        if (closed)
            return;
        if (!result.Success)
        {
            if (result.Conflict)
            {
                currentMap = host.ReadMap(map.Key);
                populateLayers(layer.Name);
                region = fullRegion();
                evaluation = null;
                onLayerChanged();
                showStep(false);
                status.Text = localizer.Text("conflict");
            }
            else
                status.Text = localizer.Format("operationError", result.Error);
            return;
        }
        currentMap = host.ReadMap(map.Key);
        currentLayer = currentMap.Layers.First(candidate => candidate.Name == layer.Name);
        refreshMapPreview();
        generate.Content = localizer.Text("regenerate");
        int doorCount = generated.Placements.Count(placement => placement.Kind is MotaCandidateKind.YellowDoor or MotaCandidateKind.BlueDoor);
        int enemyCount = generated.Placements.Count(placement => placement.Kind == MotaCandidateKind.Enemy);
        int itemCount = generated.Placements.Count(placement => placement.Kind == MotaCandidateKind.Item);
        status.Text = localizer.Format("saved", doorCount, enemyCount, itemCount, generated.RoomCount, generated.Seed);
        if (generated.SkippedReasons.Count > 0)
            status.Text += "\n" + string.Join("\n", generated.SkippedReasons.Select(localizer.Text));
    }
}
