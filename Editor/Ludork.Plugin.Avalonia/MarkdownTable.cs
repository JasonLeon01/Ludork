using Avalonia.Media;
using System.Collections.Generic;

namespace Ludork.Plugin.Avalonia;

public sealed record MarkdownTable(
    IReadOnlyList<string> Header,
    IReadOnlyList<TextAlignment> Alignments,
    IReadOnlyList<IReadOnlyList<string>> Rows,
    int ConsumedLines);
