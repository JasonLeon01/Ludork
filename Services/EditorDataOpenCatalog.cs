using System;
using System.Collections.Generic;

namespace Ludork.Services;

internal enum EditorDataOpenTarget
{
    SystemConfig,
    Tilesets,
    AutoTiles,
    Map,
    CommonFunctions,
    Blueprint,
    Animation,
    Curve,
    TextConfig,
    UiAsset,
    GeneralData,
}

internal static class EditorDataOpenCatalog
{
    private static readonly IReadOnlyDictionary<string, EditorDataOpenTarget> Targets =
        new Dictionary<string, EditorDataOpenTarget>(StringComparer.Ordinal)
        {
            ["system"] = EditorDataOpenTarget.SystemConfig,
            ["tileset"] = EditorDataOpenTarget.Tilesets,
            ["autoTile"] = EditorDataOpenTarget.AutoTiles,
            ["map"] = EditorDataOpenTarget.Map,
            ["commonFunction"] = EditorDataOpenTarget.CommonFunctions,
            ["blueprint"] = EditorDataOpenTarget.Blueprint,
            ["animation"] = EditorDataOpenTarget.Animation,
            ["curve"] = EditorDataOpenTarget.Curve,
            ["vector2Curve"] = EditorDataOpenTarget.Curve,
            ["vector3Curve"] = EditorDataOpenTarget.Curve,
            ["vector4Curve"] = EditorDataOpenTarget.Curve,
            ["plainTextConfig"] = EditorDataOpenTarget.TextConfig,
            ["richTextConfig"] = EditorDataOpenTarget.TextConfig,
            ["uiAsset"] = EditorDataOpenTarget.UiAsset,
            ["general"] = EditorDataOpenTarget.GeneralData,
        };

    public static bool TryResolve(string type, out EditorDataOpenTarget target)
    {
        return Targets.TryGetValue(type, out target);
    }
}
