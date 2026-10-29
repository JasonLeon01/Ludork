using Avalonia.Controls;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed partial class BlueprintVariableForm
{
    private readonly Dictionary<BlueprintVariableField, Control> fieldValueEditors = [];
    private readonly Dictionary<Control, ValueEditorUpdater> valueEditorUpdaters = [];
    private bool applyingFieldValues;

    internal bool TryUpdateFieldValues(IReadOnlyList<BlueprintVariableField> nextFields)
    {
        if (disposed || building || applyingFieldValues || fields.Count != nextFields.Count)
            return false;
        List<(ValueEditorUpdater Updater, BlueprintVariableField Field)> updates = [];
        for (int index = 0; index < fields.Count; index++)
        {
            if (!fieldShapesMatch(fields[index], nextFields[index])
                || !collectFieldValueUpdates(fields[index], nextFields[index], updates))
                return false;
        }
        applyingFieldValues = true;
        try
        {
            for (int index = 0; index < fields.Count; index++)
            {
                BlueprintVariableField field = fields[index];
                copyFieldValueState(field, nextFields[index]);
                JsonNode? value = getFieldValue(field);
                if (!values.TryGetValue(field.Name, out JsonNode? previous) || !JsonNode.DeepEquals(previous, value))
                    values[field.Name] = value?.DeepClone();
            }
            foreach ((ValueEditorUpdater updater, BlueprintVariableField field) in updates)
                updater.Update(field.DisplayValue ?? getFieldValue(field), field.IsMixedValue);
            foreach (MultipleFieldEditor item in multipleFieldEditors)
                item.Editor.IsEnabled = !isReadOnly && !item.Field.IsReadOnly && !item.Field.IsMultiEditReadOnly;
            refreshDependencyStates();
        }
        finally
        {
            applyingFieldValues = false;
        }
        return true;
    }

    private bool collectFieldValueUpdates(BlueprintVariableField previous, BlueprintVariableField next,
        List<(ValueEditorUpdater Updater, BlueprintVariableField Field)> updates)
    {
        if (previous.IsMultiEditContainer && previous.MultiEditChildren.Count != 0)
        {
            for (int index = 0; index < previous.MultiEditChildren.Count; index++)
                if (!collectFieldValueUpdates(previous.MultiEditChildren[index], next.MultiEditChildren[index], updates))
                    return false;
            return true;
        }
        JsonNode? nextValue = next.DisplayValue ?? getFieldValue(next);
        if (previous.IsMixedValue == next.IsMixedValue
            && JsonNode.DeepEquals(previous.DisplayValue ?? getFieldValue(previous), nextValue))
            return true;
        if (!fieldValueEditors.TryGetValue(previous, out Control? editor)
            || !valueEditorUpdaters.TryGetValue(editor, out ValueEditorUpdater? updater)
            || !updater.CanUpdate(nextValue, next.IsMixedValue))
            return false;
        updates.Add((updater, next));
        return true;
    }

    private static void copyFieldValueState(BlueprintVariableField field, BlueprintVariableField next)
    {
        if (!JsonNode.DeepEquals(field.Value, next.Value))
            field.Value = next.Value?.DeepClone();
        if (!JsonNode.DeepEquals(field.DisplayValue, next.DisplayValue))
            field.DisplayValue = next.DisplayValue?.DeepClone();
        field.PreserveNullValue = next.PreserveNullValue;
        field.IsMixedValue = next.IsMixedValue;
        field.IsMultiEditReadOnly = next.IsMultiEditReadOnly;
        if (field.MultiEditValues.Count != next.MultiEditValues.Count
            || !field.MultiEditValues.Zip(next.MultiEditValues).All(pair => JsonNode.DeepEquals(pair.First, pair.Second)))
            field.MultiEditValues = next.MultiEditValues.Select(value => value?.DeepClone()).ToArray();
        for (int index = 0; index < field.MultiEditChildren.Count; index++)
            copyFieldValueState(field.MultiEditChildren[index], next.MultiEditChildren[index]);
    }

    private static bool fieldShapesMatch(BlueprintVariableField field, BlueprintVariableField next)
    {
        if (field.Name != next.Name || field.DisplayName != next.DisplayName || field.Description != next.Description
            || field.Type != next.Type || field.Module != next.Module || field.TypeName != next.TypeName
            || field.SourceClass != next.SourceClass || field.HasDefaultValue != next.HasDefaultValue
            || !JsonNode.DeepEquals(field.DefaultValue, next.DefaultValue) || !JsonNode.DeepEquals(field.Meta, next.Meta)
            || field.IsMultiSelection != next.IsMultiSelection || field.IsMultiEditContainer != next.IsMultiEditContainer
            || !field.EditPath.SequenceEqual(next.EditPath) || field.IsComponent != next.IsComponent
            || field.IsReadOnly != next.IsReadOnly || field.UseJsonTableEditor != next.UseJsonTableEditor
            || field.EditorKind != next.EditorKind || field.RelatedFieldName != next.RelatedFieldName
            || field.AssetSubdirectory != next.AssetSubdirectory || field.RectSourceField != next.RectSourceField
            || field.Range != next.Range || field.Dependency?.Source != next.Dependency?.Source
            || field.Dependency?.Operator != next.Dependency?.Operator
            || !JsonNode.DeepEquals(field.Dependency?.ExpectedValue, next.Dependency?.ExpectedValue)
            || field.Options.Count != next.Options.Count || field.Fields.Count != next.Fields.Count
            || field.MultiEditChildren.Count != next.MultiEditChildren.Count)
            return false;
        for (int index = 0; index < field.Options.Count; index++)
            if (field.Options[index].Label != next.Options[index].Label
                || !JsonNode.DeepEquals(field.Options[index].Value, next.Options[index].Value))
                return false;
        for (int index = 0; index < field.Fields.Count; index++)
            if (!fieldShapesMatch(field.Fields[index], next.Fields[index])
                || !JsonNode.DeepEquals(field.Fields[index].Value, next.Fields[index].Value)
                || !JsonNode.DeepEquals(field.Fields[index].DisplayValue, next.Fields[index].DisplayValue)
                || field.Fields[index].PreserveNullValue != next.Fields[index].PreserveNullValue)
                return false;
        for (int index = 0; index < field.MultiEditChildren.Count; index++)
            if (!fieldShapesMatch(field.MultiEditChildren[index], next.MultiEditChildren[index]))
                return false;
        return true;
    }

    private void registerNumberValueUpdater(NumericUpDown editor, bool integer, double limit,
        bool requireExactDecimal = false)
    {
        bool canUpdate(JsonNode? value, bool mixed)
        {
            if (!JsonScalar.TryGetFiniteNumber(value, out double number))
                return false;
            decimal displayed = getDecimal(value);
            return Math.Abs(number) <= limit
                && (mixed || displayed >= editor.Minimum && displayed <= editor.Maximum)
                && (!integer || number == Math.Truncate(number))
                && (!requireExactDecimal || decimal.ToDouble(displayed) == number);
        }
        void update(JsonNode? value, bool mixed)
        {
            decimal? next = mixed ? null : getDecimal(value);
            editor.PlaceholderText = mixed ? LocaleService.Get("MULTIPLE_CHOICES") : null;
            if (editor.Value != next)
                editor.Value = next;
        }
        valueEditorUpdaters[editor] = new(canUpdate, update);
    }

    private sealed record ValueEditorUpdater(
        Func<JsonNode?, bool, bool> CanUpdate,
        Action<JsonNode?, bool> Update);
}
