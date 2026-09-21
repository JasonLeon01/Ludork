using Ludork.Models;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Controls;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

namespace Ludork.Views;

public sealed class TextConfigEditorWindow : Window
{
    private readonly ProjectDataStore gameData;
    private readonly ProjectSaveService projectSave;
    private readonly EditorDocument? resourceDocument;
    private readonly EditorDocumentBinding documentBinding;
    private string key => resourceDocument?.Key ?? string.Empty;
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
    private JsonObject sourceData;
    private JsonObject displayBaseline = [];
    private bool syncing;

    public TextConfigEditorWindow(
        ProjectDataStore gameData,
        ProjectSaveService projectSave,
        string key,
        JsonObject data)
    {
        this.gameData = gameData;
        this.projectSave = projectSave;
        resourceDocument = gameData.GetDocument("TextConfigs", key);
        sourceData = (JsonObject)data.DeepClone();
        this.data = (JsonObject)data.DeepClone();
        normalizeData();
        Title = $"{LocaleService.Get("TEXT_CONFIG_EDITOR")} - {key}";
        Width = 1240;
        Height = 820;
        MinWidth = 960;
        MinHeight = 640;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = Ludork.Services.EditorTheme.Brush("Background");
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
        displayBaseline = (JsonObject)this.data.DeepClone();
        refreshPreview();
        AddHandler(KeyDownEvent, onKeyDown, RoutingStrategies.Tunnel);
        gameData.Documents.ContentChanged += onDataChanged;
        documentBinding = new EditorDocumentBinding(this, gameData, () => resourceDocument,
            () => $"{LocaleService.Get("TEXT_CONFIG_EDITOR")} - {this.key}", synchronizeDocument, closeWhenDeleted: true);
        Closed += (_, _) =>
        {
            gameData.Documents.ContentChanged -= onDataChanged;

        };
    }

    public void Reload(JsonObject nextData)
    {
        sourceData = (JsonObject)nextData.DeepClone();
        data = (JsonObject)nextData.DeepClone();
        normalizeData();
        rebuildInspector();
        displayBaseline = (JsonObject)data.DeepClone();
        refreshPreview();
    }

    private Control buildLayout()
    {
        Border inspectorBorder = new()
        {
            Background = new SolidColorBrush(Color.Parse("#1c1c1c")),
            BorderBrush = Ludork.Services.EditorTheme.Brush("Border"),
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
        input.PropertyChanged += (_, args) =>
        {
            if (args.Property != TextBox.TextProperty || syncing)
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
        input.PropertyChanged += (_, args) =>
        {
            if (args.Property != TextBox.TextProperty || syncing)
                return;
            target[field] = kind == TextConfigReferenceKind.Font
                ? input.Text ?? string.Empty
                : normalizeReference(input.Text);
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
            BorderBrush = EditorTheme.Brush("TextMuted"),
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
                gameData.Assets.CurvesData
                    .Where(item => item.Value.Type == "vector4Curve")
                    .Select(item => item.Key)
                    .OrderBy(item => item, StringComparer.Ordinal),
                current);
        }
        string root = Path.Combine(gameData.ProjectPath, "Assets", "Fonts");
        Directory.CreateDirectory(root);
        string filter = FileSelectorDialog.FilesFilter("*.ttf", "*.otf");
        string? initialFilePath = GameAssetPath.TryResolveExistingFile(
            gameData.ProjectPath,
            current,
            out string resolvedCurrent)
            ? resolvedCurrent
            : null;
        string? path = await FileSelectorDialog.ShowAsync(
            this,
            root,
            filter,
            LocaleService.Get("TEXT_CONFIG_FONT"),
            initialFilePath: initialFilePath);
        return path is not null
            && GameAssetPath.TryFromProjectFile(gameData.ProjectPath, path, out string assetPath)
                ? assetPath
                : null;
    }

    private void applyChanges()
    {
        if (syncing)
            return;
        refreshPreview();
        IReadOnlyList<string> errors = getReferenceErrors();
        updateValidation(errors);
        if (errors.Count != 0)
            return;
        if (!gameData.Assets.TextConfigsData.ContainsKey(key))
        {
            Close();
            return;
        }
        JsonObject changed = resourceDocument?.Data ?? (JsonObject)sourceData.DeepClone();
        applyEditedFields(changed, displayBaseline, data);
        if (JsonNode.DeepEquals(sourceData, changed))
            return;
        sourceData = changed;
        displayBaseline = (JsonObject)data.DeepClone();
        gameData.Assets.UpdateTextConfig(key, sourceData);
    }

    private static void applyEditedFields(JsonObject target, JsonObject before, JsonObject after)
    {
        foreach (string name in before.Select(item => item.Key).Union(after.Select(item => item.Key)))
        {
            bool wasPresent = before.TryGetPropertyValue(name, out JsonNode? previous);
            bool isPresent = after.TryGetPropertyValue(name, out JsonNode? current);
            if (wasPresent == isPresent && JsonNode.DeepEquals(previous, current))
                continue;
            if (!isPresent)
            {
                target.Remove(name);
            }
            else if (previous is JsonObject oldObject && current is JsonObject newObject)
            {
                JsonObject result = target[name]?.DeepClone() as JsonObject ?? [];
                applyEditedFields(result, oldObject, newObject);
                target[name] = result;
            }
            else
            {
                target[name] = current?.DeepClone();
            }
        }
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
            && (!gameData.Assets.CurvesData.TryGetValue(curve, out CurveSnapshot? curveData)
                || curveData.Type != "vector4Curve"))
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
                if (!gameData.Assets.TextConfigsData.ContainsKey(key))
                {
                    await AlertDialog.ShowAsync(
                        this,
                        LocaleService.Get("ERROR"),
                        LocaleService.Get("TEXT_CONFIG_NO_LONGER_EXISTS"));
                    Close();
                    args.Handled = true;
                    return;
                }
                applyChanges();
                await EditorSaveWorkflow.TrySaveAsync(this, projectSave);
            }
        }
        else if (args.Key == Key.Z)
            EditorFeedback.ShowHistory(toast, "Undo", documentBinding.Undo());
        else if (args.Key == Key.Y)
            EditorFeedback.ShowHistory(toast, "Redo", documentBinding.Redo());
        else
            return;
        args.Handled = true;
    }

    private void onDataChanged(object? sender, EditorDocumentsChangedEventArgs args)
    {
        string curve = stringValue(data["gradient"]?["curve"]);
        if (!args.Reset && !args.Changes.Any(change => change.Section == "Curves"
                && (change.PreviousKey == curve || change.Key == curve)))
            return;
        if (!gameData.Assets.TextConfigsData.ContainsKey(key))
        {
            Close();
            return;
        }
        updateValidation();
        refreshPreview();
    }

    private void synchronizeDocument()
    {
        if (resourceDocument?.Data is not JsonObject current)
        {
            Close();
            return;
        }
        if (!JsonNode.DeepEquals(current, sourceData))
            Reload(current);
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
