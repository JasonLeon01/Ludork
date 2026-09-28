using Avalonia;
using Ludork.Plugin.Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Documents;
using Avalonia.Media;
using System;
using System.Collections.Generic;
using System.Text;
using System.Text.RegularExpressions;

namespace Ludork.Plugins.OfficialBlueprintAI.UI;

public static class BlueprintAssistantMarkdownRenderer
{
    private static readonly FontFamily CodeFont =
        FontFamily.Parse("Cascadia Mono,Menlo,Monaco,Consolas");

    public static Control Create(string markdown)
    {
        StackPanel content = new() { Spacing = 6 };
        string[] lines = MarkdownSyntax.GetLines(markdown);
        bool inCode = false;
        string codeLanguage = string.Empty;
        StringBuilder code = new();
        for (int index = 0; index < lines.Length; index++)
        {
            string line = lines[index];
            if (line.StartsWith("```", StringComparison.Ordinal))
            {
                if (inCode)
                {
                    content.Children.Add(createCodeBlock(codeLanguage, code.ToString()));
                    code.Clear();
                    codeLanguage = string.Empty;
                    inCode = false;
                }
                else
                {
                    inCode = true;
                    codeLanguage = line[3..].Trim();
                }
                continue;
            }
            if (inCode)
            {
                if (code.Length != 0)
                    code.AppendLine();
                code.Append(line);
                continue;
            }
            if (tryCreateTable(
                    lines,
                    index,
                    out Control table,
                    out int consumedLines))
            {
                content.Children.Add(table);
                index += consumedLines - 1;
                continue;
            }
            Match heading = Regex.Match(line, "^(#{1,6})\\s+(.+)$");
            if (heading.Success)
            {
                TextBlock headingBlock = createInlineBlock(heading.Groups[2].Value);
                headingBlock.FontSize = Math.Max(
                    15,
                    24 - heading.Groups[1].Value.Length * 1.5);
                headingBlock.FontWeight = FontWeight.SemiBold;
                headingBlock.Margin = new Thickness(0, 6, 0, 2);
                content.Children.Add(headingBlock);
                continue;
            }
            Match listItem = Regex.Match(line, "^\\s*([-*+]|\\d+[.)])\\s+(.*)$");
            if (listItem.Success)
            {
                Grid row = new()
                {
                    ColumnDefinitions = new ColumnDefinitions("Auto,*"),
                    ColumnSpacing = 7,
                    Margin = new Thickness(8, 0, 0, 0),
                };
                row.Children.Add(new TextBlock
                {
                    Text = listItem.Groups[1].Value is "-" or "*" or "+"
                        ? "•"
                        : listItem.Groups[1].Value,
                });
                TextBlock item = createInlineBlock(listItem.Groups[2].Value);
                Grid.SetColumn(item, 1);
                row.Children.Add(item);
                content.Children.Add(row);
                continue;
            }
            if (string.IsNullOrWhiteSpace(line))
            {
                content.Children.Add(new Border { Height = 4 });
                continue;
            }
            TextBlock paragraph = createInlineBlock(line);
            content.Children.Add(paragraph);
        }
        if (inCode)
            content.Children.Add(createCodeBlock(codeLanguage, code.ToString()));
        return content;
    }

    private static bool tryCreateTable(
        IReadOnlyList<string> lines,
        int startIndex,
        out Control table,
        out int consumedLines)
    {
        MarkdownTable? parsed = MarkdownSyntax.ReadTable(lines, startIndex, MarkdownSyntax.Profile.Assistant);
        if (parsed is null)
        {
            table = null!;
            consumedLines = 0;
            return false;
        }
        List<IReadOnlyList<string>> rows = [parsed.Header];
        rows.AddRange(parsed.Rows);
        table = createTable(rows, parsed.Alignments);
        consumedLines = parsed.ConsumedLines;
        return true;
    }

    private static Control createTable(
        IReadOnlyList<IReadOnlyList<string>> rows,
        IReadOnlyList<TextAlignment> alignments)
    {
        Grid grid = new();
        for (int column = 0; column < alignments.Count; column++)
        {
            grid.ColumnDefinitions.Add(
                new ColumnDefinition(
                    column == 0 && alignments.Count > 1
                        ? GridLength.Auto
                        : new GridLength(1, GridUnitType.Star)));
        }
        for (int row = 0; row < rows.Count; row++)
        {
            grid.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
            for (int column = 0; column < alignments.Count; column++)
            {
                TextBlock content = createInlineBlock(rows[row][column]);
                content.TextAlignment = alignments[column];
                content.VerticalAlignment = Avalonia.Layout.VerticalAlignment.Center;
                if (row == 0)
                    content.FontWeight = FontWeight.SemiBold;
                Border cell = new()
                {
                    Background = row == 0
                        ? PluginTheme.Brush("Input")
                        : row % 2 == 0
                            ? PluginTheme.Brush("Surface")
                            : Brushes.Transparent,
                    BorderBrush = PluginTheme.Brush("Border"),
                    BorderThickness = new Thickness(
                        column == 0 ? 0 : 1,
                        row == 0 ? 0 : 1,
                        0,
                        0),
                    Padding = new Thickness(9, 6),
                    Child = content,
                };
                Grid.SetColumn(cell, column);
                Grid.SetRow(cell, row);
                grid.Children.Add(cell);
            }
        }
        return new Border
        {
            BorderBrush = PluginTheme.Brush("Border"),
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(4),
            ClipToBounds = true,
            Child = grid,
        };
    }

    private static TextBlock createInlineBlock(string text)
    {
        TextBlock block = new()
        {
            TextWrapping = TextWrapping.Wrap,
        };
        InlineCollection inlines = block.Inlines ?? new InlineCollection();
        block.Inlines = inlines;
        MatchCollection matches = MarkdownSyntax.GetInlineMatches(text, MarkdownSyntax.Profile.Assistant);
        int position = 0;
        foreach (Match match in matches)
        {
            if (match.Index > position)
                inlines.Add(new Run(text[position..match.Index]));
            appendToken(inlines, match.Value);
            position = match.Index + match.Length;
        }
        if (position < text.Length)
            inlines.Add(new Run(text[position..]));
        return block;
    }

    private static void appendToken(InlineCollection inlines, string token)
    {
        if (token.StartsWith('`'))
        {
            inlines.Add(new Run("\u2009" + token[1..^1] + "\u2009")
            {
                Background = PluginTheme.Brush("Input"),
                Foreground = new SolidColorBrush(Color.Parse("#ce9178")),
                FontFamily = CodeFont,
            });
            return;
        }
        if (token.StartsWith("**", StringComparison.Ordinal)
            || token.StartsWith("__", StringComparison.Ordinal))
        {
            inlines.Add(new Run(token[2..^2]) { FontWeight = FontWeight.Bold });
            return;
        }
        if (token.StartsWith('*'))
        {
            inlines.Add(new Run(token[1..^1]) { FontStyle = FontStyle.Italic });
            return;
        }
        Match link = Regex.Match(token, @"^\[([^\]]+)\]\(([^)]+)\)$");
        if (!link.Success)
        {
            inlines.Add(new Run(token));
            return;
        }
        inlines.Add(new Run(link.Groups[1].Value)
        {
            Foreground = PluginTheme.Brush("Accent"),
            TextDecorations = TextDecorations.Underline,
        });
        inlines.Add(new Run(" (" + link.Groups[2].Value + ")")
        {
            Foreground = PluginTheme.Brush("TextMuted"),
        });
    }

    private static Border createCodeBlock(string language, string code)
    {
        StackPanel panel = new();
        if (language.Length != 0)
        {
            panel.Children.Add(new TextBlock
            {
                Text = language.ToUpperInvariant(),
                FontFamily = CodeFont,
                FontSize = 12,
                FontWeight = FontWeight.SemiBold,
                Foreground = PluginTheme.Brush("TextMuted"),
                Margin = new Thickness(10, 5),
            });
        }
        panel.Children.Add(new SelectableTextBlock
        {
            Text = code,
            FontFamily = CodeFont,
            FontSize = 14,
            Foreground = PluginTheme.Brush("Text"),
            TextWrapping = TextWrapping.Wrap,
            Padding = new Thickness(10),
        });
        return new Border
        {
            Background = PluginTheme.Brush("Background"),
            BorderBrush = PluginTheme.Brush("Border"),
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(4),
            ClipToBounds = true,
            Child = panel,
        };
    }
}
