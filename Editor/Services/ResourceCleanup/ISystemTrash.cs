namespace Ludork.Services.ResourceCleanup;

internal interface ISystemTrash
{
    string? MoveToTrash(string path);
}
