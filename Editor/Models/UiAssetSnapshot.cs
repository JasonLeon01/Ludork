using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class UiAssetSnapshot(JsonObject source) : EditorJsonSnapshot(source)
{
    public string? Type => ReadString("type");
    public UiNodeSnapshot? Root => ReadValue("root") is JsonObject root ? new UiNodeSnapshot(root) : null;
    public UiPaletteSnapshot Palette => new(ReadObject("palette"));
    public IReadOnlyList<UiAnimationSnapshot> Animations => ReadArray("animations").OfType<JsonObject>().Select(value => new UiAnimationSnapshot(value)).ToArray();
    public UiDesignSize DesignSize
    {
        get
        {
            JsonObject size = ReadObject("designSize");
            return new UiDesignSize(ReadFiniteNumber(size["width"]) ?? 640, ReadFiniteNumber(size["height"]) ?? 480, size["width"]?.ToJsonString(), size["height"]?.ToJsonString());
        }
    }

}
