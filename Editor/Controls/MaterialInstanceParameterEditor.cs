using Avalonia.Controls;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

internal sealed class MaterialInstanceParameterEditor : UserControl
{
    private readonly ProjectDataStore gameData;
    private readonly JsonObject values;
    private readonly string path;
    private readonly Action<JsonNode?, bool> changed;
    private readonly StackPanel panel = new() { Spacing = 6 };
    private readonly List<BlueprintVariableForm> forms = [];

    public MaterialInstanceParameterEditor(ProjectDataStore gameData, string path, JsonNode? value, Action<JsonNode?, bool> changed)
    {
        this.gameData = gameData;
        this.path = path;
        this.changed = changed;
        values = value?.DeepClone() as JsonObject ?? new JsonObject();
        Content = panel;
        refresh();
        AttachedToVisualTree += (_, _) => { gameData.Documents.ContentChanged += onDocumentsChanged; refresh(); };
        DetachedFromVisualTree += (_, _) => { gameData.Documents.ContentChanged -= onDocumentsChanged; clearForms(); };
    }

    private void onDocumentsChanged(object? sender, EditorDocumentsChangedEventArgs args)
    {
        if (args.Reset || args.AffectsSection("Materials"))
            refresh();
    }

    private void clearForms()
    {
        foreach (BlueprintVariableForm form in forms)
            form.Dispose();
        forms.Clear();
    }

    private void refresh()
    {
        clearForms();
        panel.Children.Clear();
        JsonArray parameters = GameDataPath.TryGetMaterialKey(path, out string key)
            && gameData.Assets.MaterialsData.TryGetValue(key, out Models.MaterialSnapshot? material)
            ? material.ToJson()["parameters"] as JsonArray ?? [] : [];
        HashSet<string> declared = [];
        foreach (JsonObject parameter in parameters.OfType<JsonObject>())
        {
            string id = parameter["id"]!.GetValue<string>();
            declared.Add(id);
            CheckBox enabled = new() { Content = parameter["displayName"]?.GetValue<string>() ?? id, IsChecked = values.ContainsKey(id) };
            enabled.IsCheckedChanged += (_, _) =>
            {
                if (enabled.IsChecked == true)
                    values[id] = parameter["default"]?.DeepClone();
                else
                    values.Remove(id);
                changed(values.DeepClone(), false);
                refresh();
            };
            panel.Children.Add(enabled);
            if (values.ContainsKey(id))
            {
                BlueprintVariableForm form = MaterialPropertyForm.Create(gameData, parameter, values[id], next =>
                {
                    values[id] = next;
                    changed(values.DeepClone(), false);
                });
                forms.Add(form);
                panel.Children.Add(form);
            }
        }
        foreach (string unknown in values.Select(pair => pair.Key).Where(key => !declared.Contains(key)).ToArray())
        {
            Button remove = new() { Content = LocaleService.Get("MATERIAL_REMOVE_UNKNOWN") + " " + unknown };
            remove.Click += (_, _) => { values.Remove(unknown); changed(values.DeepClone(), false); refresh(); };
            panel.Children.Add(remove);
        }
        if (parameters.Count == 0 && values.Count == 0)
            panel.Children.Add(new TextBlock { Text = LocaleService.Get("MATERIAL_NO_PARAMETERS") });
    }
}
