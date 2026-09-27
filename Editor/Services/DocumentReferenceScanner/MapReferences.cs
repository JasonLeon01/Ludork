using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal sealed partial class DocumentReferenceScanner
{
    private void scanMapReferences(string sourceId, string key, JsonObject data)
    {
        JsonObject? overrides = data["BPClassVarChanged"] as JsonObject;
        string overridesPath = $"Maps/{key}.BPClassVarChanged";
        if (data["layers"] is JsonObject layers)
        {
            foreach (KeyValuePair<string, JsonNode?> pair in layers)
            {
                cancellationToken.ThrowIfCancellationRequested();
                if (pair.Value is not JsonObject layer)
                    continue;
                string? tilesetKey = getString(layer["layerTileset"]);
                if (!string.IsNullOrWhiteSpace(tilesetKey))
                {
                    addReference(
                        sourceId,
                        nodeId("tileset", tilesetKey),
                        "tileset",
                        $"Maps/{key}.layers.{pair.Key}.layerTileset");
                }
                addAssetReference(
                    sourceId,
                    layer["shaderPath"],
                    "asset",
                    $"Maps/{key}.layers.{pair.Key}.shaderPath");
                scanAutoTileReferences(
                    sourceId,
                    layer["autoTiles"],
                    $"Maps/{key}.layers.{pair.Key}.autoTiles");
                scanMapActorReferences(
                    sourceId,
                    layer["actors"],
                    $"Maps/{key}.layers.{pair.Key}.actors", overrides, overridesPath);
                scanKnownMapNodeReferences(
                    sourceId,
                    layer["actors"],
                    $"Maps/{key}.layers.{pair.Key}.actors");
            }
        }
        if (data["actors"] is JsonObject actorsByLayer)
        {
            foreach (KeyValuePair<string, JsonNode?> pair in actorsByLayer)
            {
                scanMapActorReferences(sourceId, pair.Value, $"Maps/{key}.actors.{pair.Key}", overrides, overridesPath);
                scanKnownMapNodeReferences(sourceId, pair.Value, $"Maps/{key}.actors.{pair.Key}");
            }
        }
        else
        {
            scanMapActorReferences(sourceId, data["actors"], $"Maps/{key}.actors", overrides, overridesPath);
            scanKnownMapNodeReferences(sourceId, data["actors"], $"Maps/{key}.actors");
        }
        addAssetReference(sourceId, data["bgm"], "asset", $"Maps/{key}.bgm");
        addAssetReference(sourceId, data["bgs"], "asset", $"Maps/{key}.bgs");
        addAssetReference(sourceId, data["fog"], "asset", $"Maps/{key}.fog");
        addAssetReference(sourceId, data["panorama"], "asset", $"Maps/{key}.panorama");
        scanGenericReferences(sourceId, data["BPClassVarChanged"], $"Maps/{key}.BPClassVarChanged");
        scanKnownMapNodeReferences(
            sourceId,
            data["BPClassVarChanged"],
            $"Maps/{key}.BPClassVarChanged");
    }

    private void scanKnownMapNodeReferences(string sourceId, JsonNode? node, string path)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (node is JsonObject objectValue)
        {
            string? nodeFunction = getString(objectValue["nodeFunction"]);
            if (nodeFunction is not null
                && isKnownMapNodeReference(nodeFunction)
                && objectValue["params"] is JsonArray { Count: > 0 } parameters)
            {
                addMapReference(sourceId, parameters[0], "nodeParam", $"{path}.params[0]");
            }
            foreach (KeyValuePair<string, JsonNode?> pair in objectValue)
            {
                if (pair.Value is not null)
                    scanKnownMapNodeReferences(sourceId, pair.Value, $"{path}.{pair.Key}");
            }
            return;
        }
        if (node is not JsonArray arrayValue)
            return;
        for (int index = 0; index < arrayValue.Count; index += 1)
        {
            if (arrayValue[index] is JsonNode child)
                scanKnownMapNodeReferences(sourceId, child, $"{path}[{index}]");
        }
    }

    private void scanWorldMapReferences(string sourceId, string key, JsonObject data)
    {
        addAssetReference(sourceId, data["fog"], "asset", $"Maps/{key}/_world.fog");
        addAssetReference(sourceId, data["panorama"], "asset", $"Maps/{key}/_world.panorama");
        if (data["placements"] is not JsonArray placements)
            return;
        for (int index = 0; index < placements.Count; index += 1)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (placements[index] is not JsonObject placement
                || getString(placement["map"]) is not string fileName)
            {
                continue;
            }
            string childName = Path.ChangeExtension(fileName.Replace('\\', '/'), null) ?? string.Empty;
            if (childName.Length == 0 || childName.Contains('/'))
                continue;
            addReference(
                sourceId,
                nodeId("map", $"{key}/{childName}"),
                "worldPlacement",
                $"Maps/{key}/_world.placements[{index}].map");
        }
    }

    private static bool isKnownMapNodeReference(string nodeFunction)
    {
        return nodeFunction.EndsWith(".GotoMap", StringComparison.Ordinal)
            || nodeFunction.EndsWith(".RecordTelepoint", StringComparison.Ordinal);
    }

    private void addMapReference(string sourceId, JsonNode? value, string kind, string path)
    {
        string? key = normalizeReferenceParam(value);
        if (string.IsNullOrWhiteSpace(key))
            return;
        key = Path.ChangeExtension(key.Replace('\\', '/'), null) ?? key;
        if (key.EndsWith("/_world", StringComparison.OrdinalIgnoreCase))
        {
            addReference(sourceId, nodeId("worldMap", key[..^"/_world".Length]), kind, path);
            return;
        }
        addReference(sourceId, nodeId("map", key), kind, path);
    }
}
