namespace Ludork.Services;

internal static class ActorMaterialReference
{
    public const string Standard = GameDataPath.Materials + "/Global/Standard.json";

    public static string Resolve(string? path) => string.IsNullOrEmpty(path) ? Standard : path;

    public static bool IsField(string name, string? sourceClass) => name == "material" && sourceClass == "Engine.Actor";
}
