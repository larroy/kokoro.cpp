using System.Globalization;

// Parses a strictly positive finite decimal number (the -s/--speed option and the /speed command).
internal static class SpeedValue
{
    public static bool TryParse(string value, out float speed)
    {
        var ok = float.TryParse(value, NumberStyles.AllowLeadingSign | NumberStyles.AllowDecimalPoint
            | NumberStyles.AllowExponent, CultureInfo.InvariantCulture, out var parsed)
            && float.IsFinite(parsed) && parsed > 0;
        speed = ok ? parsed : 0;
        return ok;
    }
}
