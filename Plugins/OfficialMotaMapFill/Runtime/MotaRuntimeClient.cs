using Ludork.Plugin.Abstractions;
using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Plugins.OfficialMotaMapFill.Runtime;

internal sealed class MotaRuntimeClient
{
    private readonly IMapEditorHost host;
    private readonly string scriptPath;

    public MotaRuntimeClient(IMapEditorHost host, string pluginDirectory)
    {
        this.host = host;
        scriptPath = Path.Combine(pluginDirectory, "Runtime", "Evaluate.lua");
    }

    public Task<MotaRuntimeResult> InspectAsync(
        string savePath,
        IReadOnlyList<string> blueprintReferences,
        CancellationToken cancellationToken)
    {
        return executeAsync("inspect", savePath, string.Empty, blueprintReferences, cancellationToken);
    }

    public Task<MotaRuntimeResult> EvaluateAsync(
        string savePath,
        string playerKey,
        IReadOnlyList<string> blueprintReferences,
        CancellationToken cancellationToken)
    {
        return executeAsync("evaluate", savePath, playerKey, blueprintReferences, cancellationToken);
    }

    private async Task<MotaRuntimeResult> executeAsync(
        string operation,
        string savePath,
        string playerKey,
        IReadOnlyList<string> blueprintReferences,
        CancellationToken cancellationToken)
    {
        JsonArray blueprints = [];
        foreach (string reference in blueprintReferences)
            blueprints.Add(reference);
        JsonObject request = new()
        {
            ["operation"] = operation,
            ["savePath"] = savePath,
            ["playerKey"] = playerKey,
            ["blueprints"] = blueprints,
        };
        PluginProjectScriptResult result = await host.RunProjectScriptAsync(scriptPath, request, cancellationToken);
        if (!result.Success || result.Data is null)
            return MotaRuntimeResult.Failed(result.Error);
        JsonObject data = result.Data;
        if (data["success"]?.GetValue<bool>() != true)
            return MotaRuntimeResult.Failed(data["error"]?.GetValue<string>() ?? "Runtime evaluation failed.");
        List<MotaPlayerInfo> players = [];
        foreach (JsonNode? node in requireArray(data, "players"))
        {
            JsonObject player = requireObject(node);
            players.Add(new MotaPlayerInfo(
                requireString(player, "key"),
                requireString(player, "name"),
                requireNumber(player, "hp"),
                requireNumber(player, "attack"),
                requireNumber(player, "defense")));
        }
        List<MotaBlueprintInfo> definitions = [];
        foreach (JsonNode? node in requireArray(data, "blueprints"))
        {
            JsonObject blueprint = requireObject(node);
            MotaActorCategory category = requireString(blueprint, "category") switch
            {
                "door" => MotaActorCategory.Door,
                "enemy" => MotaActorCategory.Enemy,
                "item" => MotaActorCategory.Item,
                "teleporter" => MotaActorCategory.Teleporter,
                "unsupported" => MotaActorCategory.Unsupported,
                _ => throw new InvalidDataException("Unknown runtime Actor category."),
            };
            double? damage = blueprint["damage"] is null ? null : requireNumber(blueprint, "damage");
            definitions.Add(new MotaBlueprintInfo(
                requireString(blueprint, "reference"),
                category,
                blueprint["doorKeyId"]?.GetValue<string>() ?? string.Empty,
                blueprint["doorKeyCount"]?.GetValue<int>() ?? 0,
                damage,
                blueprint["battleResultCode"]?.GetValue<int>(),
                blueprint["canWin"]?.GetValue<bool>() ?? false));
        }
        return new MotaRuntimeResult(true, string.Empty, players, definitions,
            requireString(data, "selectedPlayerKey"));
    }

    private static JsonObject requireObject(JsonNode? node) =>
        node as JsonObject ?? throw new InvalidDataException("Runtime result must contain objects.");

    private static JsonArray requireArray(JsonObject data, string name) =>
        data[name] as JsonArray ?? throw new InvalidDataException("Missing runtime result array: " + name);

    private static string requireString(JsonObject data, string name) =>
        data[name]?.GetValue<string>() ?? throw new InvalidDataException("Missing runtime result string: " + name);

    private static double requireNumber(JsonObject data, string name)
    {
        double value = data[name]?.GetValue<double>()
            ?? throw new InvalidDataException("Missing runtime result number: " + name);
        return double.IsFinite(value) ? value
            : throw new InvalidDataException("Non-finite runtime result number: " + name);
    }
}
