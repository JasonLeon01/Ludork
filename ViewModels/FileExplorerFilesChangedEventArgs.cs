using System;
using System.Collections.Generic;

namespace Ludork.ViewModels;

public sealed class FileExplorerFilesChangedEventArgs : EventArgs
{
    public FileExplorerFilesChangedEventArgs(
        IReadOnlyList<string> added,
        IReadOnlyList<(string OldPath, string NewPath)> moved,
        IReadOnlyList<string> deleted)
    {
        Added = added;
        Moved = moved;
        Deleted = deleted;
    }

    public IReadOnlyList<string> Added { get; }
    public IReadOnlyList<(string OldPath, string NewPath)> Moved { get; }
    public IReadOnlyList<string> Deleted { get; }
}
