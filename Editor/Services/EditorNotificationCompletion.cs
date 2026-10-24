namespace Ludork.Services;

internal enum EditorNotificationStatus
{
    Activated = 0,
    Dismissed = 1,
    Denied = 2,
    Failed = 3,
}

internal sealed record EditorNotificationCompletion(uint Id, EditorNotificationStatus Status, string? Error = null);
