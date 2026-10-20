using Ludork.EditorTutorial.Models;
using Ludork.Services;
using System.Globalization;
using System.Linq;

namespace Ludork.EditorTutorial.Playback;

internal static class TutorialText
{
    public static string Format(TutorialElement element, bool isMacOS)
    {
        string text = LocaleService.Get(element.TextKey!);
        if (element.TextArguments.Count == 0)
            return text;
        object[] arguments = element.TextArguments.Select(argument => (object)(argument switch
        {
            "primaryModifier" => isMacOS ? "Command" : "Ctrl",
            "alternateModifier" => isMacOS ? "Option" : "Alt",
            _ => throw new System.ArgumentException($"Unknown tutorial text argument '{argument}'.")
        })).ToArray();
        return string.Format(CultureInfo.CurrentCulture, text, arguments);
    }
}
