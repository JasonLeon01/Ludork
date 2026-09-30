using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using Ludork.Controls;
using Ludork.Models;
using Ludork.Services;
using Ludork.Services.UiAssets;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Views;

public partial class UiAssetEditorWindow
{
    private static readonly HashSet<string> TextStylePropertyIds = new(StringComparer.Ordinal)
    {
        "font",
        "characterSize",
        "bold",
        "italic",
        "underlined",
        "strikeThrough",
        "slantAngle",
        "fillColor",
        "letterSpacing",
        "lineSpacing",
        "lineAlignment",
        "outlineColor",
        "outlineThickness",
        "glowEnabled",
        "glowColor",
        "glowRadius",
        "glowIntensity",
        "gradientEnabled",
        "gradientDirection",
        "gradientCurve",
    };
    private Action? pendingFieldCommit;
    private bool committingDetails;
    private bool detailsRefreshPending;
    private string? detailsSignature;
    private readonly List<Control> textStyleFields = [];

    private void refreshDetails(bool force = false)
    {
        JsonObject? node = selectedNodeName is null ? null : document.FindNode(selectedNodeName);
        bool isRoot = node is not null && document.FindParent(selectedNodeName!) is null;
        string signature = selectedNodeName + "\n" + (node is null ? string.Empty : string.Join("\n",
            node.Where(entry => entry.Key is not "children" and not "animations")
                .Select(entry => entry.Key + "=" + entry.Value?.ToJsonString())));
        if (isRoot)
            signature += "\n" + document.AssetKey + "\n" + document.Data["palette"]?.ToJsonString()
                + "\n" + document.Data["designSize"]?.ToJsonString();
        if (!force && signature == detailsSignature)
            return;
        detailsSignature = signature;
        if (committingDetails && !force)
        {
            updateTextStyleFields(node);
            return;
        }
        detailsRefreshPending = false;
        pendingFieldCommit = null;
        textStyleFields.Clear();
        DetailsPanel.Children.Clear();
        if (node is null)
            return;
        if (isRoot)
            addAssetDetails();
        if (!isRoot)
            addSlotDetails(node);
        addWidgetDetails(node);
    }

    private void onDetailsLostFocus(object? sender, RoutedEventArgs args)
    {
        Dispatcher.UIThread.Post(() =>
        {
            if (!closed && detailsRefreshPending && !DetailsPanel.IsKeyboardFocusWithin)
                refreshDetails(true);
        }, DispatcherPriority.Background);
    }

    private void addAssetDetails()
    {
        addSection(LocaleService.Get("UI_ASSET"));
        addReadOnlyTextField(
            LocaleService.Get("ASSET_PATH"),
            document.AssetKey);
        JsonObject? designSize = document.Data["designSize"] as JsonObject;
        double width = getDouble(designSize?["width"], 640);
        double height = getDouble(designSize?["height"], 480);
        addNumericField(
            LocaleService.Get("WIDTH"),
            width,
            1,
            32768,
            1,
            value => setDesignSize(value, getDesignHeight()));
        addNumericField(
            LocaleService.Get("HEIGHT"),
            height,
            1,
            32768,
            1,
            value => setDesignSize(getDesignWidth(), value));
        JsonObject? palette = document.Data["palette"] as JsonObject;
        bool exposed = getBool(palette?["exposed"], true);
        string displayName = getString(palette, "displayName", document.Title);
        string category = getString(palette, "category", "Project");
        addBoolField(
            LocaleService.Get("EXPOSE_TO_PALETTE"),
            exposed,
            value => setPalette(value, getPaletteDisplayName(), getPaletteCategory()));
        addTextField(
            LocaleService.Get("DISPLAY_NAME"),
            displayName,
            value => setPalette(getPaletteExposed(), value, getPaletteCategory()));
        addTextField(
            LocaleService.Get("CATEGORY"),
            category,
            value => setPalette(getPaletteExposed(), getPaletteDisplayName(), value));
    }

    private void setPalette(bool exposed, string displayName, string category)
    {
        if (document.SetPalette(exposed, displayName, category))
            refreshAll();
    }

    private void setDesignSize(double width, double height)
    {
        if (document.SetDesignSize(width, height))
            refreshAll();
    }

    private void addWidgetDetails(JsonObject node)
    {
        addSection(LocaleService.Get("WIDGET"));
        string nodeName = getString(node, "name");
        addTextField(
            LocaleService.Get("NAME"),
            nodeName,
            value => renameNode(nodeName, value));
        string controlId = getString(node, "controlId");
        addReadOnlyTextField(LocaleService.Get("CONTROL"), controlId);
        if (!controlLookup.TryGetValue(controlId, out UiControlDescriptor? descriptor))
            return;
        if (descriptor.AssetKey is not null)
        {
            addReadOnlyTextField(
                LocaleService.Get("UI_SOURCE_ASSET"),
                descriptor.AssetKey ?? string.Empty);
            Button open = new()
            {
                Content = LocaleService.Get("OPEN_SOURCE_ASSET"),
                HorizontalAlignment = HorizontalAlignment.Stretch,
            };
            open.Click += (_, _) =>
            {
                if (descriptor.AssetKey is not null)
                {
                    NestedAssetOpenRequested?.Invoke(
                        this,
                        UiAssetSchema.NormalizeAssetKey(descriptor.AssetKey));
                }
            };
            DetailsPanel.Children.Add(open);
            return;
        }
        JsonObject properties = node["properties"] as JsonObject ?? new JsonObject();
        JsonObject editor = node["editor"] as JsonObject ?? new JsonObject();
        bool rootCanvas = document.FindParent(nodeName) is null
            && string.Equals(
                descriptor.ControlId,
                "Engine.Canvas",
                StringComparison.Ordinal);
        foreach (UiControlPropertyDescriptor property in descriptor.Properties)
        {
            if (rootCanvas && string.Equals(property.Id, "size", StringComparison.Ordinal))
                continue;
            JsonObject source = property.EditorOnly ? editor : properties;
            JsonNode? value = source[property.Id] ?? property.Default;
            int fieldIndex = DetailsPanel.Children.Count;
            addPropertyField(nodeName, descriptor.ControlId, property, value);
            if (TextStylePropertyIds.Contains(property.Id)
                && DetailsPanel.Children.Count > fieldIndex)
            {
                textStyleFields.Add(DetailsPanel.Children[fieldIndex]);
            }
        }
        updateTextStyleFields(node);
    }

    private void updateTextStyleFields(JsonObject? node)
    {
        bool usesTextConfig = !string.IsNullOrEmpty(getString(node?["properties"]?["textConfig"]));
        foreach (Control field in textStyleFields)
            field.IsEnabled = !usesTextConfig;
    }

    private void renameNode(string nodeName, string value)
    {
        if (refreshing)
            return;
        string nextName = value.Trim();
        if (string.Equals(nodeName, nextName, StringComparison.Ordinal))
            return;
        bool renamed;
        bool wasRefreshing = refreshing;
        refreshing = true;
        try
        {
            renamed = document.RenameNode(nodeName, value);
        }
        finally
        {
            refreshing = wasRefreshing;
        }
        if (!renamed)
        {
            setStatus(LocaleService.Get("UI_NAME_MUST_BE_UNIQUE"));
            refreshDetails(true);
            return;
        }
        if (string.Equals(selectedNodeName, nodeName, StringComparison.Ordinal))
            selectedNodeName = nextName;
        refreshDetails(true);
        refreshAll();
    }

    private void addPropertyField(
        string nodeName,
        string controlId,
        UiControlPropertyDescriptor property,
        JsonNode? value)
    {
        Action<JsonNode?> commit = nextValue =>
        {
            if (property.EditorOnly)
                document.SetNodeEditorProperty(nodeName, property.Id, nextValue);
            else
                document.SetNodeProperty(nodeName, property.Id, nextValue);
        };
        switch (property.Type)
        {
            case "string" when property.Id == "particle":
                addChoiceField(
                    property.DisplayName,
                    getString(value),
                    gameData.Assets.ParticlesData.Keys.Prepend(string.Empty).OrderBy(key => key, StringComparer.Ordinal).ToArray(),
                    next => commit(JsonValue.Create(next)));
                break;
            case "sf.Text.LineAlignment":
                addChoiceField(
                    property.DisplayName,
                    getString(value),
                    ["default", "left", "center", "right"],
                    next => commit(JsonValue.Create(next)));
                break;
            case "Engine.TextGradientDirection":
                addChoiceField(
                    property.DisplayName,
                    getString(value),
                    ["vertical", "horizontal"],
                    next => commit(JsonValue.Create(next)));
                break;
            case "Engine.ImageDrawAs":
                addChoiceField(
                    property.DisplayName,
                    getString(value),
                    ["Image", "Tile"],
                    next => commit(JsonValue.Create(next)));
                break;
            case "string" when controlId == "Engine.Button" && property.Id == "gamepadButton":
                addChoiceField(
                    property.DisplayName,
                    getString(value),
                    ["", "A", "B", "X", "Y", "LB", "RB", "View", "Menu", "LS", "RS", "XBox", "Share"],
                    next => commit(JsonValue.Create(next)));
                break;
            case "string" when property.Id == "font":
                addFontField(
                    property.DisplayName,
                    getString(value),
                    next => commit(JsonValue.Create(next)));
                break;
            case "string" when property.Id is "texture"
                or "backgroundTexture"
                or "fillTexture"
                or "windowSkin"
                or "lineTexture"
                or "handleTexture":
                addTextureField(
                    property.DisplayName,
                    getString(value),
                    next => commit(JsonValue.Create(next)));
                break;
            case "string" when property.Id == "shader":
                addShaderField(
                    property.DisplayName,
                    getString(value),
                    next => commit(JsonValue.Create(next)));
                break;
            case "bool":
                addBoolField(
                    property.DisplayName,
                    getBool(value, false),
                    next => commit(JsonValue.Create(next)));
                break;
            case "int":
                addNumericField(
                    property.DisplayName,
                    getDouble(value, 0),
                    controlId == "Engine.WrapBox" && property.Id == "count" ? 0 : -2147483648,
                    2147483647,
                    1,
                    next => commit(JsonValue.Create((int)Math.Round(next))));
                break;
            case "float":
                addNumericField(
                    property.DisplayName,
                    getDouble(value, 0),
                    -1000000000,
                    1000000000,
                    0.1,
                    next => commit(JsonValue.Create(next)));
                break;
            case "sf.Vector2f":
                addNumberArrayField(property.DisplayName, value, 2, false, commit);
                break;
            case "sf.Vector2u":
                double? unsignedMaximum =
                    string.Equals(property.Id, "size", StringComparison.Ordinal)
                    && (string.Equals(
                            controlId,
                            "Engine.Canvas",
                            StringComparison.Ordinal)
                        || string.Equals(
                            controlId,
                            "Engine.Window",
                            StringComparison.Ordinal))
                        ? int.MaxValue
                        : null;
                addNumberArrayField(
                    property.DisplayName,
                    value,
                    2,
                    true,
                    commit,
                    false,
                    unsignedMaximum);
                break;
            case "sf.IntRect":
                addNumberArrayField(property.DisplayName, value, 4, false, commit, true);
                break;
            case "sf.Color":
                addColorField(property.DisplayName, value, commit);
                break;
            case "string[]":
                addStringArrayField(property.DisplayName, value, commit);
                break;
            default:
                addTextField(
                    property.DisplayName,
                    getString(value),
                    next => commit(JsonValue.Create(next)));
                break;
        }
    }

    private void addSlotDetails(JsonObject node)
    {
        JsonObject? parent = document.FindParent(getString(node, "name"));
        if (parent is null
            || !controlLookup.TryGetValue(
                getString(parent, "controlId"),
                out UiControlDescriptor? parentDescriptor))
        {
            return;
        }
        addSection(LocaleService.Get("SLOT"));
        if (string.Equals(parentDescriptor.SlotType, "list", StringComparison.Ordinal))
        {
            DetailsPanel.Children.Add(new TextBlock
            {
                Text = LocaleService.Get("UI_LIST_SLOT_ORDERED"),
                Foreground = EditorTheme.Brush("TextMuted"),
                TextWrapping = TextWrapping.Wrap,
            });
            return;
        }
        if (!string.Equals(parentDescriptor.SlotType, "canvas", StringComparison.Ordinal))
            return;
        JsonObject slot = node["slot"] as JsonObject
            ?? UiAssetEditingService.CreateDefaultCanvasSlot();
        addCanvasSlotFields(getString(node, "name"), slot);
    }

    private void addCanvasSlotFields(string nodeName, JsonObject slot)
    {
        JsonObject anchors = slot["anchors"] as JsonObject ?? new JsonObject();
        JsonArray min = anchors["min"] as JsonArray ?? new JsonArray(0, 0);
        JsonArray max = anchors["max"] as JsonArray ?? new JsonArray(0, 0);
        JsonObject offsets = slot["offsets"] as JsonObject ?? new JsonObject();
        JsonArray alignment = slot["alignment"] as JsonArray ?? new JsonArray(0, 0);
        double minimumX = getDouble(min[0], 0);
        double minimumY = getDouble(min[1], 0);
        double maximumX = getDouble(max[0], 0);
        double maximumY = getDouble(max[1], 0);
        addAnchorPicker(
            nodeName,
            minimumX,
            minimumY,
            maximumX,
            maximumY);
        bool stretchesX = Math.Abs(maximumX - minimumX) > 0.0001;
        bool stretchesY = Math.Abs(maximumY - minimumY) > 0.0001;
        addSlotOffsetField(
            nodeName,
            stretchesX ? "OFFSET_LEFT" : "POSITION_X",
            "left",
            getDouble(offsets["left"], 0));
        addSlotOffsetField(
            nodeName,
            stretchesY ? "OFFSET_TOP" : "POSITION_Y",
            "top",
            getDouble(offsets["top"], 0));
        addSlotOffsetField(
            nodeName,
            stretchesX ? "OFFSET_RIGHT" : "SIZE_X",
            "right",
            getDouble(offsets["right"], 100));
        addSlotOffsetField(
            nodeName,
            stretchesY ? "OFFSET_BOTTOM" : "SIZE_Y",
            "bottom",
            getDouble(offsets["bottom"], 34));
        addAxisPointField(
            LocaleService.Get("ALIGNMENT"),
            getDouble(alignment[0], 0),
            getDouble(alignment[1], 0),
            0,
            1,
            (x, y) => updateSlotPoint(nodeName, null, "alignment", x, y));
        addBoolField(
            LocaleService.Get("AUTO_SIZE"),
            getBool(slot["autoSize"], false),
            value => updateSlotScalar(nodeName, "autoSize", JsonValue.Create(value)));
        addNumericField(
            LocaleService.Get("Z_ORDER"),
            getDouble(slot["zOrder"], 0),
            int.MinValue,
            int.MaxValue,
            1,
            value => updateSlotScalar(
                nodeName,
                "zOrder",
                JsonValue.Create((int)Math.Round(value))));
    }

    private void addAnchorPicker(
        string nodeName,
        double minimumX,
        double minimumY,
        double maximumX,
        double maximumY)
    {
        CanvasAnchorPresetPicker picker = new(
            minimumX,
            minimumY,
            maximumX,
            maximumY);
        picker.PresetSelected += preset => setAnchorPreset(nodeName, preset);
        DetailsPanel.Children.Add(createField(
            LocaleService.Get("ANCHORS"),
            picker));
    }

    private void addSlotOffsetField(
        string nodeName,
        string localeKey,
        string offsetName,
        double value)
    {
        addNumericField(
            LocaleService.Get(localeKey),
            value,
            -1000000000,
            1000000000,
            1,
            next => updateSlotOffset(nodeName, offsetName, next));
    }

    private void setAnchorPreset(
        string nodeName,
        CanvasAnchorPreset preset)
    {
        JsonObject slot = cloneSlot(nodeName);
        slot["anchors"] = new JsonObject
        {
            ["min"] = new JsonArray(preset.MinimumX, preset.MinimumY),
            ["max"] = new JsonArray(preset.MaximumX, preset.MaximumY),
        };
        slot["alignment"] = new JsonArray(
            preset.AlignmentX,
            preset.AlignmentY);
        document.SetNodeSlot(nodeName, slot);
    }

    private void updateSlotPoint(
        string nodeName,
        string? group,
        string name,
        double x,
        double y)
    {
        JsonObject slot = cloneSlot(nodeName);
        if (group is null)
        {
            slot[name] = new JsonArray(x, y);
        }
        else
        {
            JsonObject groupValue = slot[group] as JsonObject ?? new JsonObject();
            groupValue[name] = new JsonArray(x, y);
            slot[group] = groupValue;
        }
        document.SetNodeSlot(nodeName, slot);
    }

    private void updateSlotOffset(
        string nodeName,
        string name,
        double value)
    {
        JsonObject slot = cloneSlot(nodeName);
        JsonObject offsets = slot["offsets"] as JsonObject ?? new JsonObject();
        offsets[name] = value;
        slot["offsets"] = offsets;
        document.SetNodeSlot(nodeName, slot);
    }

    private void updateSlotScalar(string nodeName, string name, JsonNode value)
    {
        JsonObject slot = cloneSlot(nodeName);
        slot[name] = value;
        document.SetNodeSlot(nodeName, slot);
    }

    private JsonObject cloneSlot(string nodeName)
    {
        JsonObject? node = document.FindNode(nodeName);
        return node?["slot"] is JsonObject slot
            ? (JsonObject)slot.DeepClone()
            : UiAssetEditingService.CreateDefaultCanvasSlot();
    }

    private void addSection(string label)
    {
        if (DetailsPanel.Children.Count != 0)
            DetailsPanel.Children.Add(new Separator { Margin = new Thickness(0, 6) });
        DetailsPanel.Children.Add(new TextBlock
        {
            Text = label,
            FontWeight = FontWeight.SemiBold,
            FontSize = 13,
            Margin = new Thickness(0, 2, 0, 3),
        });
    }

    private void addReadOnlyTextField(string label, string value)
    {
        TextBox box = EditorInputs.CreateReadOnlyTextBox(value);
        DetailsPanel.Children.Add(createField(label, box));
    }

    private void addTextField(
        string label,
        string value,
        Action<string> commit)
    {
        TextBox box = EditorInputs.CreateEditableTextBox(value);
        bindTextField(box, commit);
        DetailsPanel.Children.Add(createField(label, box));
    }

    private void bindTextField(TextBox box, Action<string> commit)
    {
        string displayed = box.Text ?? string.Empty;
        Action commitValue = () =>
        {
            string next = box.Text ?? string.Empty;
            if (string.Equals(displayed, next, StringComparison.Ordinal))
                return;
            displayed = next;
            commitDetailsField(() => commit(next));
        };
        box.GotFocus += (_, _) => pendingFieldCommit = commitValue;
        box.LostFocus += (_, _) =>
        {
            if (!ReferenceEquals(pendingFieldCommit, commitValue))
                return;
            pendingFieldCommit = null;
            commitValue();
        };
        box.KeyDown += (_, args) =>
        {
            if (args.Key != Key.Enter || box.AcceptsReturn)
                return;
            if (!ReferenceEquals(pendingFieldCommit, commitValue))
                return;
            commitValue();
            args.Handled = true;
        };
    }

    private void addChoiceField(
        string label,
        string value,
        IReadOnlyList<string> choices,
        Action<string> commit)
    {
        ComboBox input = new()
        {
            ItemsSource = choices.Select(choice => choice.Length == 0
                ? LocaleService.Get("UI_GAMEPAD_UNBOUND") : choice).ToArray(),
            SelectedIndex = Math.Max(0, choices.ToList().IndexOf(value)),
            HorizontalAlignment = HorizontalAlignment.Stretch,
        };
        input.SelectionChanged += (_, _) =>
        {
            if (refreshing || input.SelectedIndex < 0 || input.SelectedIndex >= choices.Count)
                return;
            commit(choices[input.SelectedIndex]);
        };
        DetailsPanel.Children.Add(createField(label, input));
    }

    private void addStringArrayField(
        string label,
        JsonNode? value,
        Action<JsonNode?> commit)
    {
        IEnumerable<string> items = value is JsonArray array
            ? array.Select(getString)
            : [];
        TextBox box = EditorInputs.CreateEditableTextBox(string.Join(Environment.NewLine, items));
        box.AcceptsReturn = true;
        box.MinHeight = 96;
        box.TextWrapping = TextWrapping.Wrap;
        bindTextField(box, next =>
        {
            JsonArray result = new();
            foreach (string item in next.Length == 0 ? Array.Empty<string>() : next.Split(
                         ["\r\n", "\n", "\r"],
                         StringSplitOptions.None))
            {
                result.Add(item);
            }
            commit(result);
        });
        DetailsPanel.Children.Add(createField(label, box));
    }

    private void addTextureField(
        string label,
        string value,
        Action<string> commit)
    {
        addAssetFileField(
            label,
            value,
            Path.Combine(gameData.ProjectPath, "Assets"),
            FileSelectorDialog.ImageFilesFilter(),
            commit);
    }

    private void addFontField(
        string label,
        string value,
        Action<string> commit)
    {
        addAssetFileField(
            label,
            value,
            Path.Combine(gameData.ProjectPath, "Assets", "Fonts"),
            FileSelectorDialog.FilesFilter("*.ttf", "*.otf"),
            commit);
    }

    private void addShaderField(
        string label,
        string value,
        Action<string> commit)
    {
        addAssetFileField(
            label,
            value,
            Path.Combine(gameData.ProjectPath, "Assets", "Shaders"),
            FileSelectorDialog.FilesFilter("*.vert", "*.frag", "*.geom"),
            commit);
    }

    private void addAssetFileField(
        string label,
        string value,
        string selectorRoot,
        string filter,
        Action<string> commit)
    {
        TextBox pathBox = EditorInputs.CreateReadOnlyTextBox(value);
        Button browse = new()
        {
            Content = "...",
            MinWidth = 36,
            Height = EditorInputs.FieldMinHeight,
        };
        browse.Click += async (_, _) =>
        {
            Directory.CreateDirectory(selectorRoot);
            string? initialFilePath = getAssetInitialFilePath(pathBox.Text ?? string.Empty);
            string? selectedPath = await FileSelectorDialog.ShowAsync(
                this,
                selectorRoot,
                filter,
                initialDirectory: Path.GetDirectoryName(initialFilePath),
                initialFilePath: initialFilePath);
            if (selectedPath is null)
                return;
            if (!GameAssetPath.TryFromProjectFile(
                    gameData.ProjectPath,
                    selectedPath,
                    out string assetPath))
            {
                return;
            }
            pathBox.Text = assetPath;
            commit(assetPath);
        };
        ToolTip.SetTip(browse, LocaleService.Get("BROWSE"));
        Button clear = new()
        {
            Content = LocaleService.Get("CLEAR"),
            Height = EditorInputs.FieldMinHeight,
        };
        clear.Click += (_, _) =>
        {
            if (string.IsNullOrEmpty(pathBox.Text))
                return;
            pathBox.Text = string.Empty;
            commit(string.Empty);
        };
        Grid row = new()
        {
            ColumnDefinitions = new ColumnDefinitions("*,Auto,Auto"),
            ColumnSpacing = 6,
        };
        row.Children.Add(pathBox);
        Grid.SetColumn(browse, 1);
        row.Children.Add(browse);
        Grid.SetColumn(clear, 2);
        row.Children.Add(clear);
        DetailsPanel.Children.Add(createField(label, row));
    }

    private string? getAssetInitialFilePath(string value)
    {
        return GameAssetPath.TryResolveExistingFile(gameData.ProjectPath, value, out string path)
            ? path
            : null;
    }

    private void addNumericField(
        string label,
        double value,
        double minimum,
        double maximum,
        double increment,
        Action<double> commit)
    {
        NumericUpDown box = EditorInputs.CreateNumericUpDown(
            (decimal)Math.Clamp(value, minimum, maximum),
            (decimal)minimum,
            (decimal)maximum,
            (decimal)increment);
        box.ValueChanged += (_, _) =>
        {
            if (!refreshing && box.Value is decimal number)
                commitDetailsField(() => commit((double)number));
        };
        DetailsPanel.Children.Add(createField(label, box));
    }

    private void addBoolField(
        string label,
        bool value,
        Action<bool> commit)
    {
        CheckBox box = new()
        {
            IsChecked = value,
            VerticalAlignment = VerticalAlignment.Center,
            HorizontalAlignment = HorizontalAlignment.Left,
        };
        box.IsCheckedChanged += (_, _) =>
        {
            if (!refreshing)
                commit(box.IsChecked == true);
        };
        DetailsPanel.Children.Add(createField(label, box));
    }

    private void addNumberArrayField(
        string label,
        JsonNode? value,
        int count,
        bool unsigned,
        Action<JsonNode?> commit,
        bool integral = false,
        double? explicitMaximum = null)
    {
        double[] values = readArray(value, count);
        addNumberArrayField(
            label,
            values,
            unsigned,
            integral,
            next =>
            {
                JsonArray result = new();
                foreach (double item in next)
                {
                    if (unsigned)
                        result.Add(JsonValue.Create((long)Math.Round(item)));
                    else if (integral)
                        result.Add(JsonValue.Create((int)Math.Round(item)));
                    else
                        result.Add(JsonValue.Create(item));
                }
                commit(result);
            },
            null,
            explicitMaximum);
    }

    private void addAxisPointField(
        string label,
        double x,
        double y,
        double minimum,
        double maximum,
        Action<double, double> commit)
    {
        double[] values = [x, y];
        addNumberArrayField(
            label,
            values,
            false,
            false,
            next => commit(
                Math.Clamp(next[0], minimum, maximum),
                Math.Clamp(next[1], minimum, maximum)),
            minimum,
            maximum,
            0.01,
            ["X", "Y"]);
    }

    private void addNumberArrayField(
        string label,
        double[] values,
        bool unsigned,
        bool integral,
        Action<double[]> commit,
        double? explicitMinimum = null,
        double? explicitMaximum = null,
        double? explicitIncrement = null,
        IReadOnlyList<string>? componentLabels = null)
    {
        Grid grid = new()
        {
            ColumnDefinitions = new ColumnDefinitions(
                string.Join(',', Enumerable.Repeat("*", values.Length))),
            ColumnSpacing = 4,
        };
        NumericUpDown[] boxes = new NumericUpDown[values.Length];
        double minimum = explicitMinimum ?? (unsigned ? 0 : -1000000000);
        double maximum = explicitMaximum ?? (unsigned ? uint.MaxValue : 1000000000);
        double increment = explicitIncrement ?? (integral || unsigned ? 1 : 0.1);
        for (int index = 0; index < boxes.Length; index++)
        {
            NumericUpDown box = EditorInputs.CreateNumericUpDown(
                (decimal)Math.Clamp(values[index], minimum, maximum),
                (decimal)minimum,
                (decimal)maximum,
                (decimal)increment);
            int valueIndex = index;
            box.ValueChanged += (_, _) =>
            {
                if (refreshing || box.Value is not decimal number)
                    return;
                double[] next = boxes
                    .Select(candidate => (double)(candidate.Value ?? 0))
                    .ToArray();
                next[valueIndex] = (double)number;
                commitDetailsField(() => commit(next));
            };
            boxes[index] = box;
            Control editor = box;
            if (componentLabels is not null
                && index < componentLabels.Count)
            {
                Grid component = new()
                {
                    ColumnDefinitions = new ColumnDefinitions("Auto,*"),
                    ColumnSpacing = 4,
                };
                TextBlock componentLabel = new()
                {
                    Text = componentLabels[index],
                    Foreground = EditorTheme.Brush("TextMuted"),
                    VerticalAlignment = VerticalAlignment.Center,
                };
                Grid.SetColumn(box, 1);
                component.Children.Add(componentLabel);
                component.Children.Add(box);
                editor = component;
            }
            Grid.SetColumn(editor, index);
            grid.Children.Add(editor);
        }
        DetailsPanel.Children.Add(createField(label, grid));
    }

    private void addColorField(
        string label,
        JsonNode? value,
        Action<JsonNode?> commit)
    {
        double[] channels = readArray(value, 4);
        BlueprintColourSwatch swatch = new(Color.FromArgb(
            toByte(channels[3], 255),
            toByte(channels[0], 255),
            toByte(channels[1], 255),
            toByte(channels[2], 255)));
        swatch.Click += async (_, _) =>
        {
            Color? result = await ColourPickerWindow.ShowAsync(this, swatch.Colour);
            if (result is not Color colour)
                return;
            swatch.Colour = colour;
            commit(new JsonArray(
                (int)colour.R,
                (int)colour.G,
                (int)colour.B,
                (int)colour.A));
        };
        DetailsPanel.Children.Add(createField(label, swatch));
    }

    private static Grid createField(string label, Control editor)
    {
        Grid result = new()
        {
            ColumnDefinitions = new ColumnDefinitions("118,*"),
            ColumnSpacing = 8,
        };
        TextBlock caption = new()
        {
            Text = label,
            TextWrapping = TextWrapping.Wrap,
            VerticalAlignment = VerticalAlignment.Center,
        };
        Grid.SetColumn(editor, 1);
        result.Children.Add(caption);
        result.Children.Add(editor);
        return result;
    }

    private void flushPendingField()
    {
        pendingFieldCommit?.Invoke();
        if (contentInitialized)
            timelineEditor.FlushPendingChanges();
    }

    private void commitDetailsField(Action commit)
    {
        bool wasCommitting = committingDetails;
        committingDetails = true;
        detailsRefreshPending = true;
        try
        {
            commit();
        }
        finally
        {
            committingDetails = wasCommitting;
        }
    }

    private double getDesignWidth()
    {
        return getDouble((document.Data["designSize"] as JsonObject)?["width"], 640);
    }

    private double getDesignHeight()
    {
        return getDouble((document.Data["designSize"] as JsonObject)?["height"], 480);
    }

    private bool getPaletteExposed()
    {
        return getBool((document.Data["palette"] as JsonObject)?["exposed"], true);
    }

    private string getPaletteDisplayName()
    {
        return getString(document.Data["palette"] as JsonObject, "displayName", document.Title);
    }

    private string getPaletteCategory()
    {
        return getString(document.Data["palette"] as JsonObject, "category", "Project");
    }
}
