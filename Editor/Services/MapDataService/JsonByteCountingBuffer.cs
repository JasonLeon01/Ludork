using System;
using System.Buffers;

namespace Ludork.Services;

internal sealed class JsonByteCountingBuffer : IBufferWriter<byte>, IDisposable
{
    private byte[] buffer = ArrayPool<byte>.Shared.Rent(16384);

    public long Count { get; private set; }

    public void Advance(int count) => Count += count;

    public Memory<byte> GetMemory(int sizeHint = 0)
    {
        EnsureCapacity(sizeHint);
        return buffer;
    }

    public Span<byte> GetSpan(int sizeHint = 0)
    {
        EnsureCapacity(sizeHint);
        return buffer;
    }

    private void EnsureCapacity(int sizeHint)
    {
        if (buffer.Length >= sizeHint)
            return;
        byte[] previous = buffer;
        buffer = ArrayPool<byte>.Shared.Rent(sizeHint);
        ArrayPool<byte>.Shared.Return(previous);
    }

    public void Dispose()
    {
        if (buffer.Length == 0)
            return;
        ArrayPool<byte>.Shared.Return(buffer);
        buffer = [];
    }
}
