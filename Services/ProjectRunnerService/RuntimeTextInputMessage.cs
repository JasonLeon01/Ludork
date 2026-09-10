namespace Ludork.Services;

public sealed record RuntimeTextInputMessage(
    string Action,
    string Session,
    double X,
    double Y,
    double Width,
    double Height,
    long RunGeneration = 0,
    long ConnectionGeneration = 0);
