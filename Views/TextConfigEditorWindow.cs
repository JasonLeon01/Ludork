using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Documents;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Controls;
using Ludork.Services;
using Ludork.Views.Utils;
using SkiaSharp;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

namespace Ludork.Views;

public sealed class TextConfigEditorWindow : Window
{
    private readonly GameDataService gameData;
    private readonly ProjectSaveService projectSave;
    private readonly string key;
    private readonly StackPanel inspector = new() { Spacing = 10 };
    private readonly TextConfigPreview preview;
    private readonly TextBox previewText = EditorInputs.CreateEditableTextBox();
    private readonly TextBlock validationText = new()
    {
        Foreground = new SolidColorBrush(Color.Parse("#ff7777")),
        TextWrapping = TextWrapping.Wrap,
    };
    private readonly Toast toast;
    private JsonObject data;
    private bool syncing;

    public TextConfigEditorWindow(
        GameDataService gameData,
        ProjectSaveService projectSave,
        string key,
        JsonObject data)
    {
        this.gameData = gameData;
        this.projectSave = projectSave;
        this.key = key;
        this.data = (JsonObject)data.DeepClone();
        normalizeData();
        Title = $"{LocaleService.Get("TEXT_CONFIG_EDITOR")} - {key}";
        Width = 1240;
        Height = 820;
        MinWidth = 960;
        MinHeight = 640;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = new SolidColorBrush(Color.Parse("#121212"));
        EditorWindowIcon.Apply(this);
        toast = new Toast(this);
        preview = new TextConfigPreview(gameData);
        previewText.AcceptsReturn = true;
        previewText.TextWrapping = TextWrapping.Wrap;
        previewText.Height = 96;
        previewText.MinHeight = 96;
        previewText.Text = isRich()
            ? "Rich text preview\nHorizontal and vertical gradients"
            : "Damage 128\nText effect preview";
        previewText.TextChanged += (_, _) => refreshPreview();
        Content = buildLayout();
        rebuildInspector();
        refreshPreview();
        AddHandler(KeyDownEvent, onKeyDown, RoutingStrategies.Tunnel);
        gameData.DataChanged += onDataChanged;
        gameData.DataRestored += onDataRestored;
        Closed += (_, _) =>
        {
            gameData.DataChanged -= onDataChanged;
            gameData.DataRestored -= onDataRestored;
        };
    }

    public void Reload(JsonObject nextData)
    {
        data = (JsonObject)nextData.DeepClone();
        normalizeData();
        rebuildInspector();
        refreshPreview();
    }

    private Control buildLayout()
    {
        Border inspectorBorder = new()
        {
            Background = new SolidColorBrush(Color.Parse("#1c1c1c")),
            BorderBrush = new SolidColorBrush(Color.Parse("#3a3a3a")),
            BorderThickness = new Thickness(1),
            Child = new ScrollViewer
            {
                Content = inspector,
                HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled,
                VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
            },
        };
        TextBlock previewTitle = new()
        {
            Text = LocaleService.Get("TEXT_CONFIG_PREVIEW"),
            FontSize = 18,
            FontWeight = FontWeight.Bold,
        };
        StackPanel previewHeader = new() { Spacing = 6 };
        previewHeader.Children.Add(previewTitle);
        previewHeader.Children.Add(new TextBlock { Text = LocaleService.Get("TEXT_CONFIG_PREVIEW_TEXT") });
        previewHeader.Children.Add(previewText);
        previewHeader.Children.Add(validationText);
        Grid previewArea = new()
        {
            RowDefinitions = new RowDefinitions("Auto,*"),
            RowSpacing = 12,
            Margin = new Thickness(16),
        };
        previewArea.Children.Add(previewHeader);
        Grid.SetRow(preview, 1);
        previewArea.Children.Add(preview);
        Grid root = new()
        {
            ColumnDefinitions = new ColumnDefinitions("430,5,*"),
        };
        root.Children.Add(inspectorBorder);
        GridSplitter splitter = new()
        {
            Background = new SolidColorBrush(Color.Parse("#363636")),
            ResizeDirection = GridResizeDirection.Columns,
        };
        Grid.SetColumn(splitter, 1);
        root.Children.Add(splitter);
        Grid.SetColumn(previewArea, 2);
        root.Children.Add(previewArea);
        return root;
    }

    private void rebuildInspector()
    {
        syncing = true;
        inspector.Children.Clear();
        inspector.Margin = new Thickness(14);
        addSection(LocaleService.Get("TEXT_CONFIG_GENERAL"));
        addStringField(LocaleService.Get("TEXT_CONFIG_NAME"), data, "name");
        addReferenceField(LocaleService.Get("TEXT_CONFIG_FONT"), data, "font", TextConfigReferenceKind.Font);
        addChoiceField(
            LocaleService.Get("TEXT_CONFIG_LINE_ALIGNMENT"),
            data,
            "lineAlignment",
            ["default", "left", "center", "right"]);
        if (isRich())
        {
            JsonObject defaultStyle = ensureObject(data, "defaultStyle");
            addSection(LocaleService.Get("TEXT_CONFIG_DEFAULT_STYLE"));
            addStyleFields(defaultStyle, false);
            addNamedStyleEditor();
        }
        else
        {
            addStyleFields(data, true);
        }
        addGlowFields();
        addGradientFields();
        syncing = false;
        updateValidation();
    }

    private void addStyleFields(JsonObject target, bool allowSlantAngle)
    {
        addNumberField(
            LocaleService.Get("TEXT_CONFIG_CHARACTER_SIZE"),
            target,
            "characterSize",
            1,
            512,
            1);
        JsonObject style = ensureObject(target, "style");
        addCheckField(LocaleService.Get("TEXT_CONFIG_BOLD"), style, "bold");
        CheckBox italic = addCheckField(
            LocaleService.Get("TEXT_CONFIG_ITALIC"),
            style,
            "italic");
        if (allowSlantAngle)
        {
            NumericUpDown slantAngle = addNumberField(
                LocaleService.Get("TEXT_CONFIG_SLANT_ANGLE"),
                target,
                "slantAngle",
                -45,
                45,
                1);
            slantAngle.IsEnabled = italic.IsChecked != true;
            italic.IsCheckedChanged += (_, _) =>
                slantAngle.IsEnabled = italic.IsChecked != true;
        }
        addCheckField(LocaleService.Get("TEXT_CONFIG_UNDERLINED"), style, "underlined");
        addCheckField(LocaleService.Get("TEXT_CONFIG_STRIKE_THROUGH"), style, "strikeThrough");
        addColourField(LocaleService.Get("TEXT_CONFIG_FILL_COLOR"), target, "fillColor", Colors.White);
        addNumberField(
            LocaleService.Get("TEXT_CONFIG_LETTER_SPACING"),
            target,
            "letterSpacing",
            0.1,
            10,
            0.1);
        addNumberField(
            LocaleService.Get("TEXT_CONFIG_LINE_SPACING"),
            target,
            "lineSpacing",
            0.1,
            10,
            0.1);
        JsonObject outline = ensureObject(target, "outline");
        addSubsection(LocaleService.Get("TEXT_CONFIG_OUTLINE"));
        addColourField(LocaleService.Get("TEXT_CONFIG_OUTLINE_COLOR"), outline, "color", Colors.Black);
        addNumberField(
            LocaleService.Get("TEXT_CONFIG_OUTLINE_THICKNESS"),
            outline,
            "thickness",
            0,
            32,
            0.25);
    }

    private void addGlowFields()
    {
        JsonObject glow = ensureObject(data, "glow");
        addSection(LocaleService.Get("TEXT_CONFIG_GLOW"));
        addCheckField(LocaleService.Get("TEXT_CONFIG_ENABLED"), glow, "enabled");
        addColourField(LocaleService.Get("TEXT_CONFIG_COLOR"), glow, "color", Colors.Transparent);
        addNumberField(LocaleService.Get("TEXT_CONFIG_RADIUS"), glow, "radius", 0, 64, 0.25);
        addNumberField(LocaleService.Get("TEXT_CONFIG_INTENSITY"), glow, "intensity", 0, 1, 0.05);
    }

    private void addGradientFields()
    {
        JsonObject gradient = ensureObject(data, "gradient");
        addSection(LocaleService.Get("TEXT_CONFIG_GRADIENT"));
        CheckBox enabled = addCheckField(
            LocaleService.Get("TEXT_CONFIG_ENABLED"),
            gradient,
            "enabled");
        int fieldsStartIndex = inspector.Children.Count;
        addChoiceField(
            LocaleService.Get("TEXT_CONFIG_DIRECTION"),
            gradient,
            "direction",
            ["vertical", "horizontal"]);
        addReferenceField(LocaleService.Get("TEXT_CONFIG_CURVE"), gradient, "curve", TextConfigReferenceKind.Curve);
        List<Control> fields = inspector.Children
            .Skip(fieldsStartIndex)
            .ToList();
        Action updateEnabled = () =>
        {
            foreach (Control field in fields)
                field.IsEnabled = enabled.IsChecked == true;
        };
        enabled.IsCheckedChanged += (_, _) => updateEnabled();
        updateEnabled();
    }

    private void addNamedStyleEditor()
    {
        addSection(LocaleService.Get("TEXT_CONFIG_NAMED_STYLES"));
        JsonArray styleOrder = ensureArray(data, "styleOrder");
        JsonObject styles = ensureObject(data, "styles");
        ListBox styleList = new()
        {
            Height = 130,
            ItemsSource = styleOrder.Select(node => stringValue(node)).Where(name => name.Length != 0).ToArray(),
        };
        ContentControl detailHost = new();
        Button add = new() { Content = LocaleService.Get("TEXT_CONFIG_ADD_STYLE") };
        Button rename = new() { Content = LocaleService.Get("TEXT_CONFIG_RENAME_STYLE") };
        Button moveUp = new() { Content = "↑" };
        Button moveDown = new() { Content = "↓" };
        Button delete = new() { Content = LocaleService.Get("DELETE") };
        WrapPanel buttons = new() { Orientation = Orientation.Horizontal };
        buttons.Children.Add(add);
        buttons.Children.Add(rename);
        buttons.Children.Add(moveUp);
        buttons.Children.Add(moveDown);
        buttons.Children.Add(delete);
        StackPanel container = new() { Spacing = 8 };
        container.Children.Add(styleList);
        container.Children.Add(buttons);
        container.Children.Add(detailHost);
        inspector.Children.Add(container);

        Action refreshSelection = () =>
        {
            string? selected = styleList.SelectedItem as string;
            rename.IsEnabled = selected is not null;
            moveUp.IsEnabled = selected is not null && styleList.SelectedIndex > 0;
            moveDown.IsEnabled = selected is not null && styleList.SelectedIndex >= 0
                && styleList.SelectedIndex < styleList.ItemCount - 1;
            delete.IsEnabled = selected is not null;
            detailHost.Content = selected is not null && styles[selected] is JsonObject selectedStyle
                ? buildOptionalStyleInspector(selectedStyle)
                : null;
        };
        styleList.SelectionChanged += (_, _) => refreshSelection();
        add.Click += async (_, _) =>
        {
            string? name = await SingleRowDialog.ShowAsync(
                this,
                LocaleService.Get("TEXT_CONFIG_ADD_STYLE"),
                LocaleService.Get("TEXT_CONFIG_STYLE_NAME"),
                styleOrder.Select(node => stringValue(node)));
            string normalized = name?.Trim() ?? string.Empty;
            if (!isValidStyleName(normalized, styles, null))
                return;
            styles[normalized] = new JsonObject();
            styleOrder.Add(normalized);
            applyChanges();
            rebuildInspector();
        };
        rename.Click += async (_, _) =>
        {
            if (styleList.SelectedItem is not string current)
                return;
            string? name = await SingleRowDialog.ShowAsync(
                this,
                LocaleService.Get("TEXT_CONFIG_RENAME_STYLE"),
                LocaleService.Get("TEXT_CONFIG_STYLE_NAME"),
                styleOrder.Select(node => stringValue(node)).Where(item => item != current),
                current);
            string normalized = name?.Trim() ?? string.Empty;
            if (!isValidStyleName(normalized, styles, current) || styles[current] is not JsonObject currentStyle)
                return;
            int index = styleList.SelectedIndex;
            styles.Remove(current);
            styles[normalized] = currentStyle;
            styleOrder[index] = normalized;
            applyChanges();
            rebuildInspector();
        };
        moveUp.Click += (_, _) =>
        {
            int index = styleList.SelectedIndex;
            if (index <= 0)
                return;
            JsonNode? item = styleOrder[index];
            styleOrder.RemoveAt(index);
            styleOrder.Insert(index - 1, item);
            applyChanges();
            rebuildInspector();
        };
        moveDown.Click += (_, _) =>
        {
            int index = styleList.SelectedIndex;
            if (index < 0 || index >= styleOrder.Count - 1)
                return;
            JsonNode? item = styleOrder[index];
            styleOrder.RemoveAt(index);
            styleOrder.Insert(index + 1, item);
            applyChanges();
            rebuildInspector();
        };
        delete.Click += (_, _) =>
        {
            if (styleList.SelectedItem is not string selected)
                return;
            styles.Remove(selected);
            styleOrder.RemoveAt(styleList.SelectedIndex);
            applyChanges();
            rebuildInspector();
        };
        refreshSelection();
    }

    private Control buildOptionalStyleInspector(JsonObject target)
    {
        StackPanel panel = new() { Spacing = 8, Margin = new Thickness(0, 8, 0, 0) };
        addOptionalNumber(
            panel,
            LocaleService.Get("TEXT_CONFIG_CHARACTER_SIZE"),
            target,
            "characterSize",
            22,
            1,
            512,
            1);
        addOptionalFlag(panel, LocaleService.Get("TEXT_CONFIG_BOLD"), target, "bold");
        addOptionalFlag(panel, LocaleService.Get("TEXT_CONFIG_ITALIC"), target, "italic");
        addOptionalFlag(panel, LocaleService.Get("TEXT_CONFIG_UNDERLINED"), target, "underlined");
        addOptionalFlag(panel, LocaleService.Get("TEXT_CONFIG_STRIKE_THROUGH"), target, "strikeThrough");
        addOptionalColour(panel, LocaleService.Get("TEXT_CONFIG_FILL_COLOR"), target, "fillColor", Colors.White);
        addOptionalNumber(
            panel,
            LocaleService.Get("TEXT_CONFIG_LETTER_SPACING"),
            target,
            "letterSpacing",
            1,
            0.1,
            10,
            0.1);
        addOptionalNumber(
            panel,
            LocaleService.Get("TEXT_CONFIG_LINE_SPACING"),
            target,
            "lineSpacing",
            1,
            0.1,
            10,
            0.1);
        addOptionalOutline(panel, target);
        return panel;
    }

    private void addOptionalNumber(
        StackPanel panel,
        string label,
        JsonObject target,
        string field,
        double fallback,
        double minimum,
        double maximum,
        double increment)
    {
        bool hasValue = target.ContainsKey(field);
        CheckBox enabled = new()
        {
            Content = LocaleService.Get("TEXT_CONFIG_OVERRIDE"),
            IsChecked = hasValue,
        };
        NumericUpDown input = EditorInputs.CreateNumericUpDown(
            (decimal)numberValue(target[field], fallback),
            (decimal)minimum,
            (decimal)maximum,
            (decimal)increment);
        input.IsEnabled = hasValue;
        enabled.IsCheckedChanged += (_, _) =>
        {
            input.IsEnabled = enabled.IsChecked == true;
            if (enabled.IsChecked == true)
                target[field] = (double)(input.Value ?? (decimal)fallback);
            else
                target.Remove(field);
            applyChanges();
        };
        input.ValueChanged += (_, _) =>
        {
            if (input.IsEnabled)
            {
                target[field] = (double)(input.Value ?? (decimal)fallback);
                applyChanges();
            }
        };
        panel.Children.Add(optionalRow(label, enabled, input));
    }

    private void addOptionalFlag(
        StackPanel panel,
        string label,
        JsonObject target,
        string field)
    {
        JsonObject? flags = target["style"] as JsonObject;
        bool hasValue = flags?.ContainsKey(field) == true;
        CheckBox enabled = new()
        {
            Content = LocaleService.Get("TEXT_CONFIG_OVERRIDE"),
            IsChecked = hasValue,
        };
        CheckBox input = new()
        {
            IsChecked = boolValue(flags?[field]),
            IsEnabled = hasValue,
        };
        enabled.IsCheckedChanged += (_, _) =>
        {
            input.IsEnabled = enabled.IsChecked == true;
            if (enabled.IsChecked == true)
            {
                JsonObject nextFlags = ensureObject(target, "style");
                nextFlags[field] = input.IsChecked == true;
            }
            else if (target["style"] is JsonObject currentFlags)
            {
                currentFlags.Remove(field);
                if (currentFlags.Count == 0)
                    target.Remove("style");
            }
            applyChanges();
        };
        input.IsCheckedChanged += (_, _) =>
        {
            if (input.IsEnabled)
            {
                ensureObject(target, "style")[field] = input.IsChecked == true;
                applyChanges();
            }
        };
        panel.Children.Add(optionalRow(label, enabled, input));
    }

    private void addOptionalColour(
        StackPanel panel,
        string label,
        JsonObject target,
        string field,
        Color fallback)
    {
        bool hasValue = target.ContainsKey(field);
        CheckBox enabled = new()
        {
            Content = LocaleService.Get("TEXT_CONFIG_OVERRIDE"),
            IsChecked = hasValue,
        };
        Button input = createColourButton(colourValue(target[field], fallback));
        input.IsEnabled = hasValue;
        enabled.IsCheckedChanged += (_, _) =>
        {
            input.IsEnabled = enabled.IsChecked == true;
            if (enabled.IsChecked == true)
                target[field] = colourArray((Color)input.Tag!);
            else
                target.Remove(field);
            applyChanges();
        };
        input.Click += async (_, _) =>
        {
            Color? selected = await ColourPickerWindow.ShowAsync(this, (Color)input.Tag!);
            if (selected is not Color colour)
                return;
            updateColourButton(input, colour);
            target[field] = colourArray(colour);
            applyChanges();
        };
        panel.Children.Add(optionalRow(label, enabled, input));
    }

    private void addOptionalOutline(StackPanel panel, JsonObject target)
    {
        JsonObject? outline = target["outline"] as JsonObject;
        bool hasOutline = outline is not null;
        CheckBox enabled = new()
        {
            Content = LocaleService.Get("TEXT_CONFIG_OVERRIDE"),
            IsChecked = hasOutline,
        };
        StackPanel fields = new() { Spacing = 6, IsEnabled = hasOutline };
        Button colour = createColourButton(colourValue(outline?["color"], Colors.Black));
        NumericUpDown thickness = EditorInputs.CreateNumericUpDown(
            (decimal)numberValue(outline?["thickness"], 0),
            0,
            32,
            0.25m);
        fields.Children.Add(compactRow(LocaleService.Get("TEXT_CONFIG_OUTLINE_COLOR"), colour));
        fields.Children.Add(compactRow(LocaleService.Get("TEXT_CONFIG_OUTLINE_THICKNESS"), thickness));
        enabled.IsCheckedChanged += (_, _) =>
        {
            fields.IsEnabled = enabled.IsChecked == true;
            if (enabled.IsChecked == true)
            {
                target["outline"] = new JsonObject
                {
                    ["color"] = colourArray((Color)colour.Tag!),
                    ["thickness"] = (double)(thickness.Value ?? 0),
                };
            }
            else
            {
                target.Remove("outline");
            }
            applyChanges();
        };
        colour.Click += async (_, _) =>
        {
            Color? selected = await ColourPickerWindow.ShowAsync(this, (Color)colour.Tag!);
            if (selected is not Color next)
                return;
            updateColourButton(colour, next);
            ensureObject(target, "outline")["color"] = colourArray(next);
            applyChanges();
        };
        thickness.ValueChanged += (_, _) =>
        {
            if (fields.IsEnabled)
            {
                ensureObject(target, "outline")["thickness"] = (double)(thickness.Value ?? 0);
                applyChanges();
            }
        };
        Grid row = new()
        {
            ColumnDefinitions = new ColumnDefinitions("112,*"),
            ColumnSpacing = 8,
        };
        row.Children.Add(new TextBlock
        {
            Text = LocaleService.Get("TEXT_CONFIG_OUTLINE"),
            VerticalAlignment = VerticalAlignment.Center,
        });
        StackPanel right = new() { Spacing = 6 };
        right.Children.Add(enabled);
        right.Children.Add(fields);
        Grid.SetColumn(right, 1);
        row.Children.Add(right);
        panel.Children.Add(row);
    }

    private static Grid optionalRow(
        string label,
        CheckBox enabled,
        Control input)
    {
        Grid row = new()
        {
            ColumnDefinitions = new ColumnDefinitions("112,94,*"),
            ColumnSpacing = 8,
        };
        row.Children.Add(new TextBlock { Text = label, VerticalAlignment = VerticalAlignment.Center });
        Grid.SetColumn(enabled, 1);
        row.Children.Add(enabled);
        Grid.SetColumn(input, 2);
        row.Children.Add(input);
        return row;
    }

    private static Grid compactRow(string label, Control input)
    {
        Grid row = new()
        {
            ColumnDefinitions = new ColumnDefinitions("112,*"),
            ColumnSpacing = 8,
        };
        row.Children.Add(new TextBlock { Text = label, VerticalAlignment = VerticalAlignment.Center });
        Grid.SetColumn(input, 1);
        row.Children.Add(input);
        return row;
    }

    private void addSection(string text)
    {
        inspector.Children.Add(new TextBlock
        {
            Text = text,
            FontSize = 17,
            FontWeight = FontWeight.Bold,
            Margin = new Thickness(0, inspector.Children.Count == 0 ? 0 : 12, 0, 2),
        });
    }

    private void addSubsection(string text)
    {
        inspector.Children.Add(new TextBlock
        {
            Text = text,
            FontWeight = FontWeight.Bold,
            Margin = new Thickness(0, 6, 0, 0),
        });
    }

    private void addStringField(string label, JsonObject target, string field)
    {
        TextBox input = EditorInputs.CreateEditableTextBox(stringValue(target[field]));
        input.TextChanged += (_, _) =>
        {
            if (syncing)
                return;
            target[field] = input.Text ?? string.Empty;
            applyChanges();
        };
        addRow(label, input);
    }

    private CheckBox addCheckField(string label, JsonObject target, string field)
    {
        CheckBox input = new() { IsChecked = boolValue(target[field]) };
        input.IsCheckedChanged += (_, _) =>
        {
            if (syncing)
                return;
            target[field] = input.IsChecked == true;
            applyChanges();
        };
        addRow(label, input);
        return input;
    }

    private NumericUpDown addNumberField(
        string label,
        JsonObject target,
        string field,
        double minimum,
        double maximum,
        double increment)
    {
        NumericUpDown input = EditorInputs.CreateNumericUpDown(
            (decimal)numberValue(target[field], minimum),
            (decimal)minimum,
            (decimal)maximum,
            (decimal)increment);
        input.ValueChanged += (_, _) =>
        {
            if (syncing)
                return;
            target[field] = (double)(input.Value ?? (decimal)minimum);
            applyChanges();
        };
        addRow(label, input);
        return input;
    }

    private void addChoiceField(
        string label,
        JsonObject target,
        string field,
        IReadOnlyList<string> choices)
    {
        ComboBox input = new()
        {
            ItemsSource = choices,
            SelectedItem = choices.Contains(stringValue(target[field]))
                ? stringValue(target[field])
                : choices[0],
            HorizontalAlignment = HorizontalAlignment.Stretch,
        };
        input.SelectionChanged += (_, _) =>
        {
            if (syncing || input.SelectedItem is not string selected)
                return;
            target[field] = selected;
            applyChanges();
        };
        addRow(label, input);
    }

    private void addColourField(
        string label,
        JsonObject target,
        string field,
        Color fallback)
    {
        Button input = createColourButton(colourValue(target[field], fallback));
        input.Click += async (_, _) =>
        {
            Color? selected = await ColourPickerWindow.ShowAsync(this, (Color)input.Tag!);
            if (selected is not Color colour)
                return;
            updateColourButton(input, colour);
            target[field] = colourArray(colour);
            applyChanges();
        };
        addRow(label, input);
    }

    private void addReferenceField(
        string label,
        JsonObject target,
        string field,
        TextConfigReferenceKind kind)
    {
        TextBox input = EditorInputs.CreateEditableTextBox(stringValue(target[field]));
        input.TextChanged += (_, _) =>
        {
            if (syncing)
                return;
            target[field] = normalizeReference(input.Text);
            applyChanges();
        };
        Button select = new() { Content = "…", Width = 38 };
        select.Click += async (_, _) =>
        {
            string? selected = await selectReference(kind, stringValue(target[field]));
            if (selected is null)
                return;
            target[field] = selected;
            input.Text = selected;
            applyChanges();
        };
        Grid fieldGrid = new()
        {
            ColumnDefinitions = new ColumnDefinitions("*,Auto"),
            ColumnSpacing = 6,
        };
        fieldGrid.Children.Add(input);
        Grid.SetColumn(select, 1);
        fieldGrid.Children.Add(select);
        addRow(label, fieldGrid);
    }

    private void addRow(string label, Control input)
    {
        inspector.Children.Add(compactRow(label, input));
    }

    private static Button createColourButton(Color colour)
    {
        Button button = new()
        {
            HorizontalAlignment = HorizontalAlignment.Stretch,
            HorizontalContentAlignment = HorizontalAlignment.Left,
            Tag = colour,
        };
        updateColourButton(button, colour);
        return button;
    }

    private static void updateColourButton(Button button, Color colour)
    {
        Border swatch = new()
        {
            Width = 28,
            Height = 20,
            Background = new SolidColorBrush(colour),
            BorderBrush = new SolidColorBrush(Color.Parse("#777777")),
            BorderThickness = new Thickness(1),
        };
        StackPanel content = new()
        {
            Orientation = Orientation.Horizontal,
            Spacing = 8,
            Children =
            {
                swatch,
                new TextBlock
                {
                    Text = $"#{colour.R:X2}{colour.G:X2}{colour.B:X2}{colour.A:X2}",
                    VerticalAlignment = VerticalAlignment.Center,
                },
            },
        };
        button.Tag = colour;
        button.Content = content;
    }

    private async Task<string?> selectReference(
        TextConfigReferenceKind kind,
        string current)
    {
        if (kind == TextConfigReferenceKind.Curve)
        {
            return await ItemSelectorDialog.ShowAsync(
                this,
                LocaleService.Get("TEXT_CONFIG_CURVE"),
                LocaleService.Get("TEXT_CONFIG_CURVE"),
                gameData.CurvesData
                    .Where(item => stringValue(item.Value["type"]) == "vector4Curve")
                    .Select(item => item.Key)
                    .OrderBy(item => item, StringComparer.Ordinal),
                current);
        }
        string root = Path.Combine(gameData.ProjectPath, "Assets", "Fonts");
        Directory.CreateDirectory(root);
        string filter = FileSelectorDialog.FilesFilter("*.ttf", "*.otf");
        string? path = await FileSelectorDialog.ShowAsync(
            this,
            root,
            filter,
            LocaleService.Get("TEXT_CONFIG_FONT"));
        return path is null ? null : Path.GetRelativePath(root, path).Replace('\\', '/');
    }

    private void applyChanges()
    {
        refreshPreview();
        IReadOnlyList<string> errors = getReferenceErrors();
        updateValidation(errors);
        if (errors.Count != 0)
            return;
        if (!gameData.TextConfigsData.ContainsKey(key))
        {
            Close();
            return;
        }
        gameData.UpdateTextConfig(key, data);
    }

    private void refreshPreview()
    {
        preview.Update(data, previewText.Text ?? string.Empty);
    }

    private void updateValidation()
    {
        updateValidation(getReferenceErrors());
    }

    private void updateValidation(IReadOnlyList<string> errors)
    {
        validationText.Text = errors.Count == 0
            ? string.Empty
            : LocaleService.Get("TEXT_CONFIG_INVALID_REFERENCES")
                .Replace("{details}", string.Join(", ", errors));
        validationText.IsVisible = errors.Count != 0;
    }

    private IReadOnlyList<string> getReferenceErrors()
    {
        List<string> errors = [];
        string font = stringValue(data["font"]);
        if (font.Length != 0
            && !TextConfigFontLoader.TryResolve(gameData.ProjectPath, font, out _))
        {
            errors.Add($"{LocaleService.Get("TEXT_CONFIG_FONT")}: {font}");
        }
        JsonObject gradient = ensureObject(data, "gradient");
        string curve = stringValue(gradient["curve"]);
        bool enabled = boolValue(gradient["enabled"]);
        if (enabled && curve.Length == 0)
            errors.Add(LocaleService.Get("TEXT_CONFIG_CURVE"));
        else if (curve.Length != 0
            && (!gameData.CurvesData.TryGetValue(curve, out JsonObject? curveData)
                || stringValue(curveData["type"]) != "vector4Curve"))
        {
            errors.Add($"{LocaleService.Get("TEXT_CONFIG_CURVE")}: {curve}");
        }
        return errors;
    }

    private async void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (!EditorShortcuts.HasPrimaryModifier(args.KeyModifiers))
            return;
        if (args.Key == Key.S)
        {
            IReadOnlyList<string> errors = getReferenceErrors();
            if (errors.Count != 0)
            {
                await AlertDialog.ShowAsync(
                    this,
                    LocaleService.Get("ERROR"),
                    LocaleService.Get("TEXT_CONFIG_INVALID_REFERENCES")
                        .Replace("{details}", string.Join(", ", errors)));
            }
            else
            {
                if (!gameData.TextConfigsData.ContainsKey(key))
                {
                    await AlertDialog.ShowAsync(
                        this,
                        LocaleService.Get("ERROR"),
                        LocaleService.Get("TEXT_CONFIG_NO_LONGER_EXISTS"));
                    Close();
                    args.Handled = true;
                    return;
                }
                gameData.UpdateTextConfig(key, data);
                await EditorSaveWorkflow.TrySaveAsync(this, projectSave);
            }
        }
        else if (args.Key == Key.Z)
            EditorFeedback.ShowHistory(toast, "Undo", gameData.Undo());
        else if (args.Key == Key.Y)
            EditorFeedback.ShowHistory(toast, "Redo", gameData.Redo());
        else
            return;
        args.Handled = true;
    }

    private void onDataChanged(object? sender, EventArgs args)
    {
        if (!gameData.TextConfigsData.ContainsKey(key))
        {
            Close();
            return;
        }
        updateValidation();
        refreshPreview();
    }

    private void onDataRestored(object? sender, EventArgs args)
    {
        if (gameData.TextConfigsData.TryGetValue(key, out JsonObject? restored))
            Reload(restored);
        else
            Close();
    }

    private bool isRich()
    {
        return stringValue(data["type"]) == "richTextConfig";
    }

    private void normalizeData()
    {
        bool rich = isRich();
        data["type"] = rich ? "richTextConfig" : "plainTextConfig";
        setDefault(data, "name", Path.GetFileNameWithoutExtension(key));
        setDefault(data, "font", string.Empty);
        setDefault(data, "lineAlignment", "default");
        if (rich)
        {
            normalizeStyle(ensureObject(data, "defaultStyle"));
            JsonArray order = ensureArray(data, "styleOrder");
            JsonObject styles = ensureObject(data, "styles");
            foreach (JsonNode? node in order)
            {
                string name = stringValue(node);
                if (name.Length != 0 && styles[name] is not JsonObject)
                    styles[name] = new JsonObject();
            }
        }
        else
        {
            normalizeStyle(data);
            setDefault(data, "slantAngle", 0.0);
            data["slantAngle"] = Math.Clamp(
                numberValue(data["slantAngle"]),
                -45,
                45);
        }
        JsonObject glow = ensureObject(data, "glow");
        setDefault(glow, "enabled", false);
        setDefault(glow, "color", colourArray(Colors.Transparent));
        setDefault(glow, "radius", 0.0);
        setDefault(glow, "intensity", 0.0);
        glow["intensity"] = Math.Clamp(numberValue(glow["intensity"]), 0, 1);
        JsonObject gradient = ensureObject(data, "gradient");
        setDefault(gradient, "enabled", false);
        setDefault(gradient, "direction", "vertical");
        setDefault(gradient, "curve", string.Empty);
        gradient.Remove("startColor");
        gradient.Remove("endColor");
        gradient.Remove("shader");
    }

    private static void normalizeStyle(JsonObject style)
    {
        setDefault(style, "characterSize", 22);
        JsonObject flags = ensureObject(style, "style");
        setDefault(flags, "bold", false);
        setDefault(flags, "italic", false);
        setDefault(flags, "underlined", false);
        setDefault(flags, "strikeThrough", false);
        setDefault(style, "fillColor", colourArray(Colors.White));
        setDefault(style, "letterSpacing", 1.0);
        setDefault(style, "lineSpacing", 1.0);
        JsonObject outline = ensureObject(style, "outline");
        setDefault(outline, "color", colourArray(Colors.Black));
        setDefault(outline, "thickness", 0.0);
    }

    private static void setDefault(JsonObject target, string field, JsonNode? value)
    {
        if (!target.ContainsKey(field))
            target[field] = value;
    }

    private static JsonObject ensureObject(JsonObject target, string field)
    {
        if (target[field] is JsonObject value)
            return value;
        JsonObject result = new();
        target[field] = result;
        return result;
    }

    private static JsonArray ensureArray(JsonObject target, string field)
    {
        if (target[field] is JsonArray value)
            return value;
        JsonArray result = new();
        target[field] = result;
        return result;
    }

    private static string normalizeReference(string? value)
    {
        return (value ?? string.Empty).Trim().Replace('\\', '/');
    }

    private static string stringValue(JsonNode? node)
    {
        return node is JsonValue value && value.TryGetValue<string>(out string? result)
            ? result ?? string.Empty
            : string.Empty;
    }

    private static bool boolValue(JsonNode? node)
    {
        return node is JsonValue value && value.TryGetValue<bool>(out bool result) && result;
    }

    private static double numberValue(JsonNode? node, double fallback = 0)
    {
        return node is JsonValue value && value.TryGetValue<double>(out double result)
            ? result
            : fallback;
    }

    private static Color colourValue(JsonNode? node, Color fallback)
    {
        if (node is not JsonArray values || values.Count < 4)
            return fallback;
        return Color.FromArgb(
            byteValue(values[3], fallback.A),
            byteValue(values[0], fallback.R),
            byteValue(values[1], fallback.G),
            byteValue(values[2], fallback.B));
    }

    private static byte byteValue(JsonNode? node, byte fallback)
    {
        if (node is not JsonValue value || !value.TryGetValue<int>(out int result))
            return fallback;
        return (byte)Math.Clamp(result, 0, 255);
    }

    private static JsonArray colourArray(Color colour)
    {
        return new JsonArray(colour.R, colour.G, colour.B, colour.A);
    }

    private static bool isValidStyleName(
        string name,
        JsonObject styles,
        string? current)
    {
        return name.Length != 0
            && !name.Contains('#')
            && !string.Equals(name, "default", StringComparison.Ordinal)
            && (name == current || !styles.ContainsKey(name));
    }

    private enum TextConfigReferenceKind
    {
        Font,
        Curve,
    }
}

internal static class TextConfigFontLoader
{
    private static readonly Dictionary<string, FontCacheEntry> Cache =
        new(StringComparer.OrdinalIgnoreCase);

    public static bool TryResolve(
        string projectPath,
        string reference,
        out FontFamily family)
    {
        family = FontFamily.Default;
        if (!tryResolvePath(projectPath, reference, out string path))
            return false;
        DateTime stamp = File.GetLastWriteTimeUtc(path);
        if (Cache.TryGetValue(path, out FontCacheEntry? cached)
            && cached.Stamp == stamp)
        {
            family = cached.Family;
            return true;
        }
        using SKTypeface? typeface = SKTypeface.FromFile(path);
        if (typeface is null || string.IsNullOrWhiteSpace(typeface.FamilyName))
            return false;
        Uri baseUri = new(Path.GetDirectoryName(path)! + Path.DirectorySeparatorChar);
        family = new FontFamily(
            baseUri,
            $"{Path.GetFileName(path)}#{typeface.FamilyName}");
        Cache[path] = new FontCacheEntry(stamp, family);
        return true;
    }

    private static bool tryResolvePath(
        string projectPath,
        string reference,
        out string path)
    {
        path = string.Empty;
        if (reference.Length == 0
            || Path.IsPathRooted(reference)
            || reference.IndexOfAny(Path.GetInvalidPathChars()) >= 0)
        {
            return false;
        }
        string extension = Path.GetExtension(reference);
        if (!string.Equals(extension, ".ttf", StringComparison.OrdinalIgnoreCase)
            && !string.Equals(extension, ".otf", StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }
        string root = Path.GetFullPath(Path.Combine(projectPath, "Assets", "Fonts"));
        path = Path.GetFullPath(Path.Combine(
            root,
            reference.Replace('/', Path.DirectorySeparatorChar)));
        string relative = Path.GetRelativePath(root, path);
        return !Path.IsPathRooted(relative)
            && relative != ".."
            && !relative.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal)
            && File.Exists(path);
    }

    private sealed record FontCacheEntry(DateTime Stamp, FontFamily Family);
}

internal sealed class TextConfigPreview : Border
{
    private readonly GameDataService gameData;
    private readonly Grid layers = new();

    public TextConfigPreview(GameDataService gameData)
    {
        this.gameData = gameData;
        Background = new SolidColorBrush(Color.Parse("#181818"));
        BorderBrush = new SolidColorBrush(Color.Parse("#3a3a3a"));
        BorderThickness = new Thickness(1);
        CornerRadius = new CornerRadius(4);
        Padding = new Thickness(44);
        ClipToBounds = true;
        Child = layers;
    }

    public void Update(JsonObject data, string text)
    {
        layers.Children.Clear();
        PreviewStyle style = PreviewStyle.FromConfig(data);
        IReadOnlyList<PreviewStyle> textStyles = activeStyles(data, text);
        JsonObject glow = objectValue(data["glow"]);
        if (boolValue(glow["enabled"]))
            addGlowLayers(data, text, style, glow);
        foreach (PreviewStyle outlineStyle in textStyles
                     .Where(item => item.OutlineThickness > 0)
                     .DistinctBy(item => item.OutlineThickness))
        {
            addOutlineLayers(data, text, outlineStyle);
        }
        layers.Children.Add(createText(
            data,
            text,
            new SolidColorBrush(style.FillColor),
            new Vector(0, 0)));
    }

    private void addGlowLayers(
        JsonObject data,
        string text,
        PreviewStyle style,
        JsonObject glow)
    {
        Color colour = colourValue(glow["color"], Colors.Transparent);
        double radius = Math.Max(0, numberValue(glow["radius"]));
        double intensity = Math.Clamp(numberValue(glow["intensity"]), 0, 1);
        if (radius <= 0 || intensity <= 0 || colour.A == 0)
            return;
        int rings = Math.Clamp((int)Math.Ceiling(radius / 2), 1, 4);
        for (int ring = rings; ring >= 1; ring -= 1)
        {
            double ringRadius = radius * ring / rings;
            double opacity = Math.Min(1, intensity / rings * 0.45);
            for (int index = 0; index < 16; index += 1)
            {
                double angle = Math.PI * 2 * index / 16;
                Control layer = createText(
                    data,
                    text,
                    new SolidColorBrush(colour),
                    new Vector(Math.Cos(angle) * ringRadius, Math.Sin(angle) * ringRadius),
                    PreviewLayer.Forced);
                layer.Opacity = opacity;
                layers.Children.Add(layer);
            }
        }
    }

    private void addOutlineLayers(
        JsonObject data,
        string text,
        PreviewStyle style)
    {
        int samples = Math.Clamp((int)Math.Ceiling(style.OutlineThickness * 8), 8, 32);
        SolidColorBrush brush = new(style.OutlineColor);
        for (int index = 0; index < samples; index += 1)
        {
            double angle = Math.PI * 2 * index / samples;
            layers.Children.Add(createText(
                data,
                text,
                brush,
                new Vector(
                    Math.Cos(angle) * style.OutlineThickness,
                    Math.Sin(angle) * style.OutlineThickness),
                stringValue(data["type"]) == "richTextConfig"
                    ? PreviewLayer.Outline
                    : PreviewLayer.Forced,
                style.OutlineThickness));
        }
    }

    private Control createText(
        JsonObject data,
        string text,
        IBrush brush,
        Vector offset,
        PreviewLayer layer = PreviewLayer.Fill,
        double targetOutlineThickness = 0)
    {
        return stringValue(data["type"]) == "richTextConfig"
            ? createRichText(
                data,
                text,
                brush,
                offset,
                layer,
                targetOutlineThickness)
            : createPlainText(data, text, brush, offset, layer);
    }

    private Control createPlainText(
        JsonObject data,
        string text,
        IBrush brush,
        Vector offset,
        PreviewLayer layer)
    {
        PreviewStyle defaultStyle = PreviewStyle.FromConfig(data);
        FontFamily fontFamily = resolveFont(data);
        double measuredLineHeight = lineHeight(fontFamily, defaultStyle);
        double slantAngle = defaultStyle.Italic
            ? 0
            : Math.Clamp(numberValue(data["slantAngle"]), -45, 45);
        string normalizedText = text
            .Replace("\r\n", "\n", StringComparison.Ordinal)
            .Replace('\r', '\n');
        string[] lines = normalizedText.Split('\n');
        List<TextBlock> blocks = [];
        double maximumWidth = 1;
        foreach (string line in lines)
        {
            TextBlock block = new()
            {
                MaxWidth = 620,
                Height = measuredLineHeight,
                HorizontalAlignment = HorizontalAlignment.Stretch,
                TextWrapping = TextWrapping.NoWrap,
                TextAlignment = alignment(stringValue(data["lineAlignment"])),
                FontFamily = fontFamily,
                FontSize = defaultStyle.CharacterSize,
                FontWeight = defaultStyle.Bold ? FontWeight.Bold : FontWeight.Normal,
                FontStyle = defaultStyle.Italic ? FontStyle.Italic : FontStyle.Normal,
                LetterSpacing = defaultStyle.LetterSpacing,
                LineHeight = measuredLineHeight,
                Foreground = brush,
                Text = line.Length == 0 ? "\u200B" : line,
                RenderTransform = new SkewTransform(-slantAngle, 0),
            };
            block.TextDecorations = decorations(defaultStyle);
            block.Measure(new Size(10000, measuredLineHeight));
            maximumWidth = Math.Max(
                maximumWidth,
                Math.Min(620, block.DesiredSize.Width));
            blocks.Add(block);
        }
        double totalHeight = Math.Max(1, measuredLineHeight * blocks.Count);
        StackPanel result = new()
        {
            Width = maximumWidth,
            HorizontalAlignment = HorizontalAlignment.Center,
            VerticalAlignment = VerticalAlignment.Center,
            RenderTransform = new TranslateTransform(offset.X, offset.Y),
            Spacing = 0,
        };
        double verticalOffset = 0;
        foreach (TextBlock block in blocks)
        {
            block.Width = maximumWidth;
            if (layer == PreviewLayer.Fill)
            {
                block.Foreground = createFillBrush(
                    data,
                    defaultStyle.FillColor,
                    maximumWidth,
                    measuredLineHeight,
                    verticalOffset / totalHeight,
                    measuredLineHeight / totalHeight,
                    true);
            }
            result.Children.Add(block);
            verticalOffset += measuredLineHeight;
        }
        return result;
    }

    private Control createRichText(
        JsonObject data,
        string text,
        IBrush layerBrush,
        Vector offset,
        PreviewLayer layer,
        double targetOutlineThickness)
    {
        PreviewStyle defaultStyle = PreviewStyle.FromConfig(data);
        FontFamily fontFamily = resolveFont(data);
        IReadOnlyList<RichPreviewLine> lines = parseRichLines(data, text);
        List<RichLineVisual> lineVisuals = [];
        double maximumWidth = 1;
        foreach (RichPreviewLine line in lines)
        {
            IReadOnlyList<PreviewStyle> lineStyles = line.Segments.Count == 0
                ? [line.EndStyle]
                : line.Segments.Select(item => item.Style).Append(line.EndStyle).ToArray();
            double lineHeight = lineStyles.Max(item => TextConfigPreview.lineHeight(fontFamily, item));
            TextBlock block = new()
            {
                MaxWidth = 620,
                Height = lineHeight,
                HorizontalAlignment = HorizontalAlignment.Stretch,
                TextWrapping = TextWrapping.NoWrap,
                TextAlignment = alignment(stringValue(data["lineAlignment"])),
                FontFamily = fontFamily,
                FontSize = line.EndStyle.CharacterSize,
                FontWeight = line.EndStyle.Bold ? FontWeight.Bold : FontWeight.Normal,
                FontStyle = line.EndStyle.Italic ? FontStyle.Italic : FontStyle.Normal,
                LetterSpacing = line.EndStyle.LetterSpacing,
                LineHeight = lineHeight,
                Foreground = layerBrush,
                TextDecorations = decorations(line.EndStyle),
            };
            List<RichRunVisual> runs = [];
            foreach (RichPreviewSegment segment in line.Segments)
            {
                Run run = createRun(
                    segment.Content,
                    segment.Style,
                    layerBrush,
                    layer,
                    targetOutlineThickness);
                block.Inlines!.Add(run);
                runs.Add(new RichRunVisual(run, segment.Style));
            }
            if (line.Segments.Count == 0)
            {
                Run empty = createRun(
                    "\u200B",
                    line.EndStyle,
                    layerBrush,
                    layer,
                    targetOutlineThickness);
                block.Inlines!.Add(empty);
                runs.Add(new RichRunVisual(empty, line.EndStyle));
            }
            block.Measure(new Size(10000, lineHeight));
            double desiredWidth = Math.Max(1, Math.Min(620, block.DesiredSize.Width));
            maximumWidth = Math.Max(maximumWidth, desiredWidth);
            lineVisuals.Add(new RichLineVisual(block, runs, lineHeight));
        }
        double totalHeight = Math.Max(1, lineVisuals.Sum(item => item.Height));
        StackPanel result = new()
        {
            Width = maximumWidth,
            HorizontalAlignment = HorizontalAlignment.Center,
            VerticalAlignment = VerticalAlignment.Center,
            RenderTransform = new TranslateTransform(offset.X, offset.Y),
            Spacing = 0,
        };
        double verticalOffset = 0;
        foreach (RichLineVisual visual in lineVisuals)
        {
            visual.Block.Width = maximumWidth;
            if (layer == PreviewLayer.Fill)
            {
                foreach (RichRunVisual run in visual.Runs)
                {
                    run.Run.Foreground = createFillBrush(
                        data,
                        run.Style.FillColor,
                        maximumWidth,
                        visual.Height,
                        verticalOffset / totalHeight,
                        visual.Height / totalHeight);
                }
            }
            result.Children.Add(visual.Block);
            verticalOffset += visual.Height;
        }
        return result;
    }

    private static double lineHeight(FontFamily fontFamily, PreviewStyle style)
    {
        Typeface typeface = new(
            fontFamily,
            style.Italic ? FontStyle.Italic : FontStyle.Normal,
            style.Bold ? FontWeight.Bold : FontWeight.Normal,
            FontStretch.Normal);
        if (!FontManager.Current.TryGetGlyphTypeface(typeface, out GlyphTypeface? glyphTypeface)
            || glyphTypeface is null)
        {
            return Math.Max(1, style.CharacterSize * style.LineSpacing);
        }
        FontMetrics metrics = glyphTypeface.Metrics;
        double naturalLineHeight = metrics.LineSpacing
            * style.CharacterSize
            / metrics.DesignEmHeight;
        return Math.Max(1, naturalLineHeight * style.LineSpacing);
    }

    private static Run createRun(
        string content,
        PreviewStyle style,
        IBrush layerBrush,
        PreviewLayer layer,
        double targetOutlineThickness)
    {
        IBrush fill;
        if (layer == PreviewLayer.Forced)
        {
            fill = layerBrush;
        }
        else if (layer == PreviewLayer.Outline)
        {
            fill = Math.Abs(style.OutlineThickness - targetOutlineThickness) < 0.001
                ? new SolidColorBrush(style.OutlineColor)
                : Brushes.Transparent;
        }
        else
        {
            fill = new SolidColorBrush(style.FillColor);
        }
        return new Run(content)
        {
            FontSize = style.CharacterSize,
            FontWeight = style.Bold ? FontWeight.Bold : FontWeight.Normal,
            FontStyle = style.Italic ? FontStyle.Italic : FontStyle.Normal,
            Foreground = fill,
            LetterSpacing = style.LetterSpacing,
            TextDecorations = decorations(style),
        };
    }

    private static IReadOnlyList<PreviewStyle> activeStyles(
        JsonObject data,
        string text)
    {
        PreviewStyle defaultStyle = PreviewStyle.FromConfig(data);
        if (stringValue(data["type"]) != "richTextConfig")
            return [defaultStyle];
        List<PreviewStyle> result = parseRichLines(data, text)
            .SelectMany(line => line.Segments.Select(segment => segment.Style))
            .ToList();
        if (result.Count == 0)
            result.Add(defaultStyle);
        return result;
    }

    private static IReadOnlyList<RichPreviewLine> parseRichLines(
        JsonObject data,
        string text)
    {
        JsonObject styles = objectValue(data["styles"]);
        PreviewStyle defaultStyle = PreviewStyle.FromConfig(data);
        PreviewStyle current = defaultStyle;
        List<RichPreviewLine> result = [];
        List<RichPreviewSegment> segments = [];

        void appendContent(string content)
        {
            int offset = 0;
            while (offset <= content.Length)
            {
                int lineEnd = content.IndexOf('\n', offset);
                if (lineEnd < 0)
                {
                    string remaining = content[offset..];
                    if (remaining.Length != 0)
                        segments.Add(new RichPreviewSegment(remaining, current));
                    return;
                }
                string lineContent = content[offset..lineEnd];
                if (lineContent.Length != 0)
                    segments.Add(new RichPreviewSegment(lineContent, current));
                result.Add(new RichPreviewLine(segments, current));
                segments = [];
                offset = lineEnd + 1;
            }
        }

        foreach ((string content, string? marker) in parseSegments(text))
        {
            if (marker is null)
            {
                appendContent(content);
                continue;
            }
            if (marker == "default")
                current = defaultStyle;
            else if (styles[marker] is JsonObject named)
                current = current.Adapt(named);
            else if (tryParseMarkerColour(marker, out Color markerColour))
                current = current with { FillColor = markerColour };
            else
                appendContent($"#{marker}#");
        }
        result.Add(new RichPreviewLine(segments, current));
        if (text.EndsWith('\n') && result.Count > 1)
            result.RemoveAt(result.Count - 1);
        return result;
    }

    private static IEnumerable<(string Content, string? Marker)> parseSegments(string text)
    {
        int offset = 0;
        while (offset < text.Length)
        {
            int markerStart = text.IndexOf('#', offset);
            if (markerStart < 0)
            {
                yield return (text[offset..], null);
                yield break;
            }
            if (markerStart > offset)
                yield return (text[offset..markerStart], null);
            int markerEnd = text.IndexOf('#', markerStart + 1);
            if (markerEnd < 0)
            {
                yield return (text[markerStart..], null);
                yield break;
            }
            yield return (string.Empty, text[(markerStart + 1)..markerEnd]);
            offset = markerEnd + 1;
        }
    }

    private IBrush createFillBrush(
        JsonObject data,
        Color fallback,
        double width,
        double height,
        double verticalStart,
        double verticalSpan,
        bool useRelativeVerticalBounds = false)
    {
        JsonObject gradient = objectValue(data["gradient"]);
        if (!boolValue(gradient["enabled"]))
            return new SolidColorBrush(fallback);
        string curve = stringValue(gradient["curve"]);
        JsonObject? curveData = gameData.CurvesData.TryGetValue(curve, out JsonObject? value)
            ? value
            : null;
        PreviewVectorCurve? vectorCurve = createPreviewVectorCurve(curveData);
        if (vectorCurve is null)
            return new SolidColorBrush(fallback);
        bool horizontal = stringValue(gradient["direction"]) == "horizontal";
        LinearGradientBrush brush = new()
        {
            StartPoint = new RelativePoint(
                0,
                0,
                !horizontal && useRelativeVerticalBounds
                    ? RelativeUnit.Relative
                    : RelativeUnit.Absolute),
            EndPoint = horizontal
                ? new RelativePoint(Math.Max(1, width), 0, RelativeUnit.Absolute)
                : useRelativeVerticalBounds
                    ? new RelativePoint(0, 1, RelativeUnit.Relative)
                    : new RelativePoint(0, Math.Max(1, height), RelativeUnit.Absolute),
        };
        for (int index = 0; index <= 24; index += 1)
        {
            double position = index / 24.0;
            double curvePosition = horizontal
                ? position
                : verticalStart + position * verticalSpan;
            Color sampledColor = curveColour(evaluateCurve(vectorCurve, curvePosition));
            brush.GradientStops.Add(new GradientStop(multiply(fallback, sampledColor), position));
        }
        return brush;
    }

    private FontFamily resolveFont(JsonObject data)
    {
        string font = stringValue(data["font"]);
        return font.Length != 0
            && TextConfigFontLoader.TryResolve(gameData.ProjectPath, font, out FontFamily family)
                ? family
                : FontFamily.Default;
    }

    private static TextDecorationCollection? decorations(PreviewStyle style)
    {
        TextDecorationCollection result = new();
        if (style.Underlined)
        {
            foreach (TextDecoration decoration in TextDecorations.Underline)
                result.Add(decoration);
        }
        if (style.StrikeThrough)
        {
            foreach (TextDecoration decoration in TextDecorations.Strikethrough)
                result.Add(decoration);
        }
        return result.Count == 0 ? null : result;
    }

    private static TextAlignment alignment(string value)
    {
        return value switch
        {
            "center" => TextAlignment.Center,
            "right" => TextAlignment.Right,
            _ => TextAlignment.Left,
        };
    }

    private static bool tryParseMarkerColour(string marker, out Color colour)
    {
        string value = marker.TrimStart('#', '$');
        if (value.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
            value = value[2..];
        if (value.Length is not 6 and not 8
            || !uint.TryParse(value, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out uint parsed))
        {
            colour = Colors.White;
            return false;
        }
        if (value.Length == 6)
            parsed = parsed << 8 | 255;
        colour = Color.FromArgb(
            (byte)(parsed & 255),
            (byte)(parsed >> 24),
            (byte)(parsed >> 16),
            (byte)(parsed >> 8));
        return true;
    }

    private static Color multiply(Color left, Color right)
    {
        return Color.FromArgb(
            multiplyChannel(left.A, right.A),
            multiplyChannel(left.R, right.R),
            multiplyChannel(left.G, right.G),
            multiplyChannel(left.B, right.B));
    }

    private static byte multiplyChannel(byte left, byte right)
    {
        return (byte)((left * right + 127) / 255);
    }

    private static Color curveColour(IReadOnlyList<double> value)
    {
        return Color.FromArgb(
            curveChannel(value[3]),
            curveChannel(value[0]),
            curveChannel(value[1]),
            curveChannel(value[2]));
    }

    private static byte curveChannel(double value)
    {
        return (byte)Math.Clamp((int)Math.Round(value), 0, 255);
    }

    private static PreviewVectorCurve? createPreviewVectorCurve(JsonObject? curve)
    {
        if (stringValue(curve?["type"]) != "vector4Curve"
            || curve?["keys"] is not JsonArray source)
        {
            return null;
        }
        List<PreviewVectorCurveKey> keys = source
            .OfType<JsonObject>()
            .Select(item => new PreviewVectorCurveKey(
                numberValue(item["time"]),
                vectorValue(item["value"], 4),
                stringValue(item["interpolation"]),
                vectorValue(item["arriveTangent"], 4),
                vectorValue(item["leaveTangent"], 4)))
            .OrderBy(item => item.Time)
            .ToList();
        return new PreviewVectorCurve(
            vectorValue(curve["defaultValue"], 4),
            keys);
    }

    private static double[] vectorValue(JsonNode? node, int componentCount)
    {
        JsonArray? values = node as JsonArray;
        double[] result = new double[componentCount];
        for (int index = 0; index < componentCount; index += 1)
        {
            result[index] = values is not null && index < values.Count
                ? numberValue(values[index])
                : 0;
        }
        return result;
    }

    private static double[] evaluateCurve(PreviewVectorCurve curve, double time)
    {
        IReadOnlyList<PreviewVectorCurveKey> keys = curve.Keys;
        if (keys.Count == 0)
            return curve.DefaultValue;
        if (time <= keys[0].Time)
            return keys[0].Value;
        if (time >= keys[^1].Time)
            return keys[^1].Value;
        for (int index = 0; index < keys.Count - 1; index += 1)
        {
            PreviewVectorCurveKey left = keys[index];
            PreviewVectorCurveKey right = keys[index + 1];
            if (time > right.Time)
                continue;
            double duration = Math.Max(0.000001, right.Time - left.Time);
            double position = (time - left.Time) / duration;
            if (left.Interpolation == "constant")
                return left.Value;
            double[] result = new double[left.Value.Length];
            if (left.Interpolation != "cubic")
            {
                for (int component = 0; component < result.Length; component += 1)
                {
                    result[component] = left.Value[component]
                        + (right.Value[component] - left.Value[component]) * position;
                }
                return result;
            }
            double position2 = position * position;
            double position3 = position2 * position;
            double h00 = 2 * position3 - 3 * position2 + 1;
            double h10 = position3 - 2 * position2 + position;
            double h01 = -2 * position3 + 3 * position2;
            double h11 = position3 - position2;
            for (int component = 0; component < result.Length; component += 1)
            {
                result[component] = h00 * left.Value[component]
                    + h10 * duration * left.LeaveTangent[component]
                    + h01 * right.Value[component]
                    + h11 * duration * right.ArriveTangent[component];
            }
            return result;
        }
        return keys[^1].Value;
    }

    private static JsonObject objectValue(JsonNode? node)
    {
        return node as JsonObject ?? new JsonObject();
    }

    private static string stringValue(JsonNode? node)
    {
        return node is JsonValue value && value.TryGetValue<string>(out string? result)
            ? result ?? string.Empty
            : string.Empty;
    }

    private static bool boolValue(JsonNode? node)
    {
        return node is JsonValue value && value.TryGetValue<bool>(out bool result) && result;
    }

    private static double numberValue(JsonNode? node, double fallback = 0)
    {
        return node is JsonValue value && value.TryGetValue<double>(out double result)
            ? result
            : fallback;
    }

    private static Color colourValue(JsonNode? node, Color fallback)
    {
        if (node is not JsonArray values || values.Count < 4)
            return fallback;
        return Color.FromArgb(
            byteValue(values[3], fallback.A),
            byteValue(values[0], fallback.R),
            byteValue(values[1], fallback.G),
            byteValue(values[2], fallback.B));
    }

    private static byte byteValue(JsonNode? node, byte fallback)
    {
        if (node is not JsonValue value || !value.TryGetValue<int>(out int result))
            return fallback;
        return (byte)Math.Clamp(result, 0, 255);
    }

    private sealed record PreviewVectorCurve(
        double[] DefaultValue,
        IReadOnlyList<PreviewVectorCurveKey> Keys);

    private sealed record PreviewVectorCurveKey(
        double Time,
        double[] Value,
        string Interpolation,
        double[] ArriveTangent,
        double[] LeaveTangent);

    private sealed record RichPreviewSegment(string Content, PreviewStyle Style);

    private sealed record RichPreviewLine(
        IReadOnlyList<RichPreviewSegment> Segments,
        PreviewStyle EndStyle);

    private sealed record RichRunVisual(Run Run, PreviewStyle Style);

    private sealed record RichLineVisual(
        TextBlock Block,
        IReadOnlyList<RichRunVisual> Runs,
        double Height);

    private enum PreviewLayer
    {
        Fill,
        Forced,
        Outline,
    }

    private sealed record PreviewStyle(
        double CharacterSize,
        bool Bold,
        bool Italic,
        bool Underlined,
        bool StrikeThrough,
        Color FillColor,
        double LetterSpacing,
        double LineSpacing,
        Color OutlineColor,
        double OutlineThickness)
    {
        public static PreviewStyle FromConfig(JsonObject data)
        {
            JsonObject source = stringValue(data["type"]) == "richTextConfig"
                ? objectValue(data["defaultStyle"])
                : data;
            JsonObject flags = objectValue(source["style"]);
            JsonObject outline = objectValue(source["outline"]);
            return new PreviewStyle(
                Math.Max(1, numberValue(source["characterSize"], 22)),
                boolValue(flags["bold"]),
                boolValue(flags["italic"]),
                boolValue(flags["underlined"]),
                boolValue(flags["strikeThrough"]),
                colourValue(source["fillColor"], Colors.White),
                numberValue(source["letterSpacing"], 1),
                numberValue(source["lineSpacing"], 1),
                colourValue(outline["color"], Colors.Black),
                Math.Max(0, numberValue(outline["thickness"])));
        }

        public PreviewStyle Adapt(JsonObject partial)
        {
            JsonObject flags = objectValue(partial["style"]);
            JsonObject outline = objectValue(partial["outline"]);
            return this with
            {
                CharacterSize = partial.ContainsKey("characterSize")
                    ? Math.Max(1, numberValue(partial["characterSize"], CharacterSize))
                    : CharacterSize,
                Bold = flags.ContainsKey("bold") ? boolValue(flags["bold"]) : Bold,
                Italic = flags.ContainsKey("italic") ? boolValue(flags["italic"]) : Italic,
                Underlined = flags.ContainsKey("underlined") ? boolValue(flags["underlined"]) : Underlined,
                StrikeThrough = flags.ContainsKey("strikeThrough")
                    ? boolValue(flags["strikeThrough"])
                    : StrikeThrough,
                FillColor = partial.ContainsKey("fillColor")
                    ? colourValue(partial["fillColor"], FillColor)
                    : FillColor,
                LetterSpacing = partial.ContainsKey("letterSpacing")
                    ? numberValue(partial["letterSpacing"], LetterSpacing)
                    : LetterSpacing,
                LineSpacing = partial.ContainsKey("lineSpacing")
                    ? numberValue(partial["lineSpacing"], LineSpacing)
                    : LineSpacing,
                OutlineColor = outline.ContainsKey("color")
                    ? colourValue(outline["color"], OutlineColor)
                    : OutlineColor,
                OutlineThickness = outline.ContainsKey("thickness")
                    ? Math.Max(0, numberValue(outline["thickness"], OutlineThickness))
                    : OutlineThickness,
            };
        }
    }
}
