using Avalonia;
using Avalonia.Automation;
using Avalonia.Controls;
using Avalonia.Controls.Documents;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using Ludork.Views.Utils;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

namespace Ludork.Views;

public partial class MarkdownPreviewWindow : Window
{
    private enum TableAlignment
    {
        Left,
        Center,
        Right,
    }

    private enum ImageFailure
    {
        Invalid,
        Missing,
        Remote,
        Unsafe,
        Unnamed,
    }

    private static readonly IBrush codeForeground = new SolidColorBrush(Color.Parse("#dcdcdc"));
    private static readonly IBrush codeKeyword = new SolidColorBrush(Color.Parse("#c586c0"));
    private static readonly IBrush codeString = new SolidColorBrush(Color.Parse("#ce9178"));
    private static readonly IBrush codeNumber = new SolidColorBrush(Color.Parse("#b5cea8"));
    private static readonly IBrush codeComment = new SolidColorBrush(Color.Parse("#6a9955"));
    private static readonly IBrush codeVariable = new SolidColorBrush(Color.Parse("#9cdcfe"));
    private static readonly IBrush codeDirective = new SolidColorBrush(Color.Parse("#dcdcaa"));
    private static readonly IBrush codeCommand = new SolidColorBrush(Color.Parse("#4ec9b0"));
    private static readonly FontFamily codeFont = FontFamily.Parse("Cascadia Mono,Menlo,Monaco,Consolas");
    private bool singleFile;
    private readonly List<MarkdownDocumentEntry> entries = [];
    private readonly List<MarkdownDocumentSection> sections = [];
    private readonly Dictionary<MarkdownDocumentEntry, MarkdownDocumentSection> entrySections = [];
    private readonly List<MarkdownHeading> headings = [];
    private readonly List<Bitmap> renderedBitmaps = [];
    private readonly List<Image> renderedImages = [];
    private readonly Dictionary<string, MarkdownHeading> headingAnchors = new(StringComparer.Ordinal);
    private readonly Dictionary<string, int> headingAnchorCounts = new(StringComparer.Ordinal);
    private string documentRoot = string.Empty;
    private string imageRoot = string.Empty;
    private IReadOnlyList<MarkdownDocumentEntry> documentRoots = [];
    private MarkdownDocumentEntry? selectedEntry;
    private MarkdownDocumentSection? selectedSection;
    private bool changingSelection;
    private bool changingSection;
    private int renderSerial;

    public MarkdownPreviewWindow()
    {
        InitializeComponent();
        EditorInputs.ApplyEditable(SearchBox);
        SearchBox.PlaceholderText = LocaleService.Get("SEARCH");
        KeyDown += onKeyDown;
        DocumentScrollViewer.SizeChanged += onDocumentViewportSizeChanged;
    }

    public MarkdownPreviewWindow(string path, string title, string imageRoot) : this()
    {
        Title = title;
        string fullPath = Path.GetFullPath(path);
        singleFile = File.Exists(fullPath);
        documentRoot = singleFile
            ? Path.GetDirectoryName(fullPath) ?? fullPath
            : fullPath;
        this.imageRoot = Path.GetFullPath(imageRoot);
        documentRoots = collectDocuments(fullPath);
        SectionTabsScrollViewer.IsVisible = !singleFile;
        SectionTabsGap.IsVisible = !singleFile;
        if (singleFile)
        {
            DocumentTree.ItemsSource = documentRoots;
            selectInitialDocument();
            return;
        }
        createSections();
        SectionTabs.ItemsSource = sections;
        if (sections.Count > 0)
            activateSection(sections[0], null);
        else
            renderMarkdown("No markdown files");
    }

    private IReadOnlyList<MarkdownDocumentEntry> collectDocuments(string path)
    {
        if (singleFile)
        {
            if (isSymbolicLinkOrInaccessible(path))
                return [];
            MarkdownDocumentEntry entry = new(Path.GetFileNameWithoutExtension(path), path, false);
            entries.Add(entry);
            return [entry];
        }
        if (!Directory.Exists(path) || isSymbolicLinkOrInaccessible(path))
            return [];
        IReadOnlyList<MarkdownDocumentEntry> roots = collectDirectory(path, true);
        foreach (MarkdownDocumentEntry root in roots)
            indexEntry(root);
        return roots;
    }

    private IReadOnlyList<MarkdownDocumentEntry> collectDirectory(string directory, bool topLevel = false)
    {
        List<MarkdownDocumentEntry> result = [];
        foreach (string child in Directory.EnumerateFileSystemEntries(directory).OrderBy(getSortKey, StringComparer.OrdinalIgnoreCase))
        {
            if (isSymbolicLinkOrInaccessible(child))
                continue;
            if (Directory.Exists(child))
            {
                if (Path.GetFileName(child).Equals("_images", StringComparison.OrdinalIgnoreCase))
                    continue;
                IReadOnlyList<MarkdownDocumentEntry> nestedEntries = collectDirectory(child);
                if (nestedEntries.Count == 0)
                    continue;
                MarkdownDocumentEntry entry = new(Path.GetFileName(child), child, true);
                foreach (MarkdownDocumentEntry nested in nestedEntries)
                    entry.Children.Add(nested);
                result.Add(entry);
            }
            else if (Path.GetExtension(child).Equals(".md", StringComparison.OrdinalIgnoreCase))
            {
                string displayName = topLevel
                    ? LocaleService.Get("DOCUMENTATION_OVERVIEW")
                    : Path.GetFileNameWithoutExtension(child);
                MarkdownDocumentEntry entry = new(displayName, child, false);
                result.Add(entry);
            }
        }
        return result;
    }

    private void indexEntry(MarkdownDocumentEntry entry)
    {
        entries.Add(entry);
        foreach (MarkdownDocumentEntry child in entry.Children)
            indexEntry(child);
    }

    private void createSections()
    {
        MarkdownDocumentEntry? overview = documentRoots.FirstOrDefault(root => !root.IsDirectory);
        MarkdownDocumentEntry? gettingStarted = documentRoots.FirstOrDefault(root => root.IsDirectory);
        bool mergedGettingStarted = false;
        if (overview is not null && gettingStarted is not null)
        {
            mergedGettingStarted = true;
            IReadOnlyList<MarkdownDocumentEntry> treeEntries = [overview, .. gettingStarted.Children];
            MarkdownDocumentSection section = new(
                getDisplayName(gettingStarted.DisplayName),
                gettingStarted,
                treeEntries);
            sections.Add(section);
            indexSectionEntry(overview, section);
            indexSectionEntry(gettingStarted, section);
        }
        foreach (MarkdownDocumentEntry root in documentRoots)
        {
            if (mergedGettingStarted
                && (ReferenceEquals(root, overview) || ReferenceEquals(root, gettingStarted)))
                continue;
            IReadOnlyList<MarkdownDocumentEntry> treeEntries = root.IsDirectory
                ? root.Children
                : [root];
            string displayName = root.IsDirectory
                ? getDisplayName(root.DisplayName)
                : LocaleService.Get("DOCUMENTATION_OVERVIEW");
            MarkdownDocumentSection section = new(displayName, root, treeEntries);
            sections.Add(section);
            indexSectionEntry(root, section);
        }
    }

    private void indexSectionEntry(MarkdownDocumentEntry entry, MarkdownDocumentSection section)
    {
        entrySections[entry] = section;
        foreach (MarkdownDocumentEntry child in entry.Children)
            indexSectionEntry(child, section);
    }

    private static string getSortKey(string path)
    {
        string name = Path.GetFileNameWithoutExtension(path);
        Match match = Regex.Match(name, @"^(\d+)[\.\s_-]*(.*)$");
        return match.Success
            ? $"0{int.Parse(match.Groups[1].Value):D8}{match.Groups[2].Value}"
            : $"1{name.ToLowerInvariant()}";
    }

    private static string getDisplayName(string name)
    {
        string displayName = Regex.Replace(name, @"^\d+[\.\s_-]*", string.Empty);
        return displayName.Length > 0 ? displayName : name;
    }

    private void selectInitialDocument()
    {
        MarkdownDocumentEntry? initial = entries.FirstOrDefault(entry => !entry.IsDirectory && entry.IsVisible);
        if (initial is not null)
            navigateToEntry(initial, string.Empty);
        else
            renderMarkdown("No markdown files");
    }

    private void onSectionSelectionChanged(object? sender, SelectionChangedEventArgs args)
    {
        if (changingSection
            || sender is not TabStrip tabStrip
            || tabStrip.SelectedItem is not MarkdownDocumentSection section)
            return;
        activateSection(section, null);
    }

    private void activateSection(MarkdownDocumentSection section, MarkdownDocumentEntry? preferredEntry)
    {
        changingSection = true;
        SectionTabs.SelectedItem = section;
        changingSection = false;
        selectedSection = section;
        DocumentTree.ItemsSource = section.TreeEntries;
        MarkdownDocumentEntry? targetEntry = preferredEntry;
        if (targetEntry is null || !targetEntry.IsVisible)
            targetEntry = section.SelectedEntry;
        if (targetEntry is null || !targetEntry.IsVisible)
            targetEntry = findFirstVisibleEntry(section);
        changingSelection = true;
        DocumentTree.SelectedItem = targetEntry;
        changingSelection = false;
        if (targetEntry is null)
        {
            selectedEntry = null;
            renderMarkdown("No markdown files");
            return;
        }
        section.SelectedEntry = targetEntry;
        selectedEntry = targetEntry;
        loadEntry(targetEntry);
    }

    private MarkdownDocumentEntry? findFirstVisibleEntry(MarkdownDocumentSection section)
    {
        MarkdownDocumentEntry? document = entries.FirstOrDefault(entry =>
            !entry.IsDirectory
            && entry.IsVisible
            && entrySections.TryGetValue(entry, out MarkdownDocumentSection? entrySection)
            && ReferenceEquals(entrySection, section));
        return document ?? entries.FirstOrDefault(entry =>
            entry.IsVisible
            && (!ReferenceEquals(entry, section.RootEntry) || !entry.IsDirectory)
            && entrySections.TryGetValue(entry, out MarkdownDocumentSection? entrySection)
            && ReferenceEquals(entrySection, section));
    }

    private void onDocumentSelectionChanged(object? sender, SelectionChangedEventArgs args)
    {
        if (changingSelection
            || DocumentTree.SelectedItem is not MarkdownDocumentEntry entry
            || !entry.IsVisible)
            return;
        selectedEntry = entry;
        if (selectedSection is not null)
            selectedSection.SelectedEntry = entry;
        loadEntry(entry);
    }

    private void loadEntry(MarkdownDocumentEntry entry)
    {
        string markdown = entry.IsDirectory
            ? buildDirectoryContents(entry)
            : File.ReadAllText(entry.Path, Encoding.UTF8);
        renderMarkdown(markdown);
        DocumentScrollViewer.Offset = new Vector();
    }

    private void renderMarkdown(string markdown)
    {
        clearRenderedContent();
        headings.Clear();
        headingAnchors.Clear();
        headingAnchorCounts.Clear();
        renderSerial++;
        StringBuilder paragraph = new();
        string[] lines = markdown.Replace("\r\n", "\n").Replace('\r', '\n').Split('\n');

        void flushParagraph()
        {
            if (paragraph.Length == 0)
                return;
            addTextBlock(paragraph.ToString(), 16, new Thickness(0, 0, 0, 6));
            paragraph.Clear();
        }

        int index = 0;
        while (index < lines.Length)
        {
            string sourceLine = lines[index];
            string line = sourceLine.TrimEnd();
            string trimmedStart = line.TrimStart();
            if (trimmedStart.StartsWith("```", StringComparison.Ordinal))
            {
                flushParagraph();
                string language = trimmedStart[3..].Trim();
                StringBuilder code = new();
                index++;
                while (index < lines.Length && !lines[index].TrimStart().StartsWith("```", StringComparison.Ordinal))
                {
                    if (code.Length > 0)
                        code.AppendLine();
                    code.Append(lines[index]);
                    index++;
                }
                if (index < lines.Length)
                    index++;
                addCodeBlock(language, code.ToString());
                continue;
            }
            Match headingMatch = Regex.Match(line, "^(#{1,6})\\s+(.+)$");
            if (headingMatch.Success)
            {
                flushParagraph();
                addHeading(headingMatch.Groups[2].Value, headingMatch.Groups[1].Value.Length);
                index++;
                continue;
            }
            Match imageMatch = Regex.Match(line, @"^\s*!\[(?<alt>[^\]]*)\]\((?<target>[^)]+)\)\s*$");
            if (imageMatch.Success)
            {
                flushParagraph();
                addImageBlock(imageMatch.Groups["alt"].Value, imageMatch.Groups["target"].Value);
                index++;
                continue;
            }
            if (string.IsNullOrWhiteSpace(line))
            {
                flushParagraph();
                index++;
                continue;
            }
            if (index + 1 < lines.Length && isTableStart(line, lines[index + 1]))
            {
                flushParagraph();
                List<string> header = splitTableRow(line);
                List<TableAlignment> alignments = splitTableRow(lines[index + 1])
                    .Select(getTableAlignment)
                    .ToList();
                List<IReadOnlyList<string>> rows = [];
                index += 2;
                while (index < lines.Length && isTableDataLine(lines[index]))
                {
                    rows.Add(normaliseTableRow(splitTableRow(lines[index]), header.Count));
                    index++;
                }
                addTable(header, alignments, rows);
                continue;
            }
            if (Regex.IsMatch(line, "^[-*_]{3,}$"))
            {
                flushParagraph();
                DocumentPreview.Children.Add(new Separator { Margin = new Thickness(0, 4) });
                index++;
                continue;
            }
            Match listMatch = Regex.Match(line, "^(?<indent>\\s*)(?<marker>[-*+] |\\d+\\. )(?<text>.+)$");
            if (listMatch.Success)
            {
                flushParagraph();
                int level = listMatch.Groups["indent"].Value.Length / 2;
                string prefix = char.IsDigit(listMatch.Groups["marker"].Value[0])
                    ? listMatch.Groups["marker"].Value
                    : "• ";
                addTextBlock(prefix + listMatch.Groups["text"].Value, 17, new Thickness(level * 20, 0, 0, 2));
                index++;
                continue;
            }
            if (line.StartsWith(">", StringComparison.Ordinal))
            {
                flushParagraph();
                addTextBlock(line.TrimStart('>', ' '), 16, new Thickness(16, 0, 0, 4), FontStyle.Italic);
                index++;
                continue;
            }
            if (paragraph.Length > 0)
                paragraph.AppendLine();
            paragraph.Append(line);
            index++;
        }
        flushParagraph();
    }

    private void addHeading(string text, int level)
    {
        string baseAnchor = createAnchor(text);
        if (baseAnchor.Length == 0)
            baseAnchor = "section";
        headingAnchorCounts.TryGetValue(baseAnchor, out int duplicateCount);
        headingAnchorCounts[baseAnchor] = duplicateCount + 1;
        string anchor = duplicateCount == 0 ? baseAnchor : $"{baseAnchor}-{duplicateCount}";
        MarkdownHeading heading = new(text, level, anchor);
        Button button = new()
        {
            Classes = { "markdownHeading" },
            FontSize = level switch
            {
                1 => 30,
                2 => 26,
                3 => 23,
                4 => 21,
                5 => 19,
                _ => 18,
            },
            FontWeight = FontWeight.Bold,
            Margin = new Thickness(0, level == 1 ? 4 : 10, 0, 2),
            Tag = heading,
            Focusable = level > 1,
            IsHitTestVisible = level > 1,
        };
        heading.Button = button;
        updateHeadingText(heading);
        if (level > 1)
            button.Click += onHeadingClicked;
        headings.Add(heading);
        headingAnchors[anchor] = heading;
        DocumentPreview.Children.Add(button);
    }

    private void addTextBlock(string text, double fontSize, Thickness margin, FontStyle fontStyle = FontStyle.Normal, FontFamily? fontFamily = null)
    {
        TextBlock block = createTextBlock(text, fontSize, fontStyle, fontFamily);
        block.Margin = margin;
        DocumentPreview.Children.Add(block);
    }

    private TextBlock createTextBlock(
        string text,
        double fontSize,
        FontStyle fontStyle = FontStyle.Normal,
        FontFamily? fontFamily = null)
    {
        TextBlock block = new()
        {
            FontSize = fontSize,
            FontStyle = fontStyle,
            TextWrapping = TextWrapping.Wrap,
        };
        if (fontFamily is not null)
            block.FontFamily = fontFamily;
        appendInlineMarkdown(getInlines(block), text);
        return block;
    }

    private static InlineCollection getInlines(TextBlock block)
    {
        InlineCollection inlines = block.Inlines ?? new InlineCollection();
        block.Inlines = inlines;
        return inlines;
    }

    private void appendInlineMarkdown(InlineCollection inlines, string text, bool linksEnabled = true)
    {
        MatchCollection matches = Regex.Matches(
            text,
            @"`[^`\n]+`|\*\*[^*\n]+?\*\*|__[^_\n]+?__|~~[^~\n]+?~~|(?<!\*)\*[^*\n]+?\*(?!\*)|!?\[[^\]]*\]\([^)]+\)");
        int position = 0;
        foreach (Match match in matches)
        {
            if (match.Index > position)
                inlines.Add(new Run(text[position..match.Index]));
            appendInlineToken(inlines, match.Value, linksEnabled);
            position = match.Index + match.Length;
        }
        if (position < text.Length)
            inlines.Add(new Run(text[position..]));
    }

    private void appendInlineToken(InlineCollection inlines, string token, bool linksEnabled)
    {
        if (token.StartsWith('`'))
        {
            Run code = new("\u2009" + token[1..^1] + "\u2009")
            {
                Background = new SolidColorBrush(Color.Parse("#2d2d2d")),
                Foreground = codeString,
                FontFamily = codeFont,
            };
            inlines.Add(code);
            return;
        }
        if (token.StartsWith("**", StringComparison.Ordinal) || token.StartsWith("__", StringComparison.Ordinal))
        {
            inlines.Add(new Run(token[2..^2]) { FontWeight = FontWeight.Bold });
            return;
        }
        if (token.StartsWith("~~", StringComparison.Ordinal))
        {
            inlines.Add(new Run(token[2..^2]) { TextDecorations = TextDecorations.Strikethrough });
            return;
        }
        if (token.StartsWith('*'))
        {
            inlines.Add(new Run(token[1..^1]) { FontStyle = FontStyle.Italic });
            return;
        }
        Match link = Regex.Match(token, @"^!?\[([^\]]+)\]\(([^)]+)\)$");
        if (!link.Success)
        {
            inlines.Add(new Run(token));
            return;
        }
        string label = link.Groups[1].Value;
        string target = link.Groups[2].Value;
        if (token.StartsWith("![", StringComparison.Ordinal) || !linksEnabled)
        {
            inlines.Add(new Run(label)
            {
                Foreground = new SolidColorBrush(Color.Parse("#7ec8ff")),
                TextDecorations = TextDecorations.Underline,
            });
            if (token.StartsWith("![", StringComparison.Ordinal))
            {
                inlines.Add(new Run(" (" + target + ")")
                {
                    Foreground = new SolidColorBrush(Color.Parse("#a0a0a0")),
                });
            }
            return;
        }
        TextBlock linkText = new()
        {
            Text = label,
            TextDecorations = TextDecorations.Underline,
        };
        Button linkButton = new()
        {
            Classes = { "markdownLink" },
            Content = linkText,
            Tag = target,
        };
        linkButton.Click += onLinkClicked;
        inlines.Add(linkButton);
    }

    private void addImageBlock(string alt, string target)
    {
        string value = unwrapLinkTarget(target);
        if (Uri.TryCreate(value, UriKind.Absolute, out Uri? absoluteUri))
        {
            bool remote = absoluteUri.Scheme.Equals(Uri.UriSchemeHttps, StringComparison.OrdinalIgnoreCase)
                || absoluteUri.Scheme.Equals(Uri.UriSchemeHttp, StringComparison.OrdinalIgnoreCase);
            addImagePlaceholder(alt, remote
                ? getImageFailureText(ImageFailure.Remote)
                : getImageFailureText(ImageFailure.Unsafe));
            return;
        }
        string baseDirectory = getCurrentDocumentDirectory();
        if (!tryResolveLocalPath(value, baseDirectory, imageRoot, out string imagePath))
        {
            addImagePlaceholder(alt, getImageFailureText(ImageFailure.Unsafe));
            return;
        }
        if (!File.Exists(imagePath))
        {
            addImagePlaceholder(alt, getImageFailureText(ImageFailure.Missing));
            return;
        }
        Bitmap bitmap;
        try
        {
            bitmap = new Bitmap(imagePath);
        }
        catch (ArgumentException)
        {
            addImagePlaceholder(alt, getImageFailureText(ImageFailure.Invalid));
            return;
        }
        catch (IOException)
        {
            addImagePlaceholder(alt, getImageFailureText(ImageFailure.Invalid));
            return;
        }
        catch (UnauthorizedAccessException)
        {
            addImagePlaceholder(alt, getImageFailureText(ImageFailure.Unsafe));
            return;
        }
        catch (NotSupportedException)
        {
            addImagePlaceholder(alt, getImageFailureText(ImageFailure.Invalid));
            return;
        }
        catch (InvalidOperationException)
        {
            addImagePlaceholder(alt, getImageFailureText(ImageFailure.Invalid));
            return;
        }
        renderedBitmaps.Add(bitmap);
        Image image = new()
        {
            Source = bitmap,
            Stretch = Stretch.Uniform,
            StretchDirection = StretchDirection.DownOnly,
            HorizontalAlignment = Avalonia.Layout.HorizontalAlignment.Left,
            Margin = new Thickness(0, 4, 0, 8),
        };
        AutomationProperties.SetName(image, alt);
        renderedImages.Add(image);
        updateImageWidth(image);
        DocumentPreview.Children.Add(image);
    }

    private void addImagePlaceholder(string alt, string reason)
    {
        string label = string.IsNullOrWhiteSpace(alt)
            ? getImageFailureText(ImageFailure.Unnamed)
            : alt.Trim();
        TextBlock text = new()
        {
            Text = $"{label} — {reason}",
            FontSize = 15,
            Foreground = new SolidColorBrush(Color.Parse("#f2b8b5")),
            TextWrapping = TextWrapping.Wrap,
        };
        Border placeholder = new()
        {
            Background = new SolidColorBrush(Color.Parse("#2d2020")),
            BorderBrush = new SolidColorBrush(Color.Parse("#7c4545")),
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(4),
            Padding = new Thickness(12, 10),
            Margin = new Thickness(0, 4, 0, 8),
            Child = text,
        };
        DocumentPreview.Children.Add(placeholder);
    }

    private static string getImageFailureText(ImageFailure reason)
    {
        bool chinese = LocaleService.CurrentLanguage.StartsWith("zh", StringComparison.OrdinalIgnoreCase);
        return reason switch
        {
            ImageFailure.Remote => chinese ? "不允许加载远程图片" : "Remote images are not allowed",
            ImageFailure.Unsafe => chinese ? "图片路径无效或超出允许的图片目录" : "The image path is invalid or outside the allowed image directory",
            ImageFailure.Missing => chinese ? "找不到图片文件" : "The image file was not found",
            ImageFailure.Unnamed => chinese ? "未命名图片" : "Unnamed image",
            _ => chinese ? "图片无法解码" : "The image could not be decoded",
        };
    }

    private void onDocumentViewportSizeChanged(object? sender, SizeChangedEventArgs args)
    {
        foreach (Image image in renderedImages)
            updateImageWidth(image);
    }

    private void updateImageWidth(Image image)
    {
        double availableWidth = DocumentScrollViewer.Viewport.Width - DocumentPreview.Margin.Left - DocumentPreview.Margin.Right;
        if (availableWidth > 0)
            image.MaxWidth = availableWidth;
    }

    private void clearRenderedContent()
    {
        foreach (Image image in renderedImages)
            image.Source = null;
        renderedImages.Clear();
        DocumentPreview.Children.Clear();
        foreach (Bitmap bitmap in renderedBitmaps)
            bitmap.Dispose();
        renderedBitmaps.Clear();
    }

    private void onLinkClicked(object? sender, RoutedEventArgs args)
    {
        if (sender is not Button { Tag: string rawTarget })
            return;
        string target = unwrapLinkTarget(rawTarget);
        if (target.StartsWith('#'))
        {
            navigateToAnchor(target[1..]);
            args.Handled = true;
            return;
        }
        if (Uri.TryCreate(target, UriKind.Absolute, out Uri? absoluteUri))
        {
            if (absoluteUri.Scheme.Equals(Uri.UriSchemeHttps, StringComparison.OrdinalIgnoreCase)
                && absoluteUri.Host.Length > 0)
                openExternalLink(absoluteUri);
            args.Handled = true;
            return;
        }
        int fragmentIndex = target.IndexOf('#');
        string pathTarget = fragmentIndex < 0 ? target : target[..fragmentIndex];
        string fragment = fragmentIndex < 0 ? string.Empty : target[(fragmentIndex + 1)..];
        if (pathTarget.IndexOf('?') >= 0
            || !tryResolveLocalPath(pathTarget, getCurrentDocumentDirectory(), documentRoot, out string documentPath)
            || !Path.GetExtension(documentPath).Equals(".md", StringComparison.OrdinalIgnoreCase)
            || !File.Exists(documentPath))
        {
            args.Handled = true;
            return;
        }
        MarkdownDocumentEntry? entry = entries.FirstOrDefault(candidate =>
            !candidate.IsDirectory && pathsEqual(candidate.Path, documentPath));
        if (entry is null && singleFile)
        {
            entry = new MarkdownDocumentEntry(Path.GetFileNameWithoutExtension(documentPath), documentPath, false);
            entries.Add(entry);
        }
        if (entry is null)
        {
            args.Handled = true;
            return;
        }
        navigateToEntry(entry, fragment);
        args.Handled = true;
    }

    private void navigateToEntry(MarkdownDocumentEntry entry, string fragment)
    {
        if (!entry.IsVisible)
            SearchBox.Text = string.Empty;
        if (entrySections.TryGetValue(entry, out MarkdownDocumentSection? section))
        {
            activateSection(section, entry);
            if (fragment.Length > 0)
                queueAnchorNavigation(fragment);
            return;
        }
        changingSelection = true;
        DocumentTree.SelectedItem = entry;
        changingSelection = false;
        selectedEntry = entry;
        loadEntry(entry);
        if (fragment.Length > 0)
            queueAnchorNavigation(fragment);
    }

    private void navigateToAnchor(string fragment)
    {
        if (fragment.Length == 0)
        {
            DocumentScrollViewer.Offset = new Vector();
            return;
        }
        string decodedFragment;
        try
        {
            decodedFragment = Uri.UnescapeDataString(fragment);
        }
        catch (UriFormatException)
        {
            return;
        }
        string anchor = createAnchor(decodedFragment);
        if (!headingAnchors.TryGetValue(anchor, out MarkdownHeading? heading))
            return;
        foreach (MarkdownHeading candidate in headings.Where(candidate => candidate.IsCollapsed))
        {
            candidate.IsCollapsed = false;
            updateHeadingText(candidate);
        }
        refreshHeadingVisibility();
        heading.Button.BringIntoView();
    }

    private void queueAnchorNavigation(string fragment)
    {
        int serial = renderSerial;
        Dispatcher.UIThread.Post(() =>
        {
            if (serial == renderSerial)
                navigateToAnchor(fragment);
        }, DispatcherPriority.Loaded);
    }

    private static void openExternalLink(Uri uri)
    {
        try
        {
            Process.Start(new ProcessStartInfo(uri.AbsoluteUri) { UseShellExecute = true });
        }
        catch (Win32Exception)
        {
        }
        catch (InvalidOperationException)
        {
        }
    }

    private string getCurrentDocumentDirectory()
    {
        if (selectedEntry is null)
            return documentRoot;
        if (selectedEntry.IsDirectory)
            return selectedEntry.Path;
        return Path.GetDirectoryName(selectedEntry.Path) ?? documentRoot;
    }

    private static bool tryResolveLocalPath(
        string rawPath,
        string baseDirectory,
        string allowedRoot,
        out string fullPath)
    {
        fullPath = string.Empty;
        if (string.IsNullOrWhiteSpace(rawPath))
            return false;
        string decodedPath;
        try
        {
            decodedPath = Uri.UnescapeDataString(rawPath);
        }
        catch (UriFormatException)
        {
            return false;
        }
        if (decodedPath.IndexOf('\0') >= 0 || Path.IsPathRooted(decodedPath))
            return false;
        try
        {
            fullPath = Path.GetFullPath(Path.Combine(baseDirectory, decodedPath));
        }
        catch (ArgumentException)
        {
            return false;
        }
        catch (NotSupportedException)
        {
            return false;
        }
        catch (IOException)
        {
            return false;
        }
        string relativePath = Path.GetRelativePath(allowedRoot, fullPath);
        bool contained = !Path.IsPathRooted(relativePath)
            && !relativePath.Equals("..", StringComparison.Ordinal)
            && !relativePath.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal)
            && !relativePath.StartsWith(".." + Path.AltDirectorySeparatorChar, StringComparison.Ordinal);
        return contained
            && !isSymbolicLinkOrInaccessible(allowedRoot)
            && !containsNestedSymbolicLink(fullPath, allowedRoot);
    }

    private static bool containsNestedSymbolicLink(string path, string allowedRoot)
    {
        string current = path;
        while (!pathsEqual(current, allowedRoot))
        {
            if ((File.Exists(current) || Directory.Exists(current))
                && isSymbolicLinkOrInaccessible(current))
                return true;
            string? parent = Path.GetDirectoryName(current);
            if (parent is null || pathsEqual(parent, current))
                return true;
            current = parent;
        }
        return false;
    }

    private static bool isSymbolicLinkOrInaccessible(string path)
    {
        try
        {
            return (File.GetAttributes(path) & FileAttributes.ReparsePoint) != 0;
        }
        catch (IOException)
        {
            return true;
        }
        catch (UnauthorizedAccessException)
        {
            return true;
        }
    }

    private static string unwrapLinkTarget(string target)
    {
        string value = target.Trim();
        return value.Length >= 2 && value.StartsWith('<') && value.EndsWith('>')
            ? value[1..^1]
            : value;
    }

    private static bool pathsEqual(string left, string right)
    {
        StringComparison comparison = OperatingSystem.IsWindows() || OperatingSystem.IsMacOS()
            ? StringComparison.OrdinalIgnoreCase
            : StringComparison.Ordinal;
        string leftPath = Path.GetFullPath(left).Normalize(NormalizationForm.FormC);
        string rightPath = Path.GetFullPath(right).Normalize(NormalizationForm.FormC);
        return leftPath.Equals(rightPath, comparison);
    }

    private static string createAnchor(string heading)
    {
        string plain = Regex.Replace(heading, @"!?\[([^\]]*)\]\([^)]+\)", "$1");
        plain = Regex.Replace(plain, @"[`*_~]", string.Empty).Normalize(NormalizationForm.FormC).ToLowerInvariant();
        StringBuilder anchor = new();
        bool pendingSeparator = false;
        foreach (char character in plain)
        {
            if (char.IsLetterOrDigit(character) || character is '-' or '_')
            {
                if (pendingSeparator && anchor.Length > 0 && anchor[^1] != '-')
                    anchor.Append('-');
                anchor.Append(character);
                pendingSeparator = false;
            }
            else if (char.IsWhiteSpace(character))
                pendingSeparator = true;
        }
        return anchor.ToString().Trim('-');
    }

    private void addCodeBlock(string language, string code)
    {
        string languageKey = getCodeLanguageKey(language);
        SelectableTextBlock codeBlock = new()
        {
            FontFamily = codeFont,
            FontSize = 14,
            Foreground = codeForeground,
            LineHeight = 20,
            Padding = new Thickness(10),
            TextWrapping = TextWrapping.Wrap,
        };
        appendHighlightedCode(getInlines(codeBlock), languageKey, code);
        TextBlock languageLabel = new()
        {
            Text = getCodeLanguageDisplay(languageKey, language),
            FontFamily = codeFont,
            FontSize = 12,
            FontWeight = FontWeight.SemiBold,
            Foreground = new SolidColorBrush(Color.Parse("#a8a8a8")),
        };
        Border header = new()
        {
            Background = new SolidColorBrush(Color.Parse("#292929")),
            BorderBrush = new SolidColorBrush(Color.Parse("#444444")),
            BorderThickness = new Thickness(0, 0, 0, 1),
            Padding = new Thickness(10, 5),
            Child = languageLabel,
        };
        StackPanel content = new();
        content.Children.Add(header);
        content.Children.Add(codeBlock);
        Border container = new()
        {
            Background = new SolidColorBrush(Color.Parse("#1e1e1e")),
            BorderBrush = new SolidColorBrush(Color.Parse("#444444")),
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(4),
            ClipToBounds = true,
            Margin = new Thickness(0, 4, 0, 8),
            Child = content,
        };
        DocumentPreview.Children.Add(container);
    }

    private static void appendHighlightedCode(InlineCollection inlines, string language, string code)
    {
        string pattern = getCodePattern(language);
        if (string.IsNullOrEmpty(pattern))
        {
            inlines.Add(new Run(code));
            return;
        }
        MatchCollection matches = Regex.Matches(code, pattern, RegexOptions.Multiline);
        int position = 0;
        foreach (Match match in matches)
        {
            if (match.Index > position)
                inlines.Add(new Run(code[position..match.Index]));
            Run token = new(match.Value)
            {
                Foreground = getCodeTokenBrush(match),
            };
            inlines.Add(token);
            position = match.Index + match.Length;
        }
        if (position < code.Length)
            inlines.Add(new Run(code[position..]));
    }

    private static IBrush getCodeTokenBrush(Match match)
    {
        if (match.Groups["comment"].Success)
            return codeComment;
        if (match.Groups["string"].Success)
            return codeString;
        if (match.Groups["number"].Success)
            return codeNumber;
        if (match.Groups["variable"].Success)
            return codeVariable;
        if (match.Groups["directive"].Success)
            return codeDirective;
        if (match.Groups["command"].Success)
            return codeCommand;
        return codeKeyword;
    }

    private static string getCodeLanguageKey(string language)
    {
        string key = language.Trim().Trim('{', '}').Split(' ', StringSplitOptions.RemoveEmptyEntries).FirstOrDefault()?.ToLowerInvariant()
            ?? string.Empty;
        return key switch
        {
            "c" or "cc" or "cpp" or "c++" or "h" or "hpp" => "cpp",
            "py" or "python" or "python3" => "python",
            "sh" or "shell" or "bash" or "zsh" => "bash",
            "ps1" or "pwsh" or "powershell" => "powershell",
            "cs" or "csharp" or "c#" => "csharp",
            "js" or "javascript" => "javascript",
            "bat" or "batch" or "cmd" => "batch",
            "jsonc" => "json",
            _ => key,
        };
    }

    private static string getCodeLanguageDisplay(string languageKey, string sourceLanguage)
    {
        return languageKey switch
        {
            "" => "CODE",
            "cpp" => "C++",
            "python" => "PYTHON",
            "bash" => "BASH",
            "powershell" => "POWERSHELL",
            "csharp" => "C#",
            "javascript" => "JAVASCRIPT",
            "json" => "JSON",
            "lua" => "LUA",
            "batch" => "BATCH",
            "text" or "txt" => "TEXT",
            _ => sourceLanguage.Trim().ToUpperInvariant(),
        };
    }

    private static string getCodePattern(string language)
    {
        return language switch
        {
            "lua" => @"(?<comment>--\[\[[\s\S]*?\]\]|--[^\n]*)|(?<string>\[\[[\s\S]*?\]\]|""(?:\\.|[^""\\])*""|'(?:\\.|[^'\\])*')|(?<number>\b(?:0[xX][0-9A-Fa-f]+|\d+(?:\.\d+)?)\b)|(?<keyword>\b(?:and|break|do|else|elseif|end|false|for|function|goto|if|in|local|nil|not|or|repeat|return|self|then|true|until|while)\b)|(?<command>\b(?:assert|collectgarbage|dofile|error|getmetatable|ipairs|load|next|pairs|pcall|print|rawequal|rawget|rawlen|rawset|require|select|setmetatable|tonumber|tostring|type|xpcall)\b)",
            "cpp" => @"(?<comment>//[^\n]*|/\*[\s\S]*?\*/)|(?<directive>^[ \t]*#[^\n]*)|(?<string>""(?:\\.|[^""\\])*""|'(?:\\.|[^'\\])*')|(?<number>\b(?:0[xX][0-9A-Fa-f]+|\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)[uUlLfF]*\b)|(?<keyword>\b(?:alignas|alignof|and|asm|auto|bool|break|case|catch|char|class|concept|const|constexpr|continue|co_await|co_return|co_yield|decltype|default|delete|do|double|else|enum|explicit|export|extern|false|float|for|friend|if|inline|int|long|namespace|new|noexcept|nullptr|operator|or|override|private|protected|public|register|requires|return|short|signed|sizeof|static|static_assert|struct|switch|template|this|thread_local|throw|true|try|typedef|typename|union|unsigned|using|virtual|void|volatile|while)\b)|(?<command>\b(?:std|string|vector|map|unordered_map|shared_ptr|unique_ptr|optional|variant)\b)",
            "python" => @"(?<comment>#[^\n]*)|(?<string>""(?:\\.|[^""\\])*""|'(?:\\.|[^'\\])*')|(?<number>\b(?:0[xX][0-9A-Fa-f]+|0[bB][01]+|\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)\b)|(?<directive>@[A-Za-z_][A-Za-z0-9_\.]*)|(?<keyword>\b(?:and|as|assert|async|await|break|case|class|continue|def|del|elif|else|except|False|finally|for|from|global|if|import|in|is|lambda|match|None|nonlocal|not|or|pass|raise|return|True|try|while|with|yield)\b)|(?<command>\b(?:abs|all|any|bool|bytes|callable|dict|enumerate|filter|float|format|frozenset|getattr|hasattr|help|input|int|isinstance|iter|len|list|map|max|min|next|object|open|print|property|range|repr|reversed|round|set|setattr|sorted|str|sum|super|tuple|type|vars|zip)\b)",
            "bash" => @"(?<comment>#[^\n]*)|(?<string>""(?:\\.|[^""\\])*""|'[^']*'|`[^`]*`)|(?<variable>\$\{[^}]+\}|\$[A-Za-z_][A-Za-z0-9_]*|\$[0-9@#?$!*\-])|(?<number>\b\d+(?:\.\d+)?\b)|(?<keyword>\b(?:case|do|done|elif|else|esac|fi|for|function|if|in|select|then|time|until|while)\b)|(?<command>\b(?:alias|cd|echo|eval|exec|exit|export|printf|pwd|read|readonly|return|set|shift|source|test|trap|type|typeset|ulimit|umask|unalias|unset)\b)",
            "powershell" => @"(?<comment><#[\s\S]*?#>|#[^\n]*)|(?<string>@?""(?:`.|[^""])*""|@?'(?:''|[^'])*')|(?<variable>\$\{[^}]+\}|\$[A-Za-z_][A-Za-z0-9_:?]*)|(?<number>\b(?:0[xX][0-9A-Fa-f]+|\d+(?:\.\d+)?)\b)|(?<keyword>\b(?:begin|break|catch|class|continue|data|define|do|dynamicparam|else|elseif|end|enum|exit|filter|finally|for|foreach|from|function|if|in|param|process|return|switch|throw|trap|try|until|using|var|while)\b)|(?<command>\b[A-Za-z]+-[A-Za-z][A-Za-z0-9-]*\b)",
            "csharp" => @"(?<comment>//[^\n]*|/\*[\s\S]*?\*/)|(?<directive>^[ \t]*#[^\n]*)|(?<string>@?""(?:""""|\\.|[^""])*""|'(?:\\.|[^'\\])*')|(?<number>\b(?:0[xX][0-9A-Fa-f]+|\d+(?:\.\d+)?)[uUlLmMdDfF]*\b)|(?<keyword>\b(?:abstract|as|async|await|base|bool|break|byte|case|catch|char|checked|class|const|continue|decimal|default|delegate|do|double|else|enum|event|explicit|extern|false|finally|fixed|float|for|foreach|goto|if|implicit|in|int|interface|internal|is|lock|long|namespace|new|null|object|operator|out|override|params|private|protected|public|readonly|record|ref|required|return|sbyte|sealed|short|sizeof|stackalloc|static|string|struct|switch|this|throw|true|try|typeof|uint|ulong|unchecked|unsafe|ushort|using|virtual|void|volatile|while)\b)",
            "javascript" => @"(?<comment>//[^\n]*|/\*[\s\S]*?\*/)|(?<string>""(?:\\.|[^""\\])*""|'(?:\\.|[^'\\])*'|`(?:\\.|[^`\\])*`)|(?<number>\b(?:0[xX][0-9A-Fa-f]+|\d+(?:\.\d+)?)\b)|(?<keyword>\b(?:async|await|break|case|catch|class|const|continue|debugger|default|delete|do|else|export|extends|false|finally|for|from|function|get|if|import|in|instanceof|let|new|null|of|return|set|static|super|switch|this|throw|true|try|typeof|undefined|var|void|while|with|yield)\b)",
            "json" => @"(?<string>""(?:\\.|[^""\\])*""(?=\s*:))|(?<variable>""(?:\\.|[^""\\])*"")|(?<number>-?\b\d+(?:\.\d+)?(?:[eE][+-]?\d+)?\b)|(?<keyword>\b(?:false|null|true)\b)",
            "batch" => @"(?<comment>^[ \t]*(?:rem\b|::)[^\n]*)|(?<string>""[^""]*"")|(?<variable>%[^%\n]+%|![^!\n]+!)|(?<number>\b\d+(?:\.\d+)?\b)|(?<keyword>\b(?:call|do|else|endlocal|equ|errorlevel|exist|for|goto|if|in|lss|neq|not|setlocal)\b)|(?<command>\b(?:cd|chdir|cls|copy|del|dir|echo|erase|exit|md|mkdir|move|path|pause|popd|prompt|pushd|rd|ren|rename|rmdir|set|shift|start|title|type)\b)",
            _ => string.Empty,
        };
    }

    private static bool isTableStart(string headerLine, string separatorLine)
    {
        List<string> header = splitTableRow(headerLine);
        List<string> separator = splitTableRow(separatorLine);
        return header.Count > 0
            && header.Count == separator.Count
            && separator.All(cell => Regex.IsMatch(cell, "^:?-{3,}:?$"));
    }

    private static bool isTableDataLine(string line)
    {
        return !string.IsNullOrWhiteSpace(line) && line.Contains('|');
    }

    private static List<string> splitTableRow(string line)
    {
        string value = line.Trim();
        if (value.StartsWith('|'))
            value = value[1..];
        if (value.EndsWith('|') && !value.EndsWith("\\|", StringComparison.Ordinal))
            value = value[..^1];
        List<string> cells = [];
        StringBuilder cell = new();
        bool inCode = false;
        bool escaped = false;
        foreach (char character in value)
        {
            if (escaped)
            {
                cell.Append(character);
                escaped = false;
                continue;
            }
            if (character == '\\')
            {
                escaped = true;
                continue;
            }
            if (character == '`')
            {
                inCode = !inCode;
                cell.Append(character);
                continue;
            }
            if (character == '|' && !inCode)
            {
                cells.Add(cell.ToString().Trim());
                cell.Clear();
                continue;
            }
            cell.Append(character);
        }
        if (escaped)
            cell.Append('\\');
        cells.Add(cell.ToString().Trim());
        return cells;
    }

    private static TableAlignment getTableAlignment(string separator)
    {
        string value = separator.Trim();
        if (value.StartsWith(':') && value.EndsWith(':'))
            return TableAlignment.Center;
        if (value.EndsWith(':'))
            return TableAlignment.Right;
        return TableAlignment.Left;
    }

    private static IReadOnlyList<string> normaliseTableRow(List<string> cells, int count)
    {
        if (cells.Count > count)
            return cells.Take(count).ToArray();
        while (cells.Count < count)
            cells.Add(string.Empty);
        return cells;
    }

    private void addTable(
        IReadOnlyList<string> header,
        IReadOnlyList<TableAlignment> alignments,
        IReadOnlyList<IReadOnlyList<string>> rows)
    {
        Grid table = new()
        {
            Margin = new Thickness(0, 4, 0, 10),
        };
        for (int column = 0; column < header.Count; column++)
            table.ColumnDefinitions.Add(new ColumnDefinition(new GridLength(1, GridUnitType.Star)));
        table.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
        addTableRow(table, header, alignments, 0, true);
        for (int row = 0; row < rows.Count; row++)
        {
            table.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
            addTableRow(table, rows[row], alignments, row + 1, false);
        }
        DocumentPreview.Children.Add(table);
    }

    private void addTableRow(
        Grid table,
        IReadOnlyList<string> cells,
        IReadOnlyList<TableAlignment> alignments,
        int row,
        bool header)
    {
        for (int column = 0; column < cells.Count; column++)
        {
            TextBlock text = createTextBlock(cells[column], 15);
            text.FontWeight = header ? FontWeight.SemiBold : FontWeight.Normal;
            text.TextAlignment = alignments[column] switch
            {
                TableAlignment.Center => TextAlignment.Center,
                TableAlignment.Right => TextAlignment.Right,
                _ => TextAlignment.Left,
            };
            Border cell = new()
            {
                Background = new SolidColorBrush(Color.Parse(header ? "#2d2d2d" : "#1f1f1f")),
                BorderBrush = new SolidColorBrush(Color.Parse("#555555")),
                BorderThickness = new Thickness(column == 0 ? 1 : 0, row == 0 ? 1 : 0, 1, 1),
                Padding = new Thickness(8, 6),
                Child = text,
            };
            Grid.SetColumn(cell, column);
            Grid.SetRow(cell, row);
            table.Children.Add(cell);
        }
    }

    private void onHeadingClicked(object? sender, RoutedEventArgs args)
    {
        if (sender is not Button { Tag: MarkdownHeading heading })
            return;
        heading.IsCollapsed = !heading.IsCollapsed;
        updateHeadingText(heading);
        refreshHeadingVisibility();
    }

    private void updateHeadingText(MarkdownHeading heading)
    {
        TextBlock content = new()
        {
            FontSize = heading.Button.FontSize,
            FontWeight = FontWeight.Bold,
            TextWrapping = TextWrapping.Wrap,
        };
        InlineCollection inlines = getInlines(content);
        if (heading.Level > 1)
            inlines.Add(new Run((heading.IsCollapsed ? "▶" : "▼") + " "));
        appendInlineMarkdown(inlines, heading.Text, false);
        heading.Button.Content = content;
    }

    private void refreshHeadingVisibility()
    {
        for (int index = 0; index < DocumentPreview.Children.Count; index++)
        {
            Control control = DocumentPreview.Children[index];
            control.IsVisible = !headings.Any(heading =>
            {
                int headingIndex = DocumentPreview.Children.IndexOf(heading.Button);
                if (!heading.IsCollapsed || headingIndex >= index)
                    return false;
                return !headings.Any(next =>
                {
                    int nextIndex = DocumentPreview.Children.IndexOf(next.Button);
                    return nextIndex > headingIndex && nextIndex <= index && next.Level <= heading.Level;
                });
            });
        }
    }

    private static string buildDirectoryContents(MarkdownDocumentEntry entry)
    {
        StringBuilder content = new($"# {entry.DisplayName}\n\n## Contents\n");
        foreach (MarkdownDocumentEntry child in entry.Children)
            content.Append("- ").Append(child.DisplayName).Append(child.IsDirectory ? "/" : string.Empty).AppendLine();
        return content.ToString();
    }

    private void onSearchChanged(object? sender, TextChangedEventArgs args)
    {
        string query = SearchBox.Text?.Trim() ?? string.Empty;
        int matchCount = refreshVisibility(query);
        SearchCountText.Text = string.IsNullOrEmpty(query) ? string.Empty : matchCount.ToString();
        if (selectedEntry is not null && selectedEntry.IsVisible)
            return;
        if (!singleFile && selectedSection is not null)
        {
            MarkdownDocumentEntry? sectionMatch = selectedSection.SelectedEntry;
            if (sectionMatch is null || !sectionMatch.IsVisible)
                sectionMatch = findFirstVisibleEntry(selectedSection);
            if (sectionMatch is not null)
            {
                activateSection(selectedSection, sectionMatch);
                return;
            }
        }
        MarkdownDocumentEntry? match = entries.FirstOrDefault(entry => !entry.IsDirectory && entry.IsVisible);
        if (match is not null)
        {
            navigateToEntry(match, string.Empty);
            return;
        }
        if (singleFile)
            selectInitialDocument();
        else if (selectedSection is not null)
            activateSection(selectedSection, null);
    }

    private int refreshVisibility(string query)
    {
        int matchCount = 0;
        foreach (MarkdownDocumentEntry root in documentRoots)
        {
            if (singleFile || !root.IsDirectory)
            {
                refreshEntryVisibility(root, query, ref matchCount);
                continue;
            }
            bool childMatches = false;
            foreach (MarkdownDocumentEntry child in root.Children)
                childMatches |= refreshEntryVisibility(child, query, ref matchCount);
            root.IsVisible = childMatches;
        }
        return matchCount;
    }

    private static bool refreshEntryVisibility(MarkdownDocumentEntry entry, string query, ref int matchCount)
    {
        bool childMatches = false;
        foreach (MarkdownDocumentEntry child in entry.Children)
            childMatches |= refreshEntryVisibility(child, query, ref matchCount);
        bool matches = string.IsNullOrEmpty(query) || entry.matches(query);
        if (!string.IsNullOrEmpty(query) && matches)
            matchCount++;
        entry.IsVisible = matches || childMatches;
        return entry.IsVisible;
    }

    private void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Key == Key.F && EditorShortcuts.HasPrimaryModifier(args.KeyModifiers))
        {
            SearchBox.Focus();
            SearchBox.SelectAll();
            args.Handled = true;
        }
    }

    protected override void OnClosed(EventArgs args)
    {
        clearRenderedContent();
        base.OnClosed(args);
    }
}

public sealed class MarkdownDocumentEntry : INotifyPropertyChanged
{
    private bool isVisible = true;
    private string? searchText;

    public MarkdownDocumentEntry(string displayName, string path, bool isDirectory)
    {
        DisplayName = displayName;
        Path = path;
        IsDirectory = isDirectory;
    }

    public string DisplayName { get; }
    public string Path { get; }
    public bool IsDirectory { get; }
    public ObservableCollection<MarkdownDocumentEntry> Children { get; } = [];
    public bool IsVisible
    {
        get => isVisible;
        set
        {
            if (isVisible == value)
                return;
            isVisible = value;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(IsVisible)));
        }
    }

    public event PropertyChangedEventHandler? PropertyChanged;

    public bool matches(string query)
    {
        if (DisplayName.Contains(query, StringComparison.OrdinalIgnoreCase))
            return true;
        if (IsDirectory)
            return false;
        searchText ??= readSearchText();
        return searchText.Contains(query, StringComparison.OrdinalIgnoreCase);
    }

    private string readSearchText()
    {
        return File.ReadAllText(Path, Encoding.UTF8);
    }
}

public sealed class MarkdownDocumentSection
{
    public MarkdownDocumentSection(
        string displayName,
        MarkdownDocumentEntry rootEntry,
        IReadOnlyList<MarkdownDocumentEntry> treeEntries)
    {
        DisplayName = displayName;
        RootEntry = rootEntry;
        TreeEntries = treeEntries;
    }

    public string DisplayName { get; }
    public MarkdownDocumentEntry RootEntry { get; }
    public IReadOnlyList<MarkdownDocumentEntry> TreeEntries { get; }
    public MarkdownDocumentEntry? SelectedEntry { get; set; }
}

public sealed class MarkdownHeading
{
    public MarkdownHeading(string text, int level, string anchor)
    {
        Text = text;
        Level = level;
        Anchor = anchor;
    }

    public string Text { get; }
    public int Level { get; }
    public string Anchor { get; }
    public Button Button { get; set; } = null!;
    public bool IsCollapsed { get; set; }
}
