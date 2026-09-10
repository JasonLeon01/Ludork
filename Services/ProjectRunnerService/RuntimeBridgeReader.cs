using System;
using System.IO;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

internal sealed class RuntimeBridgeReader(NetworkStream stream, int maximumMessageSize)
{
    private readonly byte[] buffer = new byte[4096];
    private int offset;
    private int count;

    public async Task<(string? Line, string? Error)> ReadAsync(CancellationToken cancellationToken)
    {
        using MemoryStream line = new();
        while (true)
        {
            if (offset == count)
            {
                count = await stream.ReadAsync(buffer.AsMemory(), cancellationToken);
                offset = 0;
                if (count == 0)
                    return line.Length == 0 ? (null, null) : (null, "incomplete bridge message");
            }
            int newline = Array.IndexOf(buffer, (byte)'\n', offset, count - offset);
            int end = newline < 0 ? count : newline;
            int length = end - offset;
            if (line.Length + length > maximumMessageSize)
                return (null, "bridge message exceeds the size limit");
            line.Write(buffer, offset, length);
            offset = newline < 0 ? count : newline + 1;
            if (newline >= 0)
                return (Encoding.UTF8.GetString(line.GetBuffer(), 0, (int)line.Length).TrimEnd('\r'), null);
        }
    }
}
