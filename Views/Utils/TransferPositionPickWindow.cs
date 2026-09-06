using Avalonia.Controls;
using Ludork.Services;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

internal static class TransferPositionPickWindow
{
    public static async Task<TransferPositionSelection?> ShowAsync(
        Window owner,
        GameDataService gameData,
        JsonNode? initial,
        string mapReference)
    {
        MapTargetPickerResult? result = await MapTargetPickerWindow.ShowPositionAsync(
            owner,
            gameData,
            mapReference,
            initial);
        return result is null
            ? null
            : new TransferPositionSelection(result.Position, result.RuntimePath);
    }
}
