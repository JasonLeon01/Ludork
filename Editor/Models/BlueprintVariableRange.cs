namespace Ludork.Models;

public sealed record BlueprintVariableRange(double Minimum, double Maximum, double Step)
{
    public BlueprintVariableRange Normalize()
    {
        double minimum = Minimum;
        double maximum = Maximum;
        if (maximum < minimum)
            (minimum, maximum) = (maximum, minimum);
        double step = Step > 0 ? Step : 1;
        return new BlueprintVariableRange(minimum, maximum, step);
    }
}
