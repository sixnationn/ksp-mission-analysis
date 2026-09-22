using System.Globalization;
using System.Security.Cryptography;

namespace KspMission.Import;

public static class Importer
{
    public static ImportResult Import(ImportOptions options)
    {
        if (!string.Equals(options.Scale, "Real", StringComparison.Ordinal))
            throw new ImportException("This bounded adapter requires SystemScale=Real.");
        if (options.OptionalJnsq10X || options.JnsqDirectory.Replace('\\', '/').Contains("Rescale_10X", StringComparison.OrdinalIgnoreCase))
            throw new ImportException("Reborn Real and JNSQ optional 10X would double scale the system.");
        if (options.Principia is null) throw new ImportException("Explicit Principia on/off branch selection is required.");
        if (!Directory.Exists(options.RebornDirectory)) throw new ImportException($"Missing Reborn config directory: {options.RebornDirectory}");
        if (!Directory.Exists(options.JnsqDirectory)) throw new ImportException($"Missing JNSQ config directory: {options.JnsqDirectory}");
        var reborn = Directory.GetFiles(options.RebornDirectory, "*.cfg", SearchOption.AllDirectories).OrderBy(x => x, StringComparer.Ordinal).ToArray();
        var jnsq = Directory.GetFiles(options.JnsqDirectory, "*.cfg", SearchOption.AllDirectories).OrderBy(x => x, StringComparer.Ordinal).ToArray();
        if (reborn.Length == 0 || jnsq.Length == 0) throw new ImportException("Both pinned source config sets are required.");
        if (jnsq.Any(x => x.Replace('\\', '/').Contains("/Rescale_10X/", StringComparison.OrdinalIgnoreCase)))
            throw new ImportException("Reborn Real and JNSQ optional 10X would double scale the system.");
        var files = reborn.Select(x => MakeSource(x, "JNSQ-Reborn", "v1.0.1"))
            .Concat(jnsq.Select(x => MakeSource(x, "JNSQ", "0.10.2"))).ToList();
        if (options.RebornArchivePath is not null) files.Add(MakeSource(options.RebornArchivePath, "JNSQ-Reborn release ZIP", "v1.0.1"));
        if (options.JnsqArchivePath is not null) files.Add(MakeSource(options.JnsqArchivePath, "JNSQ release ZIP", "0.10.2"));
        var parsed = reborn.Select(ConfigParser.Parse).ToArray();
        var diagnostics = new List<string>
        {
            "Raw config catalog only: ModuleManager ordering, runtime KSP values and other installed patches are unverified.",
            "No Cartesian body state, common state epoch, inertial frame axes/origin/handedness or validated force model; analysis_ready=false.",
            "geeASL is retained as source evidence. Gravitational parameter is absent unless an explicit source value exists; no KSP gee conversion is assumed."
        };
        var configuration = ReadConfiguration(parsed, options.Principia.Value);
        var scaleSelection = configuration.Scale.Value == options.Scale ? "matches_source" : "requested_override_preview";
        if (scaleSelection == "requested_override_preview")
            diagnostics.Add($"Requested SystemScale={options.Scale} overrides hashed source config SystemScale={configuration.Scale.Value} for this provisional preview only; no game configuration was changed or loaded.");
        var importedCalendar = ReadImportedCalendar(parsed, configuration.RealTime, options.Principia.Value, diagnostics);
        var bodyNodes = parsed.SelectMany(x => x.Descendants())
            .Where(x => x.Name.Equals("Body", StringComparison.OrdinalIgnoreCase))
            .Where(x => x.Fields.Any(f => f.Key.Equals("name", StringComparison.OrdinalIgnoreCase)))
            .ToArray();
        if (bodyNodes.Length == 0) throw new ImportException("No Reborn base Body nodes found.");
        var bodies = bodyNodes.Select(x => ReadBody(x, options.Principia.Value, diagnostics)).ToArray();
        Validate(bodies);
        ApplyRealPatches(parsed, bodies, options.Principia.Value, diagnostics);
        ApplyRotationOffset(parsed, bodies, options.Principia.Value, diagnostics);
        ListUnresolvedPatches(parsed, diagnostics);
        // Source configurations are never a runtime state or a resolved ModuleManager cache.
        return new(1, "raw_config_provisional", false, "bounded_raw_config_selection", options.Scale,
            options.Principia.Value, "unresolved_config_force_model", null, null,
            bodies, files, diagnostics.Distinct(StringComparer.Ordinal).ToArray(), NoLeapCalendar.Create(86400, 0),
            configuration.Scale.Value, configuration.Scale, scaleSelection, importedCalendar, null, "requested_tool_display");
    }

    private static (SourceValue<string> Scale, SourceValue<bool>? RealTime) ReadConfiguration(ConfigNode[] parsed, bool principia)
    {
        var candidates = parsed.SelectMany(x => x.Children)
            .Where(x => x.Name.Equals("JNSQReborn_Configuration", StringComparison.OrdinalIgnoreCase)).ToArray();
        if (candidates.Length != 1) throw new ImportException("Exactly one Reborn JNSQReborn_Configuration node is required.");
        var node = candidates[0];
        var scaleField = Field(node, "SystemScale", principia)
            ?? throw new ImportException($"{node.File}:{node.Line}: SystemScale is missing.");
        if (scaleField.Value is not ("Standard" or "Real"))
            throw new ImportException($"{scaleField.File}:{scaleField.Line}: unsupported SystemScale '{scaleField.Value}'.");
        var realTimeField = Field(node, "RealTime", principia);
        SourceValue<bool>? realTime = null;
        if (realTimeField is not null)
        {
            if (!bool.TryParse(realTimeField.Value, out var value))
                throw new ImportException($"{realTimeField.File}:{realTimeField.Line}: RealTime must be true or false.");
            realTime = new(value, realTimeField.File, realTimeField.Line, realTimeField.Condition,
                Key: "RealTime", Unit: "boolean");
        }
        return (new(scaleField.Value, scaleField.File, scaleField.Line, scaleField.Condition,
            Key: "SystemScale", Unit: "named configuration"), realTime);
    }

    private static ImportedCalendarPreview ReadImportedCalendar(ConfigNode[] parsed, SourceValue<bool>? realTime,
        bool principia, List<string> diagnostics)
    {
        var nodes = parsed.SelectMany(x => x.Descendants()).ToArray();
        var baseDate = nodes.FirstOrDefault(x => x.Name == "PrintDate"
            && x.File.Replace('\\', '/').EndsWith("/JNSQReborn-Configuration.cfg", StringComparison.OrdinalIgnoreCase));
        var offsetYear = baseDate is null ? null : Number(baseDate, "offsetYear", principia)?.Value;
        var offsetDay = baseDate is null ? null : Number(baseDate, "offsetDay", principia)?.Value;
        var offsetTime = baseDate is null ? null : Number(baseDate, "offsetTime", principia)?.Value;
        var sourcePath = baseDate?.File;
        if (realTime?.Value == true)
        {
            var realPatch = nodes.FirstOrDefault(x => x.Name == "@PrintDate"
                && x.File.Replace('\\', '/').EndsWith("/Rescale/Configs/OffsetTime.cfg", StringComparison.OrdinalIgnoreCase));
            var patchedOffset = realPatch is null ? null : Number(realPatch, "@offsetTime", principia);
            if (patchedOffset is not null) { offsetTime = patchedOffset.Value; sourcePath = patchedOffset.File; }
        }
        IReadOnlyList<int>? monthLengths = null;
        var months = nodes.FirstOrDefault(x => x.Name == "Months"
            && x.File.Replace('\\', '/').EndsWith("/JNSQReborn-Configuration.cfg", StringComparison.OrdinalIgnoreCase));
        if (months is not null)
        {
            var lengths = months.Children.Where(x => x.Name == "Month")
                .Select(x => Number(x, "days", principia, true)!.Value).ToArray();
            if (lengths.Length == 12 && lengths.All(x => x > 0 && x == Math.Truncate(x)) && lengths.Sum() == 365)
                monthLengths = lengths.Select(x => (int)x).ToArray();
            else diagnostics.Add($"{months.File}:{months.Line}: imported Kronometer month lengths are incomplete or not a no-leap 365-day year.");
        }
        diagnostics.Add("Imported Kronometer settings are a conditional source preview; ModuleManager resolution, useLeapYears, home-day duration and game UT zero are unverified.");
        return new("conditional_raw_config_preview", false, realTime?.Value, realTime,
            offsetYear, offsetDay, offsetTime, monthLengths, null, null, sourcePath);
    }

    private static SourceFile MakeSource(string path, string pack, string version)
    {
        if (!File.Exists(path)) throw new ImportException($"Missing provenance file: {path}");
        using var stream = File.OpenRead(path);
        var hash = Convert.ToHexString(SHA256.HashData(stream)).ToLowerInvariant();
        return new(pack, version, Path.GetFullPath(path).Replace('\\', '/'), hash);
    }

    private static BodyPreview ReadBody(ConfigNode node, bool principia, List<string> diagnostics)
    {
        var id = Field(node, "name", principia)?.Value ?? throw new ImportException($"{node.File}:{node.Line}: Body ID missing.");
        if (!ValidId(id)) throw new ImportException($"{node.File}:{node.Line}: invalid body ID '{id}'.");
        var props = node.Child("Properties") ?? throw new ImportException($"{id}: Properties node missing.");
        var orbit = node.Child("Orbit");
        if (orbit is null && id is not ("Sun" or "JNSQSun"))
            throw new ImportException($"{id}: Orbit node missing for nonroot body.");
        var radius = Number(props, "radius", principia, true)!;
        if (radius.Value <= 0) throw new ImportException($"{id}.radius must be positive metres.");
        var gee = Number(props, "geeASL", principia);
        if (gee is { Value: <= 0 }) throw new ImportException($"{id}.geeASL must be positive.");
        var mu = Number(props, "gravParameter", principia);
        if (mu is { Value: <= 0 }) throw new ImportException($"{id}.gravParameter must be positive m^3/s^2.");
        var atmosphere = node.Child("Atmosphere");
        var altitude = atmosphere is null ? null : Number(atmosphere, "altitude", principia);
        if (altitude is { Value: < 0 }) throw new ImportException($"{id}.atmosphere.altitude must be nonnegative metres.");
        var parent = orbit is null ? null : Field(orbit, "referenceBody", principia)?.Value;
        if (parent is not null && !ValidId(parent)) throw new ImportException($"{id}: invalid parent ID '{parent}'.");
        var axis = orbit is null ? null : Number(orbit, "semiMajorAxis", principia);
        if (axis is { Value: <= 0 }) throw new ImportException($"{id}.orbit.semiMajorAxis must be positive metres.");
        if (orbit is not null && axis is null) diagnostics.Add($"{id}: orbit semiMajorAxis is absent after Principia branch selection.");
        var locked = Boolean(props, "tidallyLocked", principia);
        var periodSource = Number(props, "rotationPeriod", principia);
        var initialSource = Number(props, "initialRotation", principia);
        var rotation = new RotationPreview(Boolean(props, "rotates", principia), locked,
            periodSource?.Value, initialSource?.Value,
            "Kopernicus config orientation; inertial orientation unresolved", periodSource, initialSource);
        var anomaly = orbit is null ? null : MeanAnomaly(orbit, principia);
        var elementEpoch = orbit is null ? null : Number(orbit, "epoch", principia, true);
        var eccentricity = orbit is null ? null : Number(orbit, "eccentricity", principia);
        var inclination = orbit is null ? null : Number(orbit, "inclination", principia);
        var nodeLongitude = orbit is null ? null : Number(orbit, "longitudeOfAscendingNode", principia);
        var periapsisArgument = orbit is null ? null : Number(orbit, "argumentOfPeriapsis", principia);
        var orbitPreview = orbit is null ? null : new OrbitPreview(
            principia ? "Principia Jacobi fallback elements; not Cartesian" : "Kopernicus orbit elements; not Cartesian",
            axis?.Value, axis, eccentricity?.Value,
            eccentricity is null ? null : eccentricity with { Key = "eccentricity", Unit = "dimensionless" },
            inclination?.Value,
            inclination is null ? null : inclination with { Key = "inclination", Unit = "degrees" },
            nodeLongitude?.Value,
            nodeLongitude is null ? null : nodeLongitude with { Key = "longitudeOfAscendingNode", Unit = "degrees" },
            periapsisArgument?.Value,
            periapsisArgument is null ? null : periapsisArgument with { Key = "argumentOfPeriapsis", Unit = "degrees" },
            anomaly?.Degrees, anomaly?.Source, elementEpoch?.Value,
            elementEpoch is null ? null : elementEpoch with { Key = "epoch", Unit = "UT seconds" }, parent);
        return new(id, Field(props, "displayName", principia)?.Value, parent, radius.Value, radius, gee?.Value, gee,
            mu?.Value, mu, altitude?.Value, altitude, rotation, orbitPreview, node.File);
    }

    private static bool ValidId(string value) => value.Length > 0 && (char.IsLetter(value[0]) || value[0] == '_')
        && value.All(c => char.IsLetterOrDigit(c) || c == '_');

    private static (double Degrees, SourceValue<double> Source)? MeanAnomaly(ConfigNode orbit, bool principia)
    {
        var radians = Number(orbit, "meanAnomalyAtEpoch", principia);
        var degrees = Number(orbit, "meanAnomalyAtEpochD", principia);
        if (radians is not null && degrees is not null)
            throw new ImportException($"{orbit.File}:{orbit.Line}: both radian and degree mean anomaly fields are active.");
        if (radians is not null)
            return (radians.Value * 180d / Math.PI,
                radians with { Key = "meanAnomalyAtEpoch", Unit = "radians" });
        if (degrees is not null)
            return (degrees.Value, degrees with { Key = "meanAnomalyAtEpochD", Unit = "degrees" });
        return null;
    }

    private static void ApplyRealPatches(ConfigNode[] parsed, BodyPreview[] bodies, bool principia, List<string> diagnostics)
    {
        var byId = bodies.Select((body, index) => (body, index)).ToDictionary(x => x.body.Id, x => x.index, StringComparer.Ordinal);
        var patched = new HashSet<string>(StringComparer.Ordinal);
        foreach (var root in parsed)
        foreach (var node in root.Descendants().Where(x => x.Name.StartsWith("@Body[", StringComparison.Ordinal)))
        {
            // Only this pinned pack's Real rescale files are interpreted here.
            if (!node.File.Replace('\\', '/').Contains("/Bodies/Rescale/", StringComparison.OrdinalIgnoreCase)) continue;
            var close = node.Name.IndexOf(']');
            if (close < 0) { diagnostics.Add($"{node.File}:{node.Line}: unhandled body patch {node.Name}."); continue; }
            var id = node.Name[6..close];
            if (!byId.TryGetValue(id, out var index)) { diagnostics.Add($"{node.File}:{node.Line}: Real patch targets unknown body {id}."); continue; }
            if (!patched.Add(id)) throw new ImportException($"{id}: duplicate Real rescale body patch.");
            var body = bodies[index];
            var properties = node.Child("@Properties");
            var orbit = node.Child("@Orbit");
            var atmosphere = node.Child("@Atmosphere");
            if (properties is not null)
            {
                var radius = PatchNumber(body.RadiusSource, properties, "radius", principia, diagnostics);
                var period = PatchNumber(body.Rotation.PeriodSource, properties, "rotationPeriod", principia, diagnostics);
                body = body with { RadiusM = radius?.Value ?? body.RadiusM,
                    RadiusSource = radius ?? body.RadiusSource,
                    Rotation = body.Rotation with { PeriodS = period?.Value ?? body.Rotation.PeriodS, PeriodSource = period } };
            }
            if (orbit is not null && body.Orbit is not null)
            {
                var axis = PatchNumber(body.Orbit.SemiMajorAxisSource, orbit, "semiMajorAxis", principia, diagnostics);
                body = body with { Orbit = body.Orbit with { SemiMajorAxisM = axis?.Value, SemiMajorAxisSource = axis } };
            }
            if (atmosphere is not null)
            {
                var altitude = PatchNumber(body.AtmosphereAltitudeSource, atmosphere, "altitude", principia, diagnostics);
                body = body with { AtmosphereAltitudeM = altitude?.Value ?? body.AtmosphereAltitudeM,
                    AtmosphereAltitudeSource = altitude ?? body.AtmosphereAltitudeSource };
            }
            bodies[index] = body;
        }
        foreach (var body in bodies)
            if (!patched.Contains(body.Id)) diagnostics.Add($"{body.Id}: no Real rescale patch found; values remain raw source fields.");
    }

    private static void ApplyRotationOffset(ConfigNode[] parsed, BodyPreview[] bodies, bool principia, List<string> diagnostics)
    {
        foreach (var root in parsed)
        foreach (var node in root.Descendants().Where(x => x.Name.StartsWith("@Body[", StringComparison.Ordinal)
            && x.File.Replace('\\', '/').EndsWith("/Configs/OffsetTime.cfg", StringComparison.OrdinalIgnoreCase)))
        {
            var close = node.Name.IndexOf(']');
            if (close < 0) continue;
            var id = node.Name[6..close];
            var index = Array.FindIndex(bodies, x => x.Id == id);
            if (index < 0) { diagnostics.Add($"{node.File}:{node.Line}: rotation patch targets unknown body {id}."); continue; }
            var properties = node.Child("@Properties");
            if (properties is null) continue;
            var body = bodies[index];
            var initial = PatchNumber(body.Rotation.InitialRotationSource, properties, "initialRotation", principia, diagnostics);
            bodies[index] = body with { Rotation = body.Rotation with { InitialRotationDeg = initial?.Value, InitialRotationSource = initial } };
        }
    }

    private static void ListUnresolvedPatches(ConfigNode[] parsed, List<string> diagnostics)
    {
        string[] physical = ["radius", "geeASL", "gravParameter", "semiMajorAxis", "rotationPeriod", "initialRotation", "tidallyLocked", "altitude"];
        string[] calendar = ["offsetTime", "offsetDay", "offsetYear", "useLeapYears", "useHomeDay", "useHomeYear"];
        foreach (var root in parsed)
        foreach (var node in root.Descendants())
        foreach (var field in node.Fields)
        {
            if (!field.Key.StartsWith('@')) continue;
            var name = field.Key[1..].TrimEnd(' ', '*', '+');
            var isPhysical = physical.Contains(name, StringComparer.OrdinalIgnoreCase)
                && ((node.Name == "@Properties" && name is "radius" or "geeASL" or "gravParameter" or "rotationPeriod" or "initialRotation" or "tidallyLocked")
                    || (node.Name == "@Orbit" && name == "semiMajorAxis")
                    || (node.Name == "@Atmosphere" && name == "altitude"));
            var isCalendar = calendar.Contains(name, StringComparer.OrdinalIgnoreCase);
            if (!isPhysical && !isCalendar) continue;
            var normalized = field.File.Replace('\\', '/');
            var knownReal = normalized.Contains("/Bodies/Rescale/", StringComparison.OrdinalIgnoreCase)
                && ((node.Name == "@Properties" && name is "radius" or "rotationPeriod")
                    || (node.Name == "@Orbit" && name == "semiMajorAxis")
                    || (node.Name == "@Atmosphere" && name == "altitude"));
            var knownRotation = normalized.EndsWith("/Configs/OffsetTime.cfg", StringComparison.OrdinalIgnoreCase)
                && node.Name == "@Properties" && name == "initialRotation";
            if (knownReal || knownRotation) continue;
            diagnostics.Add($"{field.File}:{field.Line}: unresolved ModuleManager patch {node.Name}.{field.Key} affecting {(isCalendar ? "calendar" : "physical")} data.");
        }
    }

    private static SourceValue<double>? PatchNumber(SourceValue<double>? original, ConfigNode node, string key, bool principia, List<string> diagnostics)
    {
        var operations = node.Fields.Where(x => x.Key.Equals("@" + key, StringComparison.OrdinalIgnoreCase)
            || x.Key.Equals("@" + key + " *", StringComparison.OrdinalIgnoreCase)
            || x.Key.Equals("@" + key + " +", StringComparison.OrdinalIgnoreCase))
            .Where(x => Applies(x.Condition, principia)).ToArray();
        var current = original;
        foreach (var op in operations)
        {
            if (!double.TryParse(op.Value, NumberStyles.Float, CultureInfo.InvariantCulture, out var operand) || !double.IsFinite(operand))
                throw new ImportException($"{op.File}:{op.Line}: invalid {key} patch numeric value.");
            if ((op.Key.EndsWith(" *", StringComparison.Ordinal) || op.Key.EndsWith(" +", StringComparison.Ordinal)) && current is null)
            {
                diagnostics.Add($"{op.File}:{op.Line}: cannot multiply missing {key} field."); continue;
            }
            var value = op.Key.EndsWith(" *", StringComparison.Ordinal) ? current!.Value * operand
                : op.Key.EndsWith(" +", StringComparison.Ordinal) ? current!.Value + operand : operand;
            if (!double.IsFinite(value) || (key == "rotationPeriod" ? value == 0 : key == "initialRotation" ? false
                : key == "altitude" ? value < 0 : value <= 0))
                throw new ImportException($"{op.File}:{op.Line}: {key} patch yields invalid SI value.");
            current = new(value, op.File, op.Line, "SystemScale=Real", current?.BaseFile ?? current?.File,
                current?.BaseLine ?? current?.Line, op.Key.EndsWith(" *", StringComparison.Ordinal) ? $"multiply by {operand}"
                : op.Key.EndsWith(" +", StringComparison.Ordinal) ? $"add {operand}" : "assign", key, UnitFor(key));
        }
        return current;
    }

    private static ConfigField? Field(ConfigNode node, string key, bool principia)
    {
        var matching = node.Fields.Where(x => x.Key.Equals(key, StringComparison.OrdinalIgnoreCase)).ToArray();
        var chosen = matching.Where(x => Applies(x.Condition, principia)).ToArray();
        if (chosen.Length > 1 && chosen.Any(x => x.Value != chosen[0].Value))
            throw new ImportException($"{node.File}:{chosen[1].Line}: ambiguous {node.Name}.{key} branch.");
        return chosen.FirstOrDefault();
    }

    private static bool Applies(string? condition, bool principia) => condition switch
    {
        null => true,
        "Principia" => principia,
        "!Principia" => !principia,
        _ => false
    };

    private static SourceValue<double>? Number(ConfigNode node, string key, bool principia, bool required = false)
    {
        var field = Field(node, key, principia);
        if (field is null)
        {
            if (required) throw new ImportException($"{node.File}:{node.Line}: missing {node.Name}.{key}.");
            return null;
        }
        if (!double.TryParse(field.Value, NumberStyles.Float, CultureInfo.InvariantCulture, out var parsed) || !double.IsFinite(parsed))
            throw new ImportException($"{field.File}:{field.Line}: {node.Name}.{key} requires finite SI numeric value, got '{field.Value}'.");
        return new(parsed, field.File, field.Line, field.Condition, Key: key, Unit: UnitFor(key));
    }

    private static string? UnitFor(string key) => key switch
    {
        "radius" or "semiMajorAxis" or "altitude" => "metres",
        "rotationPeriod" or "epoch" or "offsetTime" or "@offsetTime" => "seconds",
        "gravParameter" => "m^3/s^2",
        "meanAnomalyAtEpoch" => "radians",
        "inclination" or "longitudeOfAscendingNode" or "argumentOfPeriapsis" or "meanAnomalyAtEpochD" or "initialRotation" => "degrees",
        "eccentricity" or "geeASL" => "dimensionless",
        "offsetYear" => "years",
        "offsetDay" or "days" => "days",
        _ => null
    };

    private static bool? Boolean(ConfigNode node, string key, bool principia)
    {
        var field = Field(node, key, principia);
        if (field is null) return null;
        if (!bool.TryParse(field.Value, out var value)) throw new ImportException($"{field.File}:{field.Line}: {node.Name}.{key} must be true or false.");
        return value;
    }

    private static void Validate(BodyPreview[] bodies)
    {
        var ids = new HashSet<string>(StringComparer.Ordinal);
        foreach (var body in bodies)
            if (!ids.Add(body.Id)) throw new ImportException($"Duplicate body ID: {body.Id}.");
        var byId = bodies.ToDictionary(x => x.Id, StringComparer.Ordinal);
        if (bodies.Count(x => x.ParentId is null) != 1)
            throw new ImportException("Catalog requires exactly one root body.");
        foreach (var body in bodies)
            if (body.ParentId is not null && !byId.ContainsKey(body.ParentId))
                throw new ImportException($"{body.Id}: missing parent {body.ParentId}.");
        foreach (var body in bodies)
        {
            var seen = new HashSet<string>(StringComparer.Ordinal);
            var current = body;
            while (current.ParentId is not null)
            {
                if (!seen.Add(current.Id)) throw new ImportException($"{body.Id}: parent cycle detected.");
                current = byId[current.ParentId];
            }
        }
    }
}
