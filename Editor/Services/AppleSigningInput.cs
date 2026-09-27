using System.Linq;

namespace Ludork.Services;

internal static class AppleSigningInput
{
    public static bool IsValidTeamId(string value) =>
        value.Length == 10
        && value.All(character =>
            (character >= 'A' && character <= 'Z')
            || (character >= '0' && character <= '9'));
}
