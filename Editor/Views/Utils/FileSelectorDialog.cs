using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Controls.Templates;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Styling;
using Avalonia.Threading;
using Ludork.Controls;
using Ludork.Services;
using Ludork.ViewModels;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

public sealed class FileSelectorDialog : Window
{
    private static readonly StringComparer PathComparer = OperatingSystem.IsWindows() ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal;
    private static readonly HashSet<string> ImageSuffixes = new(StringComparer.OrdinalIgnoreCase)
        { "png", "jpg", "jpeg", "bmp", "gif", "webp" };

    private static readonly HashSet<string> TextSuffixes = new(StringComparer.OrdinalIgnoreCase)
        { "txt", "json", "py", "md", "ini", "xml", "csv", "log", "yaml", "yml",
          "html", "htm", "css", "qss", "bat", "sh", "toml", "cfg", "conf", "vert", "frag", "lua" };

    private const int MaxTextPreviewBytes = 256 * 1024;

    private static IBrush SurfaceBrush => EditorTheme.Brush("Background");
    private static IBrush GridBorderBrush => EditorTheme.Brush("Border");
    private static IBrush AccentBrush => EditorTheme.Brush("Accent");
    private static IBrush TextBrush => EditorTheme.Brush("Text");
    private static IBrush DetailBrush => EditorTheme.Brush("TextMuted");

    private readonly string _root;
    private readonly bool _save;
    private readonly bool _allowMultiple;
    private readonly List<List<string>> _filterPatterns;
    private readonly string[] _filterNames;
    private int _filterIndex;

    private string _currentDirectory;
    private string? _selectedPath;
    private readonly List<string> _selectedPaths = [];
    private string _fileName = string.Empty;
    private readonly EditorThumbnailService _thumbnails;
    private readonly bool _ownsThumbnails;
    private EditorThumbnailLease? _previewLease;
    private CancellationTokenSource _directoryCancellation = new();
    private CancellationTokenSource _previewCancellation = new();
    private bool _closed;
    private bool _opened;
    private bool _updatingSelection;

    private readonly TextBox _lookInBox;
    private readonly Button _upButton;
    private readonly ListBox _fileGrid;
    private readonly Image _previewImage;
    private readonly Panel _previewImageContainer;
    private readonly TextBox _previewTextBox;
    private readonly Panel _previewTextContainer;
    private readonly TextBox _fileNameBox;
    private readonly ComboBox _filterCombo;
    private readonly Button _confirmButton;
    private List<FileEntry> _entries = [];

    public string? SelectedPath => _selectedPath;
    public string SelectedNameFilter => _filterIndex < _filterNames.Length ? _filterNames[_filterIndex] : string.Empty;

    public static string AllFilesFilter(bool star = false) =>
        LocaleService.Get(star ? "FILE_FILTER_ALL_STAR" : "FILE_FILTER_ALL");

    public static string AudioFilesFilter() => LocaleService.Get("FILE_FILTER_AUDIO");
    public static string ImageFilesFilter() => LocaleService.Get("FILE_FILTER_IMAGES");

    public static string FilesFilter(params string[] patterns) =>
        LocaleService.Get("FILE_FILTER_FILES").Replace("{patterns}", string.Join(" ", patterns));

    public static Task<string?> ShowAsync(
        Window owner,
        string root,
        string filterStr,
        string? title = null,
        bool save = false,
        string? initialDirectory = null,
        string? initialFilePath = null)
    {
        FileSelectorDialog dialog = new(
            owner,
            root,
            filterStr,
            title,
            save,
            initialDirectory,
            initialFilePath: initialFilePath);
        return dialog.ShowDialog<string?>(owner);
    }

    public static Task<string[]?> ShowMultipleAsync(
        Window owner,
        string root,
        string filterStr,
        string? title = null,
        string? initialDirectory = null)
    {
        FileSelectorDialog dialog = new(owner, root, filterStr, title, allowMultiple: true, initialDirectory: initialDirectory);
        return dialog.ShowDialog<string[]?>(owner);
    }

    public static async Task<string?> SelectLayerShaderAsync(
        Window owner,
        string projectPath,
        string currentPath)
    {
        string root = Path.Combine(projectPath, "Assets", "Shaders");
        Directory.CreateDirectory(root);
        string? initialFilePath = GameAssetPath.TryResolveExistingFile(
            projectPath,
            currentPath,
            out string resolvedCurrent)
            ? resolvedCurrent
            : null;
        string? path = await ShowAsync(
            owner,
            root,
            FilesFilter("*.vert", "*.frag"),
            LocaleService.Get("SELECT_LAYER_SHADER"),
            initialFilePath: initialFilePath);
        if (path is null)
            return null;
        return GameAssetPath.TryFromProjectFile(projectPath, path, out string assetPath)
            ? assetPath
            : null;
    }

    public FileSelectorDialog(
        Window? owner,
        string root,
        string filterStr,
        string? title = null,
        bool save = false,
        string? initialDirectory = null,
        bool allowMultiple = false,
        string? initialFilePath = null)
    {
        _root = Path.GetFullPath(root);
        _save = save;
        _allowMultiple = allowMultiple && !save;
        EditorThumbnailService? projectThumbnails = findProjectThumbnails(owner);
        _thumbnails = projectThumbnails ?? new EditorThumbnailService();
        _ownsThumbnails = projectThumbnails is null;

        string[] parts = filterStr.Split(";;", StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        if (parts.Length == 0)
            parts = [filterStr];
        _filterNames = parts;
        _filterPatterns = parts.Select(ParsePatterns).ToList();

        _currentDirectory = _root;
        string? initialSelection = null;
        if (!_save
            && !_allowMultiple
            && !string.IsNullOrWhiteSpace(initialFilePath)
            && File.Exists(initialFilePath))
        {
            string fullInitialFilePath = Path.GetFullPath(initialFilePath);
            if (isWithinRoot(fullInitialFilePath) && matchesFilter(fullInitialFilePath))
            {
                initialSelection = fullInitialFilePath;
                _currentDirectory = Path.GetDirectoryName(fullInitialFilePath)!;
            }
        }
        if (initialSelection is null
            && !string.IsNullOrWhiteSpace(initialDirectory)
            && Directory.Exists(initialDirectory)
            && isWithinRoot(initialDirectory))
        {
            _currentDirectory = Path.GetFullPath(initialDirectory);
        }

        Title = title ?? LocaleService.Get("SELECT_FILE");
        Width = 940;
        Height = 620;
        MinWidth = 680;
        MinHeight = 440;
        Background = EditorTheme.Brush("Background");
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        CanResize = true;
        FontFamily = EditorTheme.FontFamily;
        EditorWindowIcon.Apply(this);

        _upButton = new Button
        {
            Content = EditorIconResources.CreateImage("EditorImage.NavigateUp", 16, 16),
            Classes = { "toolbar" },
            Width = 28,
            Height = 28,
            Padding = new Thickness(0),
            HorizontalContentAlignment = HorizontalAlignment.Center,
            VerticalContentAlignment = VerticalAlignment.Center,
        };
        ToolTip.SetTip(_upButton, LocaleService.Get("FILE_DIALOG_PARENT_FOLDER"));
        Avalonia.Automation.AutomationProperties.SetName(_upButton, LocaleService.Get("FILE_DIALOG_PARENT_FOLDER"));
        _upButton.Click += async (_, _) => await navigateUpAsync();

        TextBlock lookInLabel = new()
        {
            Text = LocaleService.Get("FILE_DIALOG_LOOK_IN"),
            VerticalAlignment = VerticalAlignment.Center,
        };

        _lookInBox = EditorInputs.CreateReadOnlyTextBox(_root);

        Grid topRow = new() { ColumnDefinitions = new ColumnDefinitions("Auto,Auto,*"), ColumnSpacing = 6 };
        topRow.Children.Add(_upButton);
        Grid.SetColumn(lookInLabel, 1);
        topRow.Children.Add(lookInLabel);
        Grid.SetColumn(_lookInBox, 2);
        topRow.Children.Add(_lookInBox);

        _fileGrid = new ListBox
        {
            Background = Brushes.Transparent,
            BorderThickness = new Thickness(0),
            SelectionMode = _allowMultiple ? SelectionMode.Multiple : SelectionMode.Single,
            ItemsPanel = new FuncTemplate<Panel?>(() => new VirtualizingTilePanel
            {
                ItemWidth = 162,
                ItemHeight = 132,
            }),
            ItemTemplate = new FuncDataTemplate<FileEntry>((entry, _) => entry is null ? null : createEntry(entry)),
        };
        ScrollViewer.SetHorizontalScrollBarVisibility(_fileGrid, ScrollBarVisibility.Disabled);
        ScrollViewer.SetVerticalScrollBarVisibility(_fileGrid, ScrollBarVisibility.Auto);
        Style itemStyle = new(selector => selector.OfType<ListBoxItem>());
        itemStyle.Setters.Add(new Setter(TemplatedControl.PaddingProperty, new Thickness(0)));
        itemStyle.Setters.Add(new Setter(Layoutable.MarginProperty, new Thickness(1)));
        itemStyle.Setters.Add(new Setter(ContentControl.HorizontalContentAlignmentProperty, HorizontalAlignment.Stretch));
        _fileGrid.Styles.Add(itemStyle);
        _fileGrid.SelectionChanged += onSelectionChanged;
        Border fileGridBorder = new()
        {
            Background = SurfaceBrush,
            BorderBrush = GridBorderBrush,
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(4),
            ClipToBounds = true,
            Child = _fileGrid,
        };

        _previewImage = new Image { Stretch = Stretch.Uniform, Margin = new Thickness(8) };
        _previewImageContainer = new Panel { Children = { _previewImage }, IsVisible = false };

        _previewTextBox = new TextBox
        {
            IsReadOnly = true,
            TextWrapping = TextWrapping.Wrap,
            AcceptsReturn = true,
            FontFamily = new FontFamily("Menlo, Monaco, Consolas, Monospace"),
            FontSize = 12,
            Background = Brushes.Transparent,
            BorderThickness = new Thickness(0),
            Padding = new Thickness(4),
        };
        ScrollViewer previewTextScroll = new()
        {
            Content = _previewTextBox,
            HorizontalScrollBarVisibility = ScrollBarVisibility.Auto,
            VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
        };
        _previewTextContainer = new Panel { Children = { previewTextScroll }, IsVisible = false };

        Panel previewStack = new() { Children = { _previewImageContainer, _previewTextContainer } };

        Border previewBorder = new()
        {
            Width = 260,
            Background = SurfaceBrush,
            BorderBrush = GridBorderBrush,
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(4),
            ClipToBounds = true,
            Child = previewStack,
        };

        Grid middleRow = new() { ColumnDefinitions = new ColumnDefinitions("*,8,Auto") };
        middleRow.Children.Add(fileGridBorder);
        Grid.SetColumn(previewBorder, 2);
        middleRow.Children.Add(previewBorder);

        _fileNameBox = save ? EditorInputs.CreateEditableTextBox() : EditorInputs.CreateReadOnlyTextBox();
        if (save)
            _fileNameBox.TextChanged += (_, _) => updateConfirmButton();

        TextBlock fileNameLabel = new()
        {
            Text = LocaleService.Get("FILE_NAME"),
            VerticalAlignment = VerticalAlignment.Center,
        };
        Grid fileNameRow = new() { ColumnDefinitions = new ColumnDefinitions("Auto,*"), ColumnSpacing = 6 };
        fileNameRow.Children.Add(fileNameLabel);
        Grid.SetColumn(_fileNameBox, 1);
        fileNameRow.Children.Add(_fileNameBox);

        _filterCombo = new ComboBox
        {
            ItemsSource = _filterNames,
            SelectedIndex = 0,
            HorizontalAlignment = HorizontalAlignment.Stretch,
        };
        _filterCombo.SelectionChanged += async (_, _) => await onFilterChangedAsync();

        TextBlock filterLabel = new()
        {
            Text = LocaleService.Get("FILE_DIALOG_FILE_TYPE"),
            VerticalAlignment = VerticalAlignment.Center,
        };

        _confirmButton = new Button
        {
            Content = save ? LocaleService.Get("SAVE") : LocaleService.Get("FILE_DIALOG_OPEN"),
            IsEnabled = false,
            Classes = { "accent" },
        };
        _confirmButton.Click += (_, _) => confirm();

        Button cancelButton = new() { Content = LocaleService.Get("CANCEL") };
        cancelButton.Click += (_, _) => Close(null);

        Grid bottomRow = new() { ColumnDefinitions = new ColumnDefinitions("Auto,*,Auto,Auto"), ColumnSpacing = 8 };
        bottomRow.Children.Add(filterLabel);
        Grid.SetColumn(_filterCombo, 1);
        bottomRow.Children.Add(_filterCombo);
        Grid.SetColumn(_confirmButton, 2);
        bottomRow.Children.Add(_confirmButton);
        Grid.SetColumn(cancelButton, 3);
        bottomRow.Children.Add(cancelButton);

        Grid layout = new()
        {
            RowDefinitions = new RowDefinitions("Auto,*,Auto,Auto"),
            RowSpacing = 8,
            Margin = new Thickness(10),
        };
        layout.Children.Add(topRow);
        Grid.SetRow(middleRow, 1);
        layout.Children.Add(middleRow);
        Grid.SetRow(fileNameRow, 2);
        layout.Children.Add(fileNameRow);
        Grid.SetRow(bottomRow, 3);
        layout.Children.Add(bottomRow);

        Content = layout;

        KeyDown += onKeyDown;
        Closed += (_, _) => disposeResources();
        Opened += async (_, _) =>
        {
            _opened = true;
            await refreshDirectoryAsync(initialSelection);
        };
    }

    private static EditorThumbnailService? findProjectThumbnails(Window? owner)
    {
        for (Window? window = owner; window is not null; window = window.Owner as Window)
        {
            if (window.DataContext is MainViewModel main)
                return main.GameData.Thumbnails;
        }
        return null;
    }

    private bool canGoUp =>
        !PathComparer.Equals(_currentDirectory, _root);

    private async Task navigateUpAsync()
    {
        if (!canGoUp)
            return;
        string? parent = Path.GetDirectoryName(_currentDirectory);
        if (parent is not null && isWithinRoot(parent))
        {
            _currentDirectory = parent;
            await refreshDirectoryAsync();
        }
    }

    private bool isWithinRoot(string path)
    {
        string full = Path.GetFullPath(path);
        string relative = Path.GetRelativePath(_root, full);
        return !relative.StartsWith("..", StringComparison.Ordinal) && !Path.IsPathRooted(relative);
    }

    private async Task refreshDirectoryAsync(string? initialFilePath = null)
    {
        if (_closed)
            return;
        _directoryCancellation.Cancel();
        _directoryCancellation.Dispose();
        _directoryCancellation = new CancellationTokenSource();
        CancellationToken token = _directoryCancellation.Token;
        _lookInBox.Text = _currentDirectory;
        _upButton.IsEnabled = canGoUp;
        _selectedPath = null;
        _selectedPaths.Clear();
        _fileName = string.Empty;
        if (!_save)
            _fileNameBox.Text = string.Empty;
        clearPreview();
        updateConfirmButton();
        _entries = [];
        _fileGrid.ItemsSource = _entries;
        string directory = _currentDirectory;
        string[] patterns = _filterIndex < _filterPatterns.Count
            ? _filterPatterns[_filterIndex].ToArray()
            : [];
        try
        {
            List<FileEntry> entries = await Task.Run(() => readDirectory(directory, patterns, token), token);
            if (_closed || token.IsCancellationRequested)
                return;
            _entries = entries;
            _fileGrid.ItemsSource = entries;
            if (initialFilePath is not null)
            {
                FileEntry? initial = entries.FirstOrDefault(entry =>
                    !entry.IsDirectory && PathComparer.Equals(entry.Path, initialFilePath));
                if (initial is not null)
                {
                    _fileGrid.SelectedItem = initial;
                    Dispatcher.UIThread.Post(() =>
                    {
                        if (!_closed && !token.IsCancellationRequested)
                            _fileGrid.ScrollIntoView(initial);
                    }, DispatcherPriority.Loaded);
                }
            }
        }
        catch (OperationCanceledException) when (token.IsCancellationRequested)
        {
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            if (!_closed && !token.IsCancellationRequested)
            {
                _previewTextBox.Text = exception.Message;
                _previewTextContainer.IsVisible = true;
            }
        }
    }

    private static List<FileEntry> readDirectory(string directory, string[] patterns, CancellationToken token)
    {
        List<FileEntry> entries = [];
        foreach (DirectoryInfo child in new DirectoryInfo(directory).EnumerateDirectories())
        {
            token.ThrowIfCancellationRequested();
            entries.Add(new FileEntry(child.FullName, true, 0));
        }
        foreach (FileInfo file in new DirectoryInfo(directory).EnumerateFiles())
        {
            token.ThrowIfCancellationRequested();
            if (!matchesFilter(file.FullName, patterns))
                continue;
            long size;
            try
            {
                size = file.Length;
            }
            catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
            {
                size = 0;
            }
            entries.Add(new FileEntry(file.FullName, false, size));
        }
        return entries.OrderBy(entry => !entry.IsDirectory)
            .ThenBy(entry => Path.GetFileName(entry.Path), StringComparer.OrdinalIgnoreCase)
            .ToList();
    }

    private bool matchesFilter(string filePath) => matchesFilter(filePath,
        _filterIndex < _filterPatterns.Count ? _filterPatterns[_filterIndex] : []);

    private static bool matchesFilter(string filePath, IReadOnlyList<string> patterns)
    {
        if (DataConfig.isAnimationCache(filePath))
            return false;
        if (patterns.Count == 0 || patterns.Contains("*") || patterns.Contains("*.*"))
            return true;
        string lower = Path.GetFileName(filePath).ToLowerInvariant();
        return patterns.Any(pattern =>
            pattern.StartsWith("*.", StringComparison.Ordinal)
                ? lower.EndsWith(pattern[1..], StringComparison.OrdinalIgnoreCase)
                : pattern == "*");
    }

    private async Task onFilterChangedAsync()
    {
        _filterIndex = Math.Max(0, _filterCombo.SelectedIndex);
        if (_opened)
            await refreshDirectoryAsync();
    }

    private Border createEntry(FileEntry entry)
    {
        string path = entry.Path;
        bool isDirectory = entry.IsDirectory;
        bool isImage = !isDirectory && ImageSuffixes.Contains(Path.GetExtension(path).TrimStart('.'));
        string name = Path.GetFileName(path);
        string detail = isDirectory ? string.Empty : formatFileSize(entry.Size);

        Border cell = new()
        {
            Width = 160,
            Height = 128,
            Padding = new Thickness(3),
            Background = Brushes.Transparent,
            BorderThickness = new Thickness(0),
            BorderBrush = Brushes.Transparent,
            CornerRadius = new CornerRadius(4),
            Cursor = new Cursor(StandardCursorType.Hand),
        };

        Grid inner = new() { RowDefinitions = new RowDefinitions("80,22,14") };

        Panel iconArea = new() { Margin = new Thickness(0, 0, 0, 2), HorizontalAlignment = HorizontalAlignment.Center };
        if (isDirectory)
            iconArea.Children.Add(EditorIconResources.CreateImage("EditorImage.Folder", 54, 44));
        else if (isImage)
            iconArea.Children.Add(createThumbnail(path));
        else
            iconArea.Children.Add(EditorIconResources.CreateImage("EditorImage.File", 40, 50));
        inner.Children.Add(iconArea);

        TextBlock nameText = new()
        {
            Text = name,
            Foreground = TextBrush,
            FontSize = 12,
            TextAlignment = TextAlignment.Center,
            TextTrimming = TextTrimming.CharacterEllipsis,
            VerticalAlignment = VerticalAlignment.Center,
            HorizontalAlignment = HorizontalAlignment.Stretch,
            Margin = new Thickness(4, 0),
        };
        Grid.SetRow(nameText, 1);
        inner.Children.Add(nameText);

        TextBlock detailText = new()
        {
            Text = detail,
            Foreground = DetailBrush,
            FontSize = 12,
            TextAlignment = TextAlignment.Center,
            VerticalAlignment = VerticalAlignment.Center,
            HorizontalAlignment = HorizontalAlignment.Stretch,
        };
        Grid.SetRow(detailText, 2);
        inner.Children.Add(detailText);

        cell.Child = inner;

        cell.DoubleTapped += async (_, args) =>
        {
            args.Handled = true;
            await activateEntryAsync(entry);
        };
        return cell;
    }

    private void onSelectionChanged(object? sender, SelectionChangedEventArgs args)
    {
        if (_closed || _updatingSelection)
            return;
        FileEntry? added = args.AddedItems.OfType<FileEntry>().LastOrDefault();
        if (added is { IsDirectory: true } && _fileGrid.SelectedItems is { Count: > 1 })
        {
            _updatingSelection = true;
            _fileGrid.SelectedItem = added;
            _updatingSelection = false;
        }
        List<FileEntry> selected = _fileGrid.SelectedItems?.OfType<FileEntry>()
            .Where(entry => !entry.IsDirectory).ToList() ?? [];
        _selectedPaths.Clear();
        _selectedPaths.AddRange(selected.Select(entry => entry.Path));
        _selectedPath = added is { IsDirectory: false } && selected.Contains(added)
            ? added.Path : selected.LastOrDefault()?.Path;
        _fileName = _selectedPath is null ? string.Empty : Path.GetFileName(_selectedPath);
        if (!_save)
            _fileNameBox.Text = _allowMultiple
                ? string.Join(", ", _selectedPaths.Select(Path.GetFileName))
                : _fileName;
        if (_selectedPath is null)
            clearPreview();
        else
            _ = updatePreviewAsync(_selectedPath);
        updateConfirmButton();
    }

    private async Task activateEntryAsync(FileEntry entry)
    {
        if (entry.IsDirectory)
        {
            if (isWithinRoot(entry.Path))
            {
                _currentDirectory = entry.Path;
                await refreshDirectoryAsync();
            }
            return;
        }
        if (!_selectedPaths.Contains(entry.Path, PathComparer))
            _fileGrid.SelectedItem = entry;
        if (!_save)
            confirm();
    }

    private void confirm()
    {
        if (_allowMultiple)
        {
            string[] paths = _selectedPaths.Select(Path.GetFullPath).ToArray();
            if (paths.Length == 0 || paths.Any(path => !isWithinRoot(path) || !File.Exists(path)))
                return;
            Close(paths);
            return;
        }
        string? resolved = _save ? buildSavePath() : _selectedPath;
        if (resolved is null)
            return;
        string full = Path.GetFullPath(resolved);
        if (!isWithinRoot(full))
            return;
        if (!_save && !File.Exists(full))
            return;
        _selectedPath = full;
        Close(full);
    }

    private string? buildSavePath()
    {
        string text = (_fileNameBox.Text ?? string.Empty).Trim();
        if (string.IsNullOrEmpty(text))
            return null;
        return Path.IsPathRooted(text) ? text : Path.Combine(_currentDirectory, text);
    }

    private void updateConfirmButton()
    {
        _confirmButton.IsEnabled = _save
            ? !string.IsNullOrWhiteSpace(_fileNameBox.Text)
            : _allowMultiple ? _selectedPaths.Count > 0 : _selectedPath is not null;
    }

    private void clearPreview()
    {
        _previewCancellation.Cancel();
        _previewCancellation.Dispose();
        _previewCancellation = new CancellationTokenSource();
        _previewImageContainer.IsVisible = false;
        _previewTextContainer.IsVisible = false;
        _previewImage.Source = null;
        _previewLease?.Dispose();
        _previewLease = null;
        _previewTextBox.Text = string.Empty;
    }

    private async Task updatePreviewAsync(string path)
    {
        clearPreview();
        CancellationToken token = _previewCancellation.Token;
        string extension = Path.GetExtension(path).TrimStart('.');
        try
        {
            if (ImageSuffixes.Contains(extension))
            {
                EditorThumbnailLease? lease = await _thumbnails.AcquireAsync(path, 520, token);
                if (_closed || token.IsCancellationRequested)
                {
                    lease?.Dispose();
                    return;
                }
                _previewLease = lease;
                _previewImage.Source = lease?.Bitmap;
                _previewImageContainer.IsVisible = lease is not null;
            }
            else if (TextSuffixes.Contains(extension))
            {
                string? text = await readTextPreviewAsync(path, token);
                if (!_closed && !token.IsCancellationRequested && text is not null)
                {
                    _previewTextBox.Text = text;
                    _previewTextContainer.IsVisible = true;
                }
            }
        }
        catch (OperationCanceledException) when (token.IsCancellationRequested)
        {
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            if (!_closed && !token.IsCancellationRequested)
            {
                _previewTextBox.Text = exception.Message;
                _previewTextContainer.IsVisible = true;
            }
        }
    }

    private static Task<string?> readTextPreviewAsync(string path, CancellationToken token) => Task.Run(async () =>
    {
        await using FileStream stream = new(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite,
            8192, FileOptions.Asynchronous | FileOptions.SequentialScan);
        byte[] buffer = new byte[Math.Min(stream.Length, MaxTextPreviewBytes)];
        int read = await stream.ReadAtLeastAsync(buffer, buffer.Length, false, token);
        if (Array.IndexOf(buffer, (byte)0, 0, Math.Min(read, 8192)) >= 0)
            return null;
        string text = Encoding.UTF8.GetString(buffer.AsSpan(0, read));
        return stream.Length > MaxTextPreviewBytes ? text + "\n\n..." : text;
    }, token);

    private Control createThumbnail(string path)
    {
        Image image = new()
        {
            Source = EditorIconResources.GetImage("EditorImage.File"),
            Stretch = Stretch.Uniform,
            Margin = new Thickness(3),
            HorizontalAlignment = HorizontalAlignment.Center,
            VerticalAlignment = VerticalAlignment.Center,
            Width = 80,
            Height = 80,
        };
        CancellationTokenSource? cancellation = null;
        EditorThumbnailLease? activeLease = null;
        image.AttachedToVisualTree += async (_, _) =>
        {
            if (_closed)
                return;
            cancellation?.Cancel();
            cancellation?.Dispose();
            CancellationTokenSource request = CancellationTokenSource.CreateLinkedTokenSource(_directoryCancellation.Token);
            cancellation = request;
            try
            {
                EditorThumbnailLease? lease = await _thumbnails.AcquireAsync(path, 160, request.Token);
                if (_closed || request.IsCancellationRequested || !ReferenceEquals(cancellation, request))
                {
                    lease?.Dispose();
                    return;
                }
                activeLease?.Dispose();
                activeLease = lease;
                image.Source = lease?.Bitmap ?? EditorIconResources.GetImage("EditorImage.File");
            }
            catch (OperationCanceledException) when (request.IsCancellationRequested)
            {
            }
        };
        image.DetachedFromVisualTree += (_, _) =>
        {
            cancellation?.Cancel();
            cancellation?.Dispose();
            cancellation = null;
            image.Source = null;
            activeLease?.Dispose();
            activeLease = null;
        };
        return image;
    }

    private static string formatFileSize(long bytes)
    {
        if (bytes < 1024)
            return $"{bytes} B";
        if (bytes < 1024 * 1024)
            return $"{bytes / 1024.0:F1} KB";
        return $"{bytes / (1024.0 * 1024):F1} MB";
    }

    private static List<string> ParsePatterns(string filterText)
    {
        MatchCollection matches = Regex.Matches(filterText, @"\(([^)]*)\)");
        List<string> patterns = [];
        foreach (Match match in matches)
        {
            foreach (string part in match.Groups[1].Value.Split(' ', StringSplitOptions.RemoveEmptyEntries))
            {
                if (part.Contains('*') || part.Contains('?'))
                    patterns.Add(part.ToLowerInvariant());
            }
        }
        return patterns.Count > 0 ? patterns : ["*"];
    }

    private async void onKeyDown(object? sender, KeyEventArgs e)
    {
        switch (e.Key)
        {
            case Key.Escape:
                Close(null);
                e.Handled = true;
                break;
            case Key.Enter when _fileGrid.IsKeyboardFocusWithin && _fileGrid.SelectedItem is FileEntry { IsDirectory: true } entry:
                e.Handled = true;
                await activateEntryAsync(entry);
                break;
            case Key.Enter when _confirmButton.IsEnabled:
                confirm();
                e.Handled = true;
                break;
        }
    }

    private void disposeResources()
    {
        _closed = true;
        _directoryCancellation.Cancel();
        _directoryCancellation.Dispose();
        clearPreview();
        _previewCancellation.Dispose();
        _fileGrid.ItemsSource = null;
        if (_ownsThumbnails)
            _thumbnails.Dispose();
    }

    private sealed record FileEntry(string Path, bool IsDirectory, long Size);
}
