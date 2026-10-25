using System;
using System.Collections.Generic;

namespace Ludork.Plugins.OfficialMotaMapFill.Generation;

internal sealed class StableRandom
{
    private uint state;

    public StableRandom(int seed)
    {
        state = seed == 0 ? 0xA341316Cu : unchecked((uint)seed);
    }

    public int Next(int exclusiveMaximum)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(exclusiveMaximum);
        uint value = state;
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        state = value;
        return (int)(value % (uint)exclusiveMaximum);
    }

    public void Shuffle<T>(IList<T> values)
    {
        for (int index = values.Count - 1; index > 0; index--)
        {
            int other = Next(index + 1);
            (values[index], values[other]) = (values[other], values[index]);
        }
    }
}
