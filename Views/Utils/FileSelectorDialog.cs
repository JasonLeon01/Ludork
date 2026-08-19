using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

public sealed class FileSelectorDialog : Window
{
    private static readonly HashSet<string> ImageSuffixes = new(StringComparer.OrdinalIgnoreCase)
        { "png", "jpg", "jpeg", "bmp", "gif", "webp" };

    private static readonly HashSet<string> TextSuffixes = new(StringComparer.OrdinalIgnoreCase)
        { "txt", "json", "py", "md", "ini", "xml", "csv", "log", "yaml", "yml",
          "html", "htm", "css", "qss", "bat", "sh", "toml", "cfg", "conf", "vert", "frag", "lua" };

    private const int MaxTextPreviewBytes = 256 * 1024;

    private static readonly SolidColorBrush SurfaceBrush = new(Color.Parse("#1c1c1c"));
    private static readonly SolidColorBrush GridBorderBrush = new(Color.Parse("#3a3a3a"));
    private static readonly SolidColorBrush AccentBrush = new(Color.Parse("#ffd740"));
    private static readonly SolidColorBrush AccentMutedBrush = new(Color.Parse("#33ffd740"));
    private static readonly SolidColorBrush HoverBrush = new(Color.Parse("#2a2a2a"));
    private static readonly SolidColorBrush TextBrush = new(Color.Parse("#ffffff"));
    private static readonly SolidColorBrush DetailBrush = new(Color.Parse("#888888"));

    private readonly string _root;
    private readonly bool _save;
    private readonly bool _allowMultiple;
    private readonly List<List<string>> _filterPatterns;
    private readonly string[] _filterNames;
    private int _filterIndex;

    private string _currentDirectory;
    private string? _selectedPath;
    private readonly List<string> _selectedPaths = [];
    private string? _selectionAnchorPath;
    private string _fileName = string.Empty;
    private Bitmap? _previewBitmap;
    private readonly List<Bitmap> _thumbnailBitmaps = [];

    private readonly TextBox _lookInBox;
    private readonly Button _upButton;
    private readonly WrapPanel _fileGrid;
    private readonly Image _previewImage;
    private readonly Panel _previewImageContainer;
    private readonly TextBox _previewTextBox;
    private readonly Panel _previewTextContainer;
    private readonly TextBox _fileNameBox;
    private readonly ComboBox _filterCombo;
    private readonly Button _confirmButton;
    private Border? _initialFileCell;

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
        string? initialFilePath = string.IsNullOrWhiteSpace(currentPath)
            ? null
            : Path.Combine(root, currentPath);
        string? path = await ShowAsync(
            owner,
            root,
            FilesFilter("*.vert", "*.frag"),
            LocaleService.Get("SELECT_LAYER_SHADER"),
            initialFilePath: initialFilePath);
        if (path is null)
            return null;
        return Path.GetRelativePath(root, path).Replace('\\', '/');
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
        Background = new SolidColorBrush(Color.Parse("#121212"));
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        CanResize = true;
        FontFamily = new FontFamily("avares://Ludork/Assets/HarmonyOS_Sans_SC_Regular.ttf#HarmonyOS Sans SC");
        EditorWindowIcon.Apply(this);

        _upButton = new Button
        {
            Content = new PathIcon
            {
                Data = EditorIconResources.GetGeometry("EditorIcon.NavigateUp"),
                Width = 18,
                Height = 18,
                Foreground = AccentBrush,
            },
            Width = 38,
            Height = 34,
            Padding = new Thickness(0),
            HorizontalContentAlignment = HorizontalAlignment.Center,
            VerticalContentAlignment = VerticalAlignment.Center,
        };
        _upButton.Click += (_, _) => navigateUp();

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

        _fileGrid = new WrapPanel { Orientation = Orientation.Horizontal };
        ScrollViewer gridScrollViewer = new()
        {
            Content = _fileGrid,
            HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled,
            VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
        };
        Border fileGridBorder = new()
        {
            Background = SurfaceBrush,
            BorderBrush = GridBorderBrush,
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(3),
            ClipToBounds = true,
            Child = gridScrollViewer,
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
            CornerRadius = new CornerRadius(3),
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
        _filterCombo.SelectionChanged += (_, _) => onFilterChanged();

        TextBlock filterLabel = new()
        {
            Text = LocaleService.Get("FILE_DIALOG_FILE_TYPE"),
            VerticalAlignment = VerticalAlignment.Center,
        };

        _confirmButton = new Button
        {
            Content = save ? LocaleService.Get("SAVE") : LocaleService.Get("FILE_DIALOG_OPEN"),
            IsEnabled = false,
            Classes = { "Raised" },
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
        Closed += (_, _) => disposeAllBitmaps();
        Opened += (_, _) =>
        {
            Border? initialFileCell = _initialFileCell;
            _initialFileCell = null;
            if (initialFileCell is not null)
            {
                Dispatcher.UIThread.Post(
                    initialFileCell.BringIntoView,
                    DispatcherPriority.Loaded);
            }
        };

        refreshDirectory(initialSelection);
    }

    private bool canGoUp =>
        !string.Equals(_currentDirectory, _root, StringComparison.OrdinalIgnoreCase);

    private void navigateUp()
    {
        if (!canGoUp)
            return;
        string? parent = Path.GetDirectoryName(_currentDirectory);
        if (parent is not null && isWithinRoot(parent))
        {
            _currentDirectory = parent;
            refreshDirectory();
        }
    }

    private bool isWithinRoot(string path)
    {
        string full = Path.GetFullPath(path);
        string relative = Path.GetRelativePath(_root, full);
        return !relative.StartsWith("..", StringComparison.Ordinal) && !Path.IsPathRooted(relative);
    }

    private void refreshDirectory(string? initialFilePath = null)
    {
        _lookInBox.Text = _currentDirectory;
        _upButton.IsEnabled = canGoUp;
        _selectedPath = null;
        _selectedPaths.Clear();
        _selectionAnchorPath = null;
        _fileName = string.Empty;
        if (_allowMultiple)
            _fileNameBox.Text = string.Empty;
        clearPreview();
        updateConfirmButton();

        disposeThumbnails();
        _fileGrid.Children.Clear();

        List<string> dirs =
        [
            .. Directory.GetDirectories(_currentDirectory)
                .OrderBy(Path.GetFileName, StringComparer.OrdinalIgnoreCase),
        ];
        List<string> files =
        [
            .. Directory.GetFiles(_currentDirectory)
                .Where(matchesFilter)
                .OrderBy(Path.GetFileName, StringComparer.OrdinalIgnoreCase),
        ];

        foreach (string dir in dirs)
            _fileGrid.Children.Add(createEntry(dir, isDirectory: true));
        foreach (string file in files)
        {
            Border entry = createEntry(file, isDirectory: false);
            _fileGrid.Children.Add(entry);
            if (string.Equals(file, initialFilePath, StringComparison.OrdinalIgnoreCase))
                _initialFileCell = entry;
        }
        if (_initialFileCell is not null)
            selectEntry(initialFilePath!, isDirectory: false, _initialFileCell, KeyModifiers.None);
    }

    private bool matchesFilter(string filePath)
    {
        if (DataConfig.isAnimationCache(filePath))
            return false;
        List<string> patterns = _filterIndex < _filterPatterns.Count
            ? _filterPatterns[_filterIndex]
            : [];
        if (patterns.Count == 0 || patterns.Contains("*") || patterns.Contains("*.*"))
            return true;
        string lower = Path.GetFileName(filePath).ToLowerInvariant();
        return patterns.Any(pattern =>
            pattern.StartsWith("*.", StringComparison.Ordinal)
                ? lower.EndsWith(pattern[1..], StringComparison.OrdinalIgnoreCase)
                : pattern == "*");
    }

    private void onFilterChanged()
    {
        _filterIndex = Math.Max(0, _filterCombo.SelectedIndex);
        refreshDirectory();
    }

    private Border createEntry(string path, bool isDirectory)
    {
        bool isImage = !isDirectory && ImageSuffixes.Contains(Path.GetExtension(path).TrimStart('.'));
        string name = Path.GetFileName(path);
        string detail = isDirectory ? string.Empty : formatFileSize(new FileInfo(path).Length);

        Border cell = new()
        {
            Width = 160,
            Height = 114,
            Padding = new Thickness(3),
            Background = Brushes.Transparent,
            BorderThickness = new Thickness(2),
            BorderBrush = Brushes.Transparent,
            CornerRadius = new CornerRadius(3),
            Tag = path,
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
            FontSize = 10,
            TextAlignment = TextAlignment.Center,
            VerticalAlignment = VerticalAlignment.Center,
            HorizontalAlignment = HorizontalAlignment.Stretch,
        };
        Grid.SetRow(detailText, 2);
        inner.Children.Add(detailText);

        cell.Child = inner;

        cell.PointerEntered += (_, _) =>
        {
            if (!isSelectedEntry(path))
                cell.Background = HoverBrush;
        };
        cell.PointerExited += (_, _) =>
        {
            if (!isSelectedEntry(path))
                resetCellStyle(cell);
        };
        cell.PointerPressed += (_, args) =>
        {
            if (args.GetCurrentPoint(cell).Properties.IsLeftButtonPressed)
                selectEntry(path, isDirectory, cell, args.KeyModifiers);
        };
        cell.DoubleTapped += (_, _) => activateEntry(path, isDirectory);

        return cell;
    }

    private bool isSelectedEntry(string path) =>
        _allowMultiple
            ? _selectedPaths.Contains(path, StringComparer.OrdinalIgnoreCase)
            : string.Equals(_selectedPath, path, StringComparison.OrdinalIgnoreCase);

    private static void resetCellStyle(Border cell)
    {
        cell.Background = Brushes.Transparent;
        cell.BorderBrush = Brushes.Transparent;
    }

    private static void applySelectedStyle(Border cell)
    {
        cell.Background = AccentMutedBrush;
        cell.BorderBrush = AccentBrush;
    }

    private void deselectAll()
    {
        foreach (Control control in _fileGrid.Children)
        {
            if (control is Border border)
                resetCellStyle(border);
        }
    }

    private void selectEntry(string path, bool isDirectory, Border clicked, KeyModifiers modifiers)
    {
        if (isDirectory)
        {
            deselectAll();
            applySelectedStyle(clicked);
            _selectedPath = null;
            _selectedPaths.Clear();
            _selectionAnchorPath = null;
            _fileName = string.Empty;
            if (_allowMultiple)
                _fileNameBox.Text = string.Empty;
            clearPreview();
        }
        else if (_allowMultiple)
        {
            bool primary = EditorShortcuts.HasPrimaryModifier(modifiers);
            bool shift = modifiers.HasFlag(KeyModifiers.Shift);
            if (shift && _selectionAnchorPath is not null)
                selectRange(_selectionAnchorPath, path, primary);
            else if (primary)
                toggleSelection(path);
            else
            {
                _selectedPaths.Clear();
                _selectedPaths.Add(path);
            }
            if (!shift || _selectionAnchorPath is null)
                _selectionAnchorPath = path;
            refreshSelectionStyles();
            updateActiveSelection(path);
        }
        else
        {
            deselectAll();
            applySelectedStyle(clicked);
            _selectedPath = path;
            _fileName = Path.GetFileName(path);
            if (!_save)
                _fileNameBox.Text = _fileName;
            updatePreview(path);
        }
        updateConfirmButton();
    }

    private void selectRange(string startPath, string endPath, bool additive)
    {
        List<string> files = _fileGrid.Children
            .OfType<Border>()
            .Select(control => control.Tag as string)
            .Where(path => path is not null && File.Exists(path))
            .Cast<string>()
            .ToList();
        int startIndex = files.FindIndex(path => string.Equals(path, startPath, StringComparison.OrdinalIgnoreCase));
        int endIndex = files.FindIndex(path => string.Equals(path, endPath, StringComparison.OrdinalIgnoreCase));
        if (startIndex < 0 || endIndex < 0)
        {
            if (!additive)
                _selectedPaths.Clear();
            addSelectedPath(endPath);
            return;
        }
        if (!additive)
            _selectedPaths.Clear();
        int first = Math.Min(startIndex, endIndex);
        int last = Math.Max(startIndex, endIndex);
        for (int index = first; index <= last; index += 1)
            addSelectedPath(files[index]);
    }

    private void toggleSelection(string path)
    {
        int index = _selectedPaths.FindIndex(selected =>
            string.Equals(selected, path, StringComparison.OrdinalIgnoreCase));
        if (index >= 0)
            _selectedPaths.RemoveAt(index);
        else
            _selectedPaths.Add(path);
    }

    private void addSelectedPath(string path)
    {
        if (!_selectedPaths.Contains(path, StringComparer.OrdinalIgnoreCase))
            _selectedPaths.Add(path);
    }

    private void refreshSelectionStyles()
    {
        foreach (Control control in _fileGrid.Children)
        {
            if (control is not Border border || border.Tag is not string path)
                continue;
            if (_selectedPaths.Contains(path, StringComparer.OrdinalIgnoreCase))
                applySelectedStyle(border);
            else
                resetCellStyle(border);
        }
    }

    private void updateActiveSelection(string requestedPath)
    {
        _selectedPath = _selectedPaths.Contains(requestedPath, StringComparer.OrdinalIgnoreCase)
            ? requestedPath
            : _selectedPaths.LastOrDefault();
        _fileName = _selectedPath is null ? string.Empty : Path.GetFileName(_selectedPath);
        _fileNameBox.Text = string.Join(", ", _selectedPaths.Select(Path.GetFileName));
        if (_selectedPath is null)
            clearPreview();
        else
            updatePreview(_selectedPath);
    }

    private void activateEntry(string path, bool isDirectory)
    {
        if (isDirectory)
        {
            if (isWithinRoot(path))
            {
                _currentDirectory = Path.GetFullPath(path);
                refreshDirectory();
            }
            return;
        }
        if (_allowMultiple)
        {
            if (!_selectedPaths.Contains(path, StringComparer.OrdinalIgnoreCase))
            {
                _selectedPaths.Clear();
                _selectedPaths.Add(path);
                refreshSelectionStyles();
            }
            updateActiveSelection(path);
            updateConfirmButton();
            confirm();
            return;
        }
        _selectedPath = path;
        _fileName = Path.GetFileName(path);
        if (!_save)
        {
            _fileNameBox.Text = _fileName;
            confirm();
        }
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
        _previewImageContainer.IsVisible = false;
        _previewTextContainer.IsVisible = false;

        _previewImage.Source = null;
        _previewBitmap?.Dispose();
        _previewBitmap = null;
        _previewTextBox.Text = string.Empty;
    }

    private void updatePreview(string path)
    {
        clearPreview();
        string ext = Path.GetExtension(path).TrimStart('.').ToLowerInvariant();

        if (ImageSuffixes.Contains(ext))
            showImagePreview(path);
        else if (TextSuffixes.Contains(ext))
            showTextPreview(path);
    }

    private void showImagePreview(string path)
    {
        _previewBitmap = new Bitmap(path);
        _previewImage.Source = _previewBitmap;
        _previewImageContainer.IsVisible = true;
    }

    private void showTextPreview(string path)
    {
        long size = new FileInfo(path).Length;
        int readSize = (int)Math.Min(size, MaxTextPreviewBytes);
        using FileStream stream = File.OpenRead(path);
        byte[] buffer = new byte[readSize];
        int read = stream.Read(buffer, 0, readSize);

        int checkLen = Math.Min(read, 8192);
        for (int i = 0; i < checkLen; i++)
        {
            if (buffer[i] == 0)
                return;
        }

        string text = Encoding.UTF8.GetString(buffer.AsSpan(0, read));
        if (size > MaxTextPreviewBytes)
            text += "\n\n...";

        _previewTextBox.Text = text;
        _previewTextContainer.IsVisible = true;
    }

    private Control createThumbnail(string path)
    {
        Bitmap bitmap = new(path);
        _thumbnailBitmaps.Add(bitmap);
        return new Image
        {
            Source = bitmap,
            Stretch = Stretch.Uniform,
            Margin = new Thickness(3),
            HorizontalAlignment = HorizontalAlignment.Center,
            VerticalAlignment = VerticalAlignment.Center,
            MaxWidth = 80,
            MaxHeight = 80,
        };
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

    private void onKeyDown(object? sender, KeyEventArgs e)
    {
        switch (e.Key)
        {
            case Key.Escape:
                Close(null);
                e.Handled = true;
                break;
            case Key.Enter when _confirmButton.IsEnabled:
                confirm();
                e.Handled = true;
                break;
        }
    }

    private void disposeThumbnails()
    {
        foreach (Bitmap bitmap in _thumbnailBitmaps)
            bitmap.Dispose();
        _thumbnailBitmaps.Clear();
    }

    private void disposeAllBitmaps()
    {
        disposeThumbnails();
        _previewBitmap?.Dispose();
        _previewBitmap = null;
    }
}
