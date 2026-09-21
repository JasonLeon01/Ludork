using Avalonia.Controls;
using Avalonia;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Avalonia.VisualTree;
using AvaloniaEdit.Document;
using Ludork.Controls;
using Ludork.Models;
using Ludork.Plugin.Abstractions;
using Ludork.Services;
using Ludork.Services.BlueprintAssistant;
using Ludork.Services.Plugins;
using Ludork.ViewModels;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Views;

public partial class MainWindow
{
    private void showHelp()
    {
        string? docsRoot = EditorRuntimePaths.FindDirectory("docs");
        if (docsRoot is null)
            return;
        string path = Path.Combine(docsRoot, LocaleService.CurrentLanguage);
        string imageRoot = Path.Combine(docsRoot, "_images");
        if (!Directory.Exists(path) || !Directory.Exists(imageRoot))
            return;
        List<HintedTextPresenter> mapHints = MapList
            .GetVisualDescendants()
            .OfType<HintedTextPresenter>()
            .ToList();
        foreach (HintedTextPresenter hint in mapHints)
        {
            ToolTip.SetIsOpen(hint, false);
            ToolTip.SetServiceEnabled(hint, false);
        }
        MarkdownPreviewWindow window = new MarkdownPreviewWindow(
            path,
            LocaleService.Get("HELP_EXPLANATION"),
            imageRoot);
        window.Closed += (_, _) =>
        {
            foreach (HintedTextPresenter hint in mapHints)
                ToolTip.SetServiceEnabled(hint, true);
        };
        _ = window.ShowDialog(this);
    }

    private void onIndividualWindowClicked(object? sender, EventArgs args)
    {
        if (viewModel?.CanConfigureIndividualWindow == true)
            viewModel.IndividualWindow = !viewModel.IndividualWindow;
    }

    private void onOpenAbout(object? sender, EventArgs args)
    {
        if (Application.Current is App app)
            app.showAbout(this);
    }

    private async void onImportPlugin(object? sender, EventArgs args)
    {
        await pluginMenus.ImportAsync();
    }

    private async void onManagePlugins(object? sender, EventArgs args)
    {
        await pluginMenus.ManageAsync();
    }

    private async void onPackProject(object? sender, EventArgs args)
    {
        if (projectPack is not null)
            await projectPack.ShowAsync();
    }
}
