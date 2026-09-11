using System;

namespace Ludork.Services;

public sealed class ProjectStateCheckException(string message, Exception? innerException = null)
    : Exception(message, innerException);
