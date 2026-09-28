namespace Ludork.Services;

internal static class ReferenceTypeDisplay
{
    public static string Name(string type)
    {
        string key = type switch
        {
            "asset" => "REFERENCE_TYPE_ASSET",
            "autoTile" => "REFERENCE_TYPE_AUTOTILE",
            "blueprint" => "REFERENCE_TYPE_BLUEPRINT",
            "commonFunction" => "REFERENCE_TYPE_COMMON_FUNCTION",
            "config" => "REFERENCE_TYPE_CONFIG",
            "general" => "REFERENCE_TYPE_GENERAL",
            "generalMember" => "REFERENCE_TYPE_GENERAL_MEMBER",
            "map" => "REFERENCE_TYPE_MAP",
            "animation" => "REFERENCE_TYPE_ANIMATION",
            "tileset" => "REFERENCE_TYPE_TILESET",
            _ => "REFERENCE_TYPE_UNKNOWN",
        };
        return LocaleService.Get(key);
    }
}
