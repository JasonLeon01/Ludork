using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Templates;
using Avalonia.Layout;
using Ludork.Services;
using System;

namespace Ludork.Views.Utils;

public sealed class DocumentStatusPresenter : StackPanel
{
    private readonly ProjectDataStore gameData;
    private readonly string section;
    private readonly string key;
    private readonly TextBlock marker = new() { Text = "*", VerticalAlignment = VerticalAlignment.Center };
    private EditorDocument? document;

    public DocumentStatusPresenter(ProjectDataStore gameData, string section, string key)
    {
        this.gameData = gameData;
        this.section = section;
        this.key = key;
        Orientation = Orientation.Horizontal;
        Spacing = 4;
        Children.Add(new HintedTextPresenter { Text = key });
        Children.Add(marker);
        marker.IsVisible = gameData.GetDocument(section, key)?.IsModified == true;
    }

    public static IDataTemplate CreateTemplate(ProjectDataStore gameData, string section)
    {
        return new FuncDataTemplate<string>((key, _) => key is null ? null : new DocumentStatusPresenter(gameData, section, key));
    }

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs args)
    {
        base.OnAttachedToVisualTree(args);
        gameData.Documents.Changed += onRegistryChanged;
        bindDocument(gameData.GetDocument(section, key));
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs args)
    {
        gameData.Documents.Changed -= onRegistryChanged;
        bindDocument(null);
        base.OnDetachedFromVisualTree(args);
    }

    private void onRegistryChanged(object? sender, EditorDocumentsChangedEventArgs args)
    {
        if (args.Reset)
        {
            bindDocument(gameData.GetDocument(section, key));
            return;
        }
        foreach (EditorDocument affected in args.AffectedDocuments)
        {
            if (ReferenceEquals(affected, document) || affected.Section == section && affected.Key == key)
            {
                bindDocument(gameData.Documents.Find(section, key));
                return;
            }
        }
    }

    private void bindDocument(EditorDocument? next)
    {
        if (!ReferenceEquals(document, next))
        {
            if (document is not null)
                document.Changed -= onDocumentChanged;
            document = next;
            if (document is not null)
                document.Changed += onDocumentChanged;
        }
        update();
    }

    private void onDocumentChanged(object? sender, EventArgs args) => update();

    private void update() => marker.IsVisible = document?.IsModified == true;
}
