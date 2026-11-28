using System;
using System.Collections.Generic;

namespace Ludork.ViewModels;

internal sealed class FileExplorerClipboard
{
    public List<string> Paths { get; } = [];
    public bool Cut { get; set; }
    public WeakReference<FileExplorerEntryViewModel>? CopiedFolder { get; set; }
}
