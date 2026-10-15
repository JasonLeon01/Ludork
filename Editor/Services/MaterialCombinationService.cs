using Ludork.Models;
using Ludork.Plugin.Abstractions;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed class MaterialCombinationService
{
    private static readonly JsonSerializerOptions jsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        WriteIndented = true,
    };
    private readonly string path;

    public MaterialCombinationService(string projectPath)
    {
        path = Path.Combine(projectPath, "MaterialExport.json");
    }

    public List<MaterialCombination> Read()
    {
        if (!File.Exists(path))
            return [];

        using JsonDocument document = JsonDocument.Parse(File.ReadAllText(path));
        if (document.RootElement.ValueKind != JsonValueKind.Object
            || !document.RootElement.TryGetProperty("additionalCombinations", out JsonElement items)
            || items.ValueKind != JsonValueKind.Array)
            throw new InvalidDataException(LocaleService.Get("MATERIAL_COMBINATIONS_LOAD_FAILED"));

        List<MaterialCombination> combinations = [];
        foreach (JsonElement item in items.EnumerateArray())
        {
            if (item.ValueKind != JsonValueKind.Object
                || !item.TryGetProperty("material", out JsonElement material)
                || material.ValueKind != JsonValueKind.String
                || !item.TryGetProperty("layer", out JsonElement layer)
                || layer.ValueKind != JsonValueKind.String)
                throw new InvalidDataException(LocaleService.Get("MATERIAL_COMBINATIONS_LOAD_FAILED"));

            string materialPath = material.GetString()!;
            string layerPath = layer.GetString()!;
            if ((materialPath.Length != 0 && !GameDataPath.TryGetMaterialKey(materialPath, out _))
                || !GameDataPath.TryGetMaterialKey(layerPath, out _))
                throw new InvalidDataException(LocaleService.Get("MATERIAL_COMBINATIONS_LOAD_FAILED"));
            combinations.Add(new MaterialCombination(materialPath, layerPath));
        }
        return combinations.Distinct()
            .OrderBy(value => value.Material, StringComparer.Ordinal)
            .ThenBy(value => value.Layer, StringComparer.Ordinal)
            .ToList();
    }

    public Task SaveAsync(IReadOnlyList<MaterialCombination> combinations)
    {
        if (combinations.Count == 0)
            return Task.Run(() => FilePersistence.DeleteFile(path));

        string json = JsonSerializer.Serialize(new { additionalCombinations = combinations }, jsonOptions);
        return FilePersistence.WriteAllTextAtomicAsync(path, json + Environment.NewLine, CancellationToken.None);
    }
}
