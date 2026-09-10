using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Templates;
using Avalonia.Layout;
using Ludork.Services;
using System;

namespace Ludork.Views.Utils;

public sealed class DocumentStatusPresenter : StackPanel
{
    private readonly GameDataService gameData;
    private readonly string section;
    private readonly string key;
    private readonly TextBlock marker = new() { Text = "*", VerticalAlignment = VerticalAlignment.Center };

    public DocumentStatusPresenter(GameDataService gameData, string section, string key)
    {
        this.gameData = gameData;
        this.section = section;
        this.key = key;
        Orientation = Orientation.Horizontal;
        Spacing = 4;
        Children.Add(new HintedTextPresenter { Text = key });
        Children.Add(marker);
        update();
    }

    public static IDataTemplate CreateTemplate(GameDataService gameData, string section)
    {
        return new FuncDataTemplate<string>((key, _) => new DocumentStatusPresenter(gameData, section, key));
    }

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs args)
    {
        base.OnAttachedToVisualTree(args);
        gameData.Documents.Changed += onChanged;
        update();
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs args)
    {
        gameData.Documents.Changed -= onChanged;
        base.OnDetachedFromVisualTree(args);
    }

    private void onChanged(object? sender, EventArgs args) => update();

    private void update() => marker.IsVisible = gameData.GetDocument(section, key)?.IsModified == true;
}
