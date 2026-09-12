using Ludork.Services;

namespace Ludork.ViewModels;

public sealed record FileExplorerFileEventArgs(string Path, DataFileInfo? Info);
