using Avalonia;
using Avalonia.Controls;
using Ludork.Controls;
using Ludork.Services;
using System;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Views.Utils;

public sealed class BlueprintNodeParameterEditorFactory
{
    private readonly GameDataService gameData;
    private readonly LuaMetadataService metadataService;
    private readonly BlueprintClassResolver classResolver;

    public BlueprintNodeParameterEditorFactory(
        GameDataService gameData,
        LuaMetadataService metadataService,
        BlueprintClassResolver classResolver)
    {
        this.gameData = gameData;
        this.metadataService = metadataService;
        this.classResolver = classResolver;
    }

    public Control? Create(
        BlueprintVariableEditorRequest request,
        Func<string, JsonNode?> getRawSiblingValue,
        Action<string, JsonNode?> setRawSiblingValue,
        Func<bool> isAlive)
    {
        return request.Field.EditorKind switch
        {
            BlueprintVariableEditorKind.MoveRoute => createMoveRouteEditor(request, isAlive),
            BlueprintVariableEditorKind.TransferPosition => createTransferPositionEditor(
                request,
                getRawSiblingValue,
                setRawSiblingValue,
                isAlive),
            BlueprintVariableEditorKind.BlueprintClass => createBlueprintClassEditor(request, isAlive),
            BlueprintVariableEditorKind.CommonFunction => createCommonFunctionEditor(request, isAlive),
            _ => null,
        };
    }

    private Control createMoveRouteEditor(BlueprintVariableEditorRequest request, Func<bool> isAlive)
    {
        JsonArray current = BlueprintNodeParameterValues.NormalizeRoute(request.Value);
        TextBox summary = EditorInputs.CreateReadOnlyTextBox(
            BlueprintNodeParameterValues.FormatRoute(current));
        Button editButton = new()
        {
            Content = LocaleService.Get("MOVE_ROUTE_EDIT"),
            MinHeight = EditorInputs.FieldMinHeight,
        };
        Grid editor = createSummaryEditor(summary, editButton);
        editButton.Click += async (_, _) =>
        {
            if (TopLevel.GetTopLevel(editor) is not Window owner)
                return;
            JsonArray? selected = await MoveRouteEditWindow.ShowAsync(
                owner,
                gameData,
                current);
            if (selected is null || !isControlAlive(editor, isAlive))
                return;
            if (!JsonNode.DeepEquals(current, selected))
            {
                current = (JsonArray)selected.DeepClone();
                request.Commit(current, false);
            }
            summary.Text = BlueprintNodeParameterValues.FormatRoute(current);
        };
        return editor;
    }

    private Control createTransferPositionEditor(
        BlueprintVariableEditorRequest request,
        Func<string, JsonNode?> getRawSiblingValue,
        Action<string, JsonNode?> setRawSiblingValue,
        Func<bool> isAlive)
    {
        JsonArray? current = BlueprintNodeParameterValues.NormalizePosition(request.Value);
        TextBox summary = EditorInputs.CreateReadOnlyTextBox(
            BlueprintNodeParameterValues.FormatPosition(current));
        Button editButton = new()
        {
            Content = LocaleService.Get("TRANSFER_POS_EDIT"),
            MinHeight = EditorInputs.FieldMinHeight,
        };
        Grid editor = createSummaryEditor(summary, editButton);
        editButton.Click += async (_, _) =>
        {
            if (TopLevel.GetTopLevel(editor) is not Window owner)
                return;
            string relatedFieldName = request.Field.RelatedFieldName ?? string.Empty;
            JsonNode? rawMapValue = relatedFieldName.Length == 0
                ? null
                : getRawSiblingValue(relatedFieldName);
            string currentMapReference = BlueprintNodeParameterValues.GetString(rawMapValue);
            TransferPositionSelection? selected = await TransferPositionPickWindow.ShowAsync(
                owner,
                gameData,
                current,
                currentMapReference);
            if (selected is null || !isControlAlive(editor, isAlive))
                return;
            if (!JsonNode.DeepEquals(current, selected.Position))
            {
                current = selected.Position?.DeepClone() as JsonArray;
                request.Commit(current, false);
            }
            summary.Text = BlueprintNodeParameterValues.FormatPosition(current);
            if (relatedFieldName.Length == 0 || selected.MapKey.Length == 0)
                return;
            string resolvedPath = resolveMapPath(selected.MapKey);
            string currentPath = resolveMapPath(
                BlueprintNodeParameterValues.GetString(getRawSiblingValue(relatedFieldName)));
            if (resolvedPath.Length != 0
                && !string.Equals(resolvedPath, currentPath, StringComparison.Ordinal)
                && isControlAlive(editor, isAlive))
            {
                setRawSiblingValue(relatedFieldName, JsonValue.Create(resolvedPath));
            }
        };
        return editor;
    }

    private Control createBlueprintClassEditor(
        BlueprintVariableEditorRequest request,
        Func<bool> isAlive)
    {
        string current = BlueprintNodeParameterValues.GetString(request.Value);
        TextBox summary = EditorInputs.CreateReadOnlyTextBox(current);
        Button browseButton = createBrowseButton();
        Grid editor = createSummaryEditor(summary, browseButton);
        browseButton.Click += async (_, _) =>
        {
            if (TopLevel.GetTopLevel(editor) is not Window owner)
                return;
            string? selected = await BlueprintClassSelector.ShowAsync(
                owner,
                gameData,
                metadataService,
                classResolver,
                current,
                null,
                BlueprintClassSelectorMode.NodeParameter);
            if (string.IsNullOrWhiteSpace(selected)
                || !isControlAlive(editor, isAlive)
                || string.Equals(current, selected, StringComparison.Ordinal))
            {
                return;
            }
            current = selected;
            summary.Text = current;
            request.Commit(JsonValue.Create(current), false);
        };
        return editor;
    }

    private Control createCommonFunctionEditor(
        BlueprintVariableEditorRequest request,
        Func<bool> isAlive)
    {
        string current = BlueprintNodeParameterValues.GetString(request.Value);
        TextBox summary = EditorInputs.CreateReadOnlyTextBox(current);
        Button browseButton = createBrowseButton();
        Grid editor = createSummaryEditor(summary, browseButton);
        browseButton.Click += async (_, _) =>
        {
            if (TopLevel.GetTopLevel(editor) is not Window owner)
                return;
            string? selected = await SearchSelectorDialog.ShowAsync(
                owner,
                LocaleService.Get("COMMON_FUNCTIONS"),
                gameData.CommonFunctionsData.Keys.OrderBy(value => value, StringComparer.Ordinal),
                current);
            if (string.IsNullOrWhiteSpace(selected)
                || !isControlAlive(editor, isAlive)
                || string.Equals(current, selected, StringComparison.Ordinal))
            {
                return;
            }
            current = selected;
            summary.Text = current;
            request.Commit(JsonValue.Create(current), false);
        };
        return editor;
    }

    private static Grid createSummaryEditor(TextBox summary, Button button)
    {
        Grid editor = new()
        {
            ColumnDefinitions = new ColumnDefinitions("*,4,Auto"),
        };
        editor.Children.Add(summary);
        Grid.SetColumn(button, 2);
        editor.Children.Add(button);
        return editor;
    }

    private static Button createBrowseButton()
    {
        return new Button
        {
            Content = "...",
            Width = 24,
            MinHeight = EditorInputs.FieldMinHeight,
            Padding = new Thickness(0),
        };
    }

    private static bool isControlAlive(Control control, Func<bool> isAlive)
    {
        return isAlive() && TopLevel.GetTopLevel(control) is not null;
    }

    internal static string normalizeMapKey(string value)
    {
        string path = value.Replace('\\', '/');
        while (path.StartsWith("./", StringComparison.Ordinal))
            path = path[2..];
        const string marker = "Data/Maps/";
        int markerIndex = path.IndexOf(marker, StringComparison.Ordinal);
        if (markerIndex >= 0)
            path = path[(markerIndex + marker.Length)..];
        int slashIndex = path.LastIndexOf('/');
        int extensionIndex = path.LastIndexOf('.');
        if (extensionIndex > slashIndex)
            path = path[..extensionIndex];
        return path;
    }

    private static string resolveMapPath(string mapKey)
    {
        string normalized = normalizeMapKey(mapKey);
        return normalized.Length == 0 ? string.Empty : normalized + DataConfig.DataFileExtension;
    }
}

internal readonly record struct RouteStep(int X, int Y);

internal sealed record TransferPositionSelection(JsonArray? Position, string MapKey);
