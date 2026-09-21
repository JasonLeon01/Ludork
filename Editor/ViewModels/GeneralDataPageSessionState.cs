
using Ludork.Models;

namespace Ludork.ViewModels;

internal sealed class GeneralDataPageSessionState
{
    public string SearchText { get; set; } = string.Empty;
    public GeneralDataViewMode ViewMode { get; set; }
    public string? SelectedMemberId { get; set; }
}
