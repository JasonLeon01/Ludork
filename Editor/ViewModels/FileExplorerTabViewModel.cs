using Ludork.Services;
using System;
using System.ComponentModel;
using System.IO;

namespace Ludork.ViewModels;

public sealed class FileExplorerTabViewModel : ViewModelBase, IDisposable
{
    public FileExplorerTabViewModel(int number, FileExplorerViewModel explorer)
    {
        Number = number;
        Explorer = explorer;
        Explorer.PropertyChanged += onExplorerPropertyChanged;
        LocaleService.LanguageChanged += onLanguageChanged;
    }

    public int Number { get; }
    public FileExplorerViewModel Explorer { get; }
    public string Title => Number == 1 ? LocaleService.Get("FILE_EXPLORER") : $"{LocaleService.Get("FILE_EXPLORER")} {Number}";
    public string RelativePath => Path.GetRelativePath(Explorer.ProjectPath, Explorer.CurrentPath).Replace('\\', '/');
    public bool CanClose => Number != 1;

    public void Dispose()
    {
        Explorer.PropertyChanged -= onExplorerPropertyChanged;
        LocaleService.LanguageChanged -= onLanguageChanged;
        Explorer.Dispose();
    }

    private void onExplorerPropertyChanged(object? sender, PropertyChangedEventArgs args)
    {
        if (args.PropertyName == nameof(FileExplorerViewModel.CurrentPath))
            OnPropertyChanged(nameof(RelativePath));
    }

    private void onLanguageChanged(object? sender, EventArgs args) => OnPropertyChanged(nameof(Title));
}
