using KspMission.Import;

var tests = new (string Name, Action Run)[]
{
    ("double scale is rejected", () => WithFixture(f => ExpectError(() => Importer.Import(f.Options with { OptionalJnsq10X = true }), "double"))),
    ("10X directory is rejected even without flag", () => WithFixture(f => ExpectError(() => Importer.Import(f.Options with { JnsqDirectory = Path.Combine(f.Root, "Rescale_10X") }), "double"))),
    ("missing Principia choice is rejected", () => WithFixture(f => ExpectError(() => Importer.Import(f.Options with { Principia = null }), "Principia"))),
    ("conditional axes and rotation select distinct branches", () => WithFixture(f =>
    {
        var on = Importer.Import(f.Options with { Principia = true });
        var off = Importer.Import(f.Options with { Principia = false });
        Equal(93_840_000d, on.Bodies.Single(b => b.Id == "JNSQMun").Orbit!.SemiMajorAxisM);
        Equal(90_960_000d, off.Bodies.Single(b => b.Id == "JNSQMun").Orbit!.SemiMajorAxisM);
        Equal(58_550_000d, on.Bodies.Single(b => b.Id == "JNSQMinmus").Orbit!.SemiMajorAxisM);
        Equal(146_970_000d, off.Bodies.Single(b => b.Id == "JNSQMinmus").Orbit!.SemiMajorAxisM);
        Equal(true, on.Bodies.Single(b => b.Id == "JNSQMinmus").Rotation.TidallyLocked);
        Equal(false, off.Bodies.Single(b => b.Id == "JNSQMinmus").Rotation.TidallyLocked);
    })),
    ("Real patch scales radius orbit and rotation once", () => WithFixture(f =>
    {
        f.WritePatch("@Kopernicus:HAS[@JNSQReborn_Configuration:HAS[#SystemScale[?eal]]]:AFTER[JNSQ-Reborn]\n{\n@Body[JNSQKerbin]\n{\n@Properties\n{\n@radius *= 4\n@rotationPeriod *= 2\n}\n@Orbit\n{\n@semiMajorAxis *= 4\n}\n}\n}");
        var kerbin = Importer.Import(f.Options).Bodies.Single(b => b.Id == "JNSQKerbin");
        Equal(6_400_000d, kerbin.RadiusM);
        Equal(86_400d, kerbin.Rotation.PeriodS);
        Equal(400_000_000d, kerbin.Orbit!.SemiMajorAxisM);
    })),
    ("conditional Real patch does not assign Principia rotation", () => WithFixture(f =>
    {
        f.WritePatch("@Kopernicus:AFTER[JNSQ-Reborn]\n{\n@Body[JNSQMinmus]\n{\n@Properties\n{\n@rotationPeriod:NEEDS[!Principia] *= 2\n}\n}\n}");
        var on = Importer.Import(f.Options with { Principia = true }).Bodies.Single(b => b.Id == "JNSQMinmus");
        Equal<double?>(null, on.Rotation.PeriodS);
    })),
    ("homeworld rotation offset is applied", () => WithFixture(f =>
    {
        f.WriteGlobalPatch("@Kopernicus:AFTER[JNSQ]\n{\n@Body[JNSQKerbin]\n{\n@Properties\n{\n@initialRotation += 180\n}\n}\n}");
        Equal(180d, Importer.Import(f.Options).Bodies.Single(b => b.Id == "JNSQKerbin").Rotation.InitialRotationDeg);
    })),
    ("unhandled physical patch is identified", () => WithFixture(f =>
    {
        f.WriteGlobalPatch("@Kopernicus:AFTER[JNSQ]\n{\n@Body[JNSQKerbin]\n{\n@Properties\n{\n@geeASL = 2\n}\n}\n}");
        var catalog = Importer.Import(f.Options);
        True(catalog.Diagnostics.Any(x => x.Contains("geeASL") && x.Contains("OffsetTime.cfg")));
    })),
    ("duplicate IDs reject catalog", () => WithFixture(f => { f.WriteBody("Duplicate.cfg", "JNSQMun", "JNSQKerbin"); ExpectError(() => Importer.Import(f.Options), "duplicate"); })),
    ("missing parent rejects catalog", () => WithFixture(f => { f.Replace("Mun.cfg", "JNSQKerbin", "Absent"); ExpectError(() => Importer.Import(f.Options), "parent"); })),
    ("parent cycle rejects catalog", () => WithFixture(f => { f.Replace("Kerbin.cfg", "referenceBody = JNSQSun", "referenceBody = JNSQMun"); ExpectError(() => Importer.Import(f.Options), "cycle"); })),
    ("invalid radius rejects catalog", () => WithFixture(f => { f.Replace("Mun.cfg", "radius = 400000", "radius = NaN"); ExpectError(() => Importer.Import(f.Options), "radius"); })),
    ("invalid gravitational parameter rejects catalog", () => WithFixture(f => { f.Replace("Mun.cfg", "geeASL = 0.145", "geeASL = 0.145\ngravParameter = -1"); ExpectError(() => Importer.Import(f.Options), "gravParameter"); })),
    ("explicit gravitational parameter keeps SI provenance", () => WithFixture(f =>
    {
        f.Replace("Mun.cfg", "geeASL = 0.145", "geeASL = 0.145\ngravParameter = 12000000000000");
        var mun = Importer.Import(f.Options).Bodies.Single(b => b.Id == "JNSQMun");
        Equal(12_000_000_000_000d, mun.GravitationalParameterM3S2);
        Equal("m^3/s^2", mun.GravitationalParameterSource!.Unit);
    })),
    ("zero atmosphere boundary retains patch provenance", () => WithFixture(f =>
    {
        f.Replace("Mun.cfg", "Orbit\n{", "Atmosphere\n{\naltitude = 100\n}\nOrbit\n{");
        f.WritePatch("@Kopernicus:AFTER[JNSQ-Reborn]\n{\n@Body[JNSQMun]\n{\n@Atmosphere\n{\n@altitude = 0\n}\n}\n}");
        var mun = Importer.Import(f.Options).Bodies.Single(b => b.Id == "JNSQMun");
        Equal(0d, mun.AtmosphereAltitudeM);
        Equal("metres", mun.AtmosphereAltitudeSource!.Unit);
        Equal("assign", mun.AtmosphereAltitudeSource.Operation);
    })),
    ("catalog is provisional and cannot be analysis ready", () => WithFixture(f =>
    {
        var catalog = Importer.Import(f.Options);
        Equal("raw_config_provisional", catalog.Confidence);
        Equal(false, catalog.AnalysisReady);
        True(catalog.Diagnostics.Any(x => x.Contains("state", StringComparison.OrdinalIgnoreCase)));
        True(catalog.Files.Count >= 3 && catalog.Files.All(x => x.Sha256.Length == 64));
    })),
    ("calendar day and year boundaries", () =>
    {
        var c = NoLeapCalendar.Create(86400, 0);
        Equal("Y0 D0 00:00:00", c.Format(0));
        Equal("Y0 D59 00:00:00", c.Format(59 * 86400));
        Equal("Y0 D60 00:00:00", c.Format(60 * 86400));
        Equal("Y1 D0 00:00:00", c.Format(365 * 86400));
        Equal("Y-1 D364 23:59:59", c.Format(-1));
    }),
    ("calendar refuses leap years and missing day duration", () =>
    {
        ExpectError(() => NoLeapCalendar.Create(86400, 0, true), "leap");
        ExpectError(() => NoLeapCalendar.Create(0, 0), "day");
    }),
    ("degree and radian anomaly fields retain units and epoch", () => WithFixture(f =>
    {
        f.Replace("Mun.cfg", "epoch = 0", "meanAnomalyAtEpochD:NEEDS[!Principia] = 270\nmeanAnomalyAtEpochD:NEEDS[Principia] = 90\nepoch = 0");
        f.Replace("Minmus.cfg", "epoch = 0", "meanAnomalyAtEpoch = 0.9\nepoch = 0");
        var on = Importer.Import(f.Options);
        var off = Importer.Import(f.Options with { Principia = false });
        Equal(90d, on.Bodies.Single(b => b.Id == "JNSQMun").Orbit!.MeanAnomalyAtEpochDeg);
        Equal(270d, off.Bodies.Single(b => b.Id == "JNSQMun").Orbit!.MeanAnomalyAtEpochDeg);
        Equal("meanAnomalyAtEpochD", on.Bodies.Single(b => b.Id == "JNSQMun").Orbit!.MeanAnomalySource!.Key);
        Equal("degrees", on.Bodies.Single(b => b.Id == "JNSQMun").Orbit!.MeanAnomalySource!.Unit);
        Near(0.9 * 180 / Math.PI, on.Bodies.Single(b => b.Id == "JNSQMinmus").Orbit!.MeanAnomalyAtEpochDeg!.Value, 1e-12);
        Equal("radians", on.Bodies.Single(b => b.Id == "JNSQMinmus").Orbit!.MeanAnomalySource!.Unit);
        Equal(0d, on.Bodies.Single(b => b.Id == "JNSQMun").Orbit!.ElementEpochUtS);
    })),
    ("ambiguous anomaly units reject catalog", () => WithFixture(f =>
    {
        f.Replace("Mun.cfg", "epoch = 0", "meanAnomalyAtEpoch = 1\nmeanAnomalyAtEpochD = 90\nepoch = 0");
        ExpectError(() => Importer.Import(f.Options), "both radian and degree");
    })),
    ("source scale mismatch is an explicit requested preview", () => WithFixture(f =>
    {
        f.ReplaceConfiguration("SystemScale = Real", "SystemScale = Standard");
        var catalog = Importer.Import(f.Options);
        Equal("Standard", catalog.SourceConfiguredScale);
        Equal("requested_override_preview", catalog.ScaleSelection);
        True(catalog.Diagnostics.Any(x => x.Contains("override", StringComparison.OrdinalIgnoreCase)));
    })),
    ("no-leap tool calendar is separate from unresolved imported calendar", () => WithFixture(f =>
    {
        var catalog = Importer.Import(f.Options);
        Equal(true, catalog.ImportedCalendar.RealTimeConfigured);
        Equal(false, catalog.ImportedCalendar.Resolved);
        Equal<string?>(null, catalog.GameUtZeroDefinition);
        Equal("requested_tool_display", catalog.DisplayCalendarSelection);
    })),
    ("missing nonroot Orbit cannot create another root", () => WithFixture(f =>
    {
        f.WriteBodyWithoutOrbit("Mun.cfg", "JNSQMun");
        ExpectError(() => Importer.Import(f.Options), "Orbit");
    })),
    ("embedded optional 10X config is rejected", () => WithFixture(f =>
    {
        var dir = Path.Combine(f.Options.JnsqDirectory, "Optional Mods", "JNSQ_Rescale", "Rescale_10X");
        Directory.CreateDirectory(dir);
        File.WriteAllText(Path.Combine(dir, "Kerbin.cfg"), "@Kopernicus {}\n");
        ExpectError(() => Importer.Import(f.Options), "double");
    }))
};

var failed = 0;
foreach (var (name, run) in tests)
{
    try { run(); Console.WriteLine($"PASS {name}"); }
    catch (Exception e) { failed++; Console.WriteLine($"FAIL {name}: {e.Message}"); }
}
Console.WriteLine($"{tests.Length - failed}/{tests.Length} passed");
return failed == 0 ? 0 : 1;

static void Equal<T>(T expected, T actual)
{
    if (!EqualityComparer<T>.Default.Equals(expected, actual)) throw new Exception($"expected {expected}, got {actual}");
}
static void True(bool value) { if (!value) throw new Exception("expected true"); }
static void Near(double expected, double actual, double tolerance)
{
    if (Math.Abs(expected - actual) > tolerance) throw new Exception($"expected {expected}, got {actual}");
}
static void ExpectError(Action run, string part)
{
    try { run(); } catch (ImportException e) when (e.Message.Contains(part, StringComparison.OrdinalIgnoreCase)) { return; }
    throw new Exception($"expected ImportException containing '{part}'");
}
static void WithFixture(Action<Fixture> run)
{
    using var fixture = new Fixture(); run(fixture);
}
sealed class Fixture : IDisposable
{
    public string Root { get; } = Path.Combine(Path.GetTempPath(), "ksp-import-" + Guid.NewGuid().ToString("N"));
    public ImportOptions Options => new(Path.Combine(Root, "Reborn"), Path.Combine(Root, "JNSQ"), "Real", true, false);
    public Fixture()
    {
        Directory.CreateDirectory(Options.RebornDirectory);
        Directory.CreateDirectory(Options.JnsqDirectory);
        File.WriteAllText(Path.Combine(Options.JnsqDirectory, "Kronometer.cfg"), "@Kronometer:FOR[JNSQ]\n{\n@useHomeDay = true\n}");
        File.WriteAllText(Path.Combine(Options.RebornDirectory, "JNSQReborn-Configuration.cfg"), "JNSQReborn_Configuration\n{\nSystemScale = Real\nRealTime = True\n}\n");
        File.WriteAllText(Path.Combine(Options.RebornDirectory, "Sun.cfg"), "@Kopernicus:FOR[JNSQ-Reborn]\n{\nBody\n{\nname = JNSQSun\nProperties\n{\nradius = 260000000\ngeeASL = 1\n}\n}\n}");
        WriteBody("Kerbin.cfg", "JNSQKerbin", "JNSQSun");
        File.WriteAllText(Path.Combine(Options.RebornDirectory, "Mun.cfg"), "@Kopernicus:FOR[JNSQ-Reborn]\n{\nBody\n{\nname = JNSQMun\nProperties\n{\nradius = 400000\ngeeASL = 0.145\ntidallyLocked = True\n}\nOrbit\n{\nreferenceBody = JNSQKerbin\nsemiMajorAxis:NEEDS[!Principia] = 90960000\nsemiMajorAxis:NEEDS[Principia] = 93840000\nepoch = 0\n}\n}\n}");
        File.WriteAllText(Path.Combine(Options.RebornDirectory, "Minmus.cfg"), "@Kopernicus:FOR[JNSQ-Reborn]\n{\nBody\n{\nname = JNSQMinmus\nProperties\n{\nradius = 160000\ngeeASL = 0.05\ntidallyLocked:NEEDS[!Principia] = False\ntidallyLocked:NEEDS[Principia] = True\n}\nOrbit\n{\nreferenceBody = JNSQKerbin\nsemiMajorAxis:NEEDS[!Principia] = 146970000\nsemiMajorAxis:NEEDS[Principia] = 58550000\nepoch = 0\n}\n}\n}");
    }
    public void WriteBody(string file, string id, string parent) => File.WriteAllText(Path.Combine(Options.RebornDirectory, file), $"@Kopernicus:FOR[JNSQ-Reborn]\n{{\nBody\n{{\nname = {id}\nProperties\n{{\nradius = 1600000\ngeeASL = 1\nrotationPeriod = 43200\ninitialRotation = 0\n}}\nOrbit\n{{\nreferenceBody = {parent}\nsemiMajorAxis = 100000000\nepoch = 0\n}}\n}}\n}}");
    public void WriteBodyWithoutOrbit(string file, string id) => File.WriteAllText(Path.Combine(Options.RebornDirectory, file), $"@Kopernicus:FOR[JNSQ-Reborn]\n{{\nBody\n{{\nname = {id}\nProperties\n{{\nradius = 400000\ngeeASL = 0.1\n}}\n}}\n}}");
    public void ReplaceConfiguration(string before, string after)
    {
        var path = Path.Combine(Options.RebornDirectory, "JNSQReborn-Configuration.cfg");
        File.WriteAllText(path, File.ReadAllText(path).Replace(before, after));
    }
    public void WritePatch(string content)
    {
        var dir = Path.Combine(Options.RebornDirectory, "Bodies", "Rescale"); Directory.CreateDirectory(dir);
        File.WriteAllText(Path.Combine(dir, "Kerbin.cfg"), content);
    }
    public void WriteGlobalPatch(string content)
    {
        var dir = Path.Combine(Options.RebornDirectory, "Configs"); Directory.CreateDirectory(dir);
        File.WriteAllText(Path.Combine(dir, "OffsetTime.cfg"), content);
    }
    public void Replace(string file, string before, string after)
    {
        var path = Path.Combine(Options.RebornDirectory, file);
        File.WriteAllText(path, File.ReadAllText(path).Replace(before, after));
    }
    public void Dispose() => Directory.Delete(Root, true);
}
