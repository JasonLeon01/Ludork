namespace Ludork.Services;

internal interface ISystemTrash
{
    string? MoveToTrash(string path);
}
