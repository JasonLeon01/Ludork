using System.Collections.Generic;

namespace Ludork.Services;

public enum HistoryStatus
{
    Success,
    Empty,
    Blocked,
    ValidationFailed,
}

public sealed record HistoryResult
{
    public HistoryResult(bool success, string? message = null)
        : this(success ? HistoryStatus.Success : message is null ? HistoryStatus.Empty : HistoryStatus.ValidationFailed, message)
    {
    }

    public HistoryResult(HistoryStatus status, string? message = null)
    {
        Status = status;
        Message = message;
    }

    public HistoryStatus Status { get; private init; }
    public string? Message { get; }
    public bool Success => Status == HistoryStatus.Success;
    public bool IsBlocked
    {
        get => Status == HistoryStatus.Blocked;
        init
        {
            if (value)
                Status = HistoryStatus.Blocked;
        }
    }
    public IReadOnlyList<string> Changes { get; init; } = [];
}
