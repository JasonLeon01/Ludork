using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class ConfigDataService
{
    public int getCellSize()
    {
        return EngineConstants.CellSize;
    }

    public (int Width, int Height) getGameSize()
    {
        if (!SystemConfigData.TryGetValue("System", out ConfigSnapshot? system)
            || !system.Fields.TryGetValue("gameSize", out ConfigFieldSnapshot? size)
            || size.CurrentValue is not JsonArray values
            || values.Count < 2)
        {
            return (640, 480);
        }
        int width = values[0]?.GetValue<int?>() ?? 640;
        int height = values[1]?.GetValue<int?>() ?? 480;
        return (width > 0 ? width : 640, height > 0 ? height : 480);
    }

    public string getGameTitle()
    {
        if (!SystemConfigData.TryGetValue("System", out ConfigSnapshot? system))
            return "Ludork";
        string? title = system.Fields.TryGetValue("title", out ConfigFieldSnapshot? field) ? field.CurrentValue?.GetValue<string>() : null;
        return string.IsNullOrWhiteSpace(title) ? "Ludork" : $"Ludork - {title.Trim()}";
    }

}
