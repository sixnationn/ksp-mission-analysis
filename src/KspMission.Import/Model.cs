namespace KspMission.Import;

public sealed record ImportOptions(string RebornDirectory, string JnsqDirectory, string Scale, bool? Principia, bool OptionalJnsq10X,
    string? RebornArchivePath = null, string? JnsqArchivePath = null);
public sealed record SourceFile(string Pack, string Version, string Path, string Sha256);
public sealed record SourceValue<T>(T Value, string File, int Line, string? Condition = null,
    string? BaseFile = null, int? BaseLine = null, string? Operation = null,
    string? Key = null, string? Unit = null);
public sealed record OrbitPreview(string? ElementConvention, double? SemiMajorAxisM, SourceValue<double>? SemiMajorAxisSource,
    double? Eccentricity, SourceValue<double>? EccentricitySource,
    double? InclinationDeg, SourceValue<double>? InclinationSource,
    double? LongitudeOfAscendingNodeDeg, SourceValue<double>? LongitudeOfAscendingNodeSource,
    double? ArgumentOfPeriapsisDeg, SourceValue<double>? ArgumentOfPeriapsisSource,
    double? MeanAnomalyAtEpochDeg, SourceValue<double>? MeanAnomalySource,
    double? ElementEpochUtS, SourceValue<double>? ElementEpochSource, string? ReferenceBodyId);
public sealed record RotationPreview(bool? Rotates, bool? TidallyLocked, double? PeriodS, double? InitialRotationDeg,
    string OrientationConvention, SourceValue<double>? PeriodSource, SourceValue<double>? InitialRotationSource);
public sealed record BodyPreview(string Id, string? DisplayName, string? ParentId, double RadiusM,
    SourceValue<double> RadiusSource, double? GeeAsl, SourceValue<double>? GeeAslSource,
    double? GravitationalParameterM3S2, SourceValue<double>? GravitationalParameterSource,
    double? AtmosphereAltitudeM, SourceValue<double>? AtmosphereAltitudeSource,
    RotationPreview Rotation, OrbitPreview? Orbit, string SourcePath);
public sealed record ImportedCalendarPreview(string Confidence, bool Resolved,
    bool? RealTimeConfigured, SourceValue<bool>? RealTimeSource,
    double? OffsetYear, double? OffsetDay, double? OffsetTimeS,
    IReadOnlyList<int>? MonthLengths, double? DayDurationS, bool? UseLeapYears,
    string? OffsetSourcePath);
public sealed record ImportResult(int SchemaVersion, string Confidence, bool AnalysisReady, string ResolutionMethod,
    string Scale, bool Principia, string ForceModel, string? StateEpochUtS, string? StateFrame,
    IReadOnlyList<BodyPreview> Bodies, IReadOnlyList<SourceFile> Files,
    IReadOnlyList<string> Diagnostics, NoLeapCalendar DisplayCalendar,
    string SourceConfiguredScale, SourceValue<string> SourceConfiguredScaleSource, string ScaleSelection,
    ImportedCalendarPreview ImportedCalendar, string? GameUtZeroDefinition, string DisplayCalendarSelection);
public sealed class ImportException(string message) : Exception(message);

public sealed record NoLeapCalendar(double DayDurationS, double DisplayOriginUtS, bool UseLeapYears,
    IReadOnlyList<int> MonthLengths)
{
    public static NoLeapCalendar Create(double dayDurationS, double? displayOriginUtS, bool useLeapYears = false,
        IReadOnlyList<int>? monthLengths = null)
    {
        if (useLeapYears) throw new ImportException("No-leap display cannot use leap years.");
        if (!double.IsFinite(dayDurationS) || dayDurationS <= 0) throw new ImportException("Calendar day duration must be positive SI seconds.");
        if (displayOriginUtS is null || !double.IsFinite(displayOriginUtS.Value)) throw new ImportException("Calendar display origin UT is required.");
        monthLengths ??= [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
        if (monthLengths.Count != 12 || monthLengths.Any(x => x <= 0) || monthLengths.Sum() != 365)
            throw new ImportException("No-leap calendar months must sum to 365 days.");
        return new(dayDurationS, displayOriginUtS.Value, false, monthLengths.ToArray());
    }

    public string Format(double utS)
    {
        if (!double.IsFinite(utS)) throw new ImportException("UT must be finite SI seconds.");
        var seconds = utS - DisplayOriginUtS;
        var day = Math.Floor(seconds / DayDurationS);
        if (day < long.MinValue || day > long.MaxValue) throw new ImportException("UT is outside calendar range.");
        var wholeDay = (long)day;
        var year = (long)Math.Floor(wholeDay / 365d);
        var ordinal = wholeDay - year * 365;
        var part = seconds - day * DayDurationS;
        var clockSeconds = (long)Math.Floor(part * 86400d / DayDurationS);
        if (clockSeconds == 86400) { clockSeconds = 0; ordinal++; if (ordinal == 365) { ordinal = 0; year++; } }
        return $"Y{year} D{ordinal} {clockSeconds / 3600:00}:{clockSeconds / 60 % 60:00}:{clockSeconds % 60:00}";
    }
}
