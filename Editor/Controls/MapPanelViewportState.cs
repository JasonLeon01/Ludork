using Avalonia;

namespace Ludork.Controls;

public sealed record MapPanelViewportState(int TileSize, double ContinuousTileSize, Vector Offset);
