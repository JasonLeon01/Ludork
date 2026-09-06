using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Ludork.Models;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;

namespace Ludork.Views;

public sealed class ReferenceTreeWindow : Window
{
    private readonly ReferenceIndexService referenceIndex;

    public ReferenceTreeWindow(ReferenceIndexService referenceIndex, string nodeId)
    {
        this.referenceIndex = referenceIndex;
        ReferenceNode? node = referenceIndex.GetNode(nodeId);
        string nodeName = node is null ? nodeId : $"{getTypeName(node.Type)}: {node.Key}";
        Title = LocaleService.Get("REFERENCE_TREE_TITLE").Replace("{name}", nodeName, StringComparison.Ordinal);
        Width = 980;
        Height = 620;
        MinWidth = 640;
        MinHeight = 420;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = new SolidColorBrush(Color.Parse("#202124"));
        FontFamily = FontFamily.Parse("avares://Ludork/Assets/HarmonyOS_Sans_SC_Regular.ttf#HarmonyOS Sans SC");
        EditorWindowIcon.Apply(this);
        ReferenceTreeGraphControl graph = new(referenceIndex, nodeId);
        graph.NodeOpenRequested += onNodeOpenRequested;
        Content = graph;
        KeyDown += onKeyDown;
    }

    private void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Key != Key.Escape)
            return;
        Close();
        args.Handled = true;
    }

    private void onNodeOpenRequested(object? sender, ReferenceNodeOpenEventArgs args)
    {
        string path = referenceIndex.GetNodePath(args.NodeId);
        if (!File.Exists(path))
            return;
        try
        {
            Process.Start(new ProcessStartInfo(path) { UseShellExecute = true });
        }
        catch (Win32Exception)
        {
        }
        catch (InvalidOperationException)
        {
        }
    }

    private static string getTypeName(string type)
    {
        string key = type switch
        {
            "asset" => "REFERENCE_TYPE_ASSET",
            "autoTile" => "REFERENCE_TYPE_AUTOTILE",
            "blueprint" => "REFERENCE_TYPE_BLUEPRINT",
            "commonFunction" => "REFERENCE_TYPE_COMMON_FUNCTION",
            "config" => "REFERENCE_TYPE_CONFIG",
            "general" => "REFERENCE_TYPE_GENERAL",
            "generalMember" => "REFERENCE_TYPE_GENERAL_MEMBER",
            "map" => "REFERENCE_TYPE_MAP",
            "animation" => "REFERENCE_TYPE_ANIMATION",
            "tileset" => "REFERENCE_TYPE_TILESET",
            _ => "REFERENCE_TYPE_UNKNOWN",
        };
        return LocaleService.Get(key);
    }
}
