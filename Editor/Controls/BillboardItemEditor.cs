using Avalonia;
using Avalonia.Controls;
using Ludork.Models;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

internal sealed class BillboardItemEditor : UserControl
{
    public BillboardItemEditor(
        BlueprintVariableEditorRequest request,
        string assetsDirectory,
        int cellSize,
        IGameVariableCatalog? gameVariables,
        bool readOnly)
    {
        JsonObject current = request.Value?.DeepClone() as JsonObject ?? [];
        TextBox summary = EditorInputs.CreateReadOnlyTextBox(formatSummary(current));
        Button edit = new()
        {
            Content = "...",
            Width = 24,
            MinWidth = 24,
            MinHeight = EditorInputs.FieldMinHeight,
            Padding = new Thickness(0),
            IsEnabled = !readOnly,
        };
        Grid row = new()
        {
            ColumnDefinitions = new ColumnDefinitions("*,Auto"),
            ColumnSpacing = 4,
            Children = { summary, edit },
        };
        Grid.SetColumn(edit, 1);
        Content = row;
        edit.Click += async (_, _) =>
        {
            if (TopLevel.GetTopLevel(this) is not Window owner)
                return;
            JsonObject editing = (JsonObject)current.DeepClone();
            if (editing.TryGetPropertyValue("image", out JsonNode? path))
            {
                editing["path"] = path?.DeepClone();
                editing.Remove("image");
            }
            IReadOnlyList<BlueprintVariableField> createFields(JsonObject value) =>
                createItemFields(request.Field.Fields, value);
            JsonObject? selected = await BlueprintStructureWindow.ShowAsync(
                owner,
                EditorDisplayName.Format("BillboardItem"),
                createFields(editing),
                editing,
                assetsDirectory,
                cellSize,
                gameVariables,
                readOnly,
                createFields);
            if (selected is null || TopLevel.GetTopLevel(this) != owner || !IsEffectivelyEnabled)
                return;
            if (selected.TryGetPropertyValue("path", out JsonNode? selectedPath))
            {
                selected["image"] = selectedPath?.DeepClone();
                selected.Remove("path");
            }
            current = (JsonObject)selected.DeepClone();
            summary.Text = formatSummary(current);
            request.Commit(current, false);
        };
    }

    public static bool Supports(BlueprintVariableField field)
    {
        return string.Equals(field.Module, "Engine", StringComparison.Ordinal)
            && string.Equals(field.TypeName, "BillboardItem", StringComparison.Ordinal);
    }

    private static string formatSummary(JsonObject value)
    {
        bool image = string.Equals(value["kind"]?.GetValue<string>(), "image", StringComparison.Ordinal);
        string content = value[image ? "image" : "text"]?.GetValue<string>() ?? string.Empty;
        content = content.Replace('\r', ' ').Replace('\n', ' ').Replace('\t', ' ');
        return image ? $"kind: image, path: {content}" : $"kind: text, text: {content}";
    }

    private static IReadOnlyList<BlueprintVariableField> createItemFields(
        IReadOnlyList<BlueprintVariableField> definitions,
        JsonObject value)
    {
        bool image = string.Equals(value["kind"]?.GetValue<string>(), "image", StringComparison.Ordinal);
        string[] names = image ? ["kind", "image"] : ["kind", "text", "fontSize", "color"];
        List<BlueprintVariableField> fields = [];
        foreach (string name in names)
        {
            BlueprintVariableField definition = definitions.Single(field => field.Name == name);
            BlueprintVariableField field = definition.Clone(name == "image" ? "path" : name);
            field.Value = value.TryGetPropertyValue(field.Name, out JsonNode? current)
                ? current?.DeepClone()
                : (definition.Value ?? definition.DefaultValue)?.DeepClone();
            field.DisplayValue = null;
            field.PreserveNullValue = field.Value is null;
            fields.Add(field);
        }
        return fields;
    }
}
