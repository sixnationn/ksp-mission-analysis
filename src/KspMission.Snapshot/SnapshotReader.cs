using System.Security.Cryptography;
using System.Text;
using System.Text.Json;

namespace KspMission.Snapshot;

public sealed class SnapshotException(string message) : Exception(message);
public sealed record RuntimeState(double[] PositionM, double[] VelocityMps);
public sealed record RuntimeBody(string Id, string? ParentId, double MuM3S2, double RadiusM,
    double? AtmosphereBoundaryM, double StateEpochUtS, RuntimeState State);
public sealed record RuntimeFrame(string Origin, string Axes, string Handedness, bool Inertial,
    string SourceFrame, string TransformMethod, string TransformVersion);
public sealed record RuntimeCapture(string ExporterId, string ExporterVersion, string GameVersion,
    string SaveId, IReadOnlyDictionary<string, string> Mods, double CaptureUtS, bool PrincipiaLoaded,
    string StateSource);
public sealed record RuntimeCalendar(double DayDurationS, double DisplayOriginUtS, IReadOnlyList<int> MonthLengths);
public sealed record RuntimeSnapshot(string SourceSha256, string Confidence, RuntimeCapture Capture,
    RuntimeFrame Frame, IReadOnlyList<RuntimeBody> Bodies, RuntimeCalendar Calendar)
{
    // Structural eligibility only. A JSON file cannot prove an actual KSP capture.
    public bool CanSeedIndependentNBody => true;
}

public static class SnapshotReader
{
    public static RuntimeSnapshot Read(string json)
    {
        if (string.IsNullOrWhiteSpace(json)) throw new SnapshotException("Runtime snapshot JSON is empty.");
        JsonDocument document;
        try { document = JsonDocument.Parse(json); }
        catch (JsonException e) { throw new SnapshotException($"Invalid runtime snapshot JSON: {e.Message}"); }
        using (document)
        {
            var root = document.RootElement;
            if (root.ValueKind != JsonValueKind.Object) throw new SnapshotException("Runtime snapshot root must be an object.");
            if (Integer(root, "schema_version", "snapshot") != 1)
                throw new SnapshotException("Unsupported snapshot schema_version.");
            var confidence = Text(root, "confidence", "snapshot");
            if (confidence == "runtime_verified")
                throw new SnapshotException("runtime_verified requires external comparison evidence, which this reader cannot validate.");
            if (confidence != "runtime_observed_uncompared")
                throw new SnapshotException("Runtime snapshot confidence must be runtime_observed_uncompared.");

            var capture = ReadCapture(Object(root, "capture", "snapshot"));
            var frame = ReadFrame(Object(root, "frame", "snapshot"));
            var bodies = ReadBodies(Array(root, "bodies", "snapshot"), capture.CaptureUtS);
            var calendar = ReadCalendar(Object(root, "calendar", "snapshot"));
            CheckBarycenter(bodies);
            return new RuntimeSnapshot(
                Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(json))).ToLowerInvariant(),
                confidence, capture, frame, bodies, calendar);
        }
    }

    private static RuntimeCapture ReadCapture(JsonElement value)
    {
        const string at = "capture";
        var exporterId = Text(value, "exporter_id", at);
        var exporterVersion = Text(value, "exporter_version", at);
        var gameVersion = Text(value, "game_version", at);
        var saveId = Text(value, "save_id", at);
        var mods = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var mod in Array(value, "mods", at).EnumerateArray())
        {
            if (mod.ValueKind != JsonValueKind.Object) throw new SnapshotException("capture.mods entry must be an object.");
            var id = Text(mod, "id", "capture.mods");
            if (!mods.TryAdd(id, Text(mod, "version", "capture.mods")))
                throw new SnapshotException($"Duplicate capture.mods id '{id}'.");
        }
        if (mods.Count == 0) throw new SnapshotException("capture.mods must identify loaded mods.");
        var ut = Number(value, "capture_ut_s", at);
        var principia = Boolean(value, "principia_loaded", at);
        var source = Text(value, "state_source", at);
        if (!principia || !mods.ContainsKey("Principia"))
            throw new SnapshotException("This Principia runtime snapshot requires principia_loaded and a Principia mod version.");
        if (source != "principia_celestial_from_parent")
            throw new SnapshotException("Unsupported capture.state_source for the Principia frame contract.");
        return new(exporterId, exporterVersion, gameVersion, saveId, mods, ut, principia, source);
    }

    private static RuntimeFrame ReadFrame(JsonElement value)
    {
        const string at = "frame";
        var origin = Text(value, "origin", at);
        var axes = Text(value, "axes", at);
        var handedness = Text(value, "handedness", at);
        var inertial = Boolean(value, "inertial", at);
        var source = Text(value, "source_frame", at);
        var method = Text(value, "transform_method", at);
        var version = Text(value, "transform_version", at);
        if (origin != "system_barycenter") throw new SnapshotException("frame.origin must be system_barycenter.");
        if (axes != "principia_alicesun_frozen_at_capture") throw new SnapshotException("Unsupported frame.axes.");
        if (handedness != "right") throw new SnapshotException("frame.handedness must be right.");
        if (!inertial) throw new SnapshotException("frame.inertial must be true.");
        if (source != "Principia/AliceSun") throw new SnapshotException("frame.source_frame must be Principia/AliceSun.");
        if (method != "parent_relative_sum_then_com_translation")
            throw new SnapshotException("Unsupported frame.transform_method.");
        if (version != "1") throw new SnapshotException("Unsupported frame.transform_version.");
        return new(origin, axes, handedness, inertial, source, method, version);
    }

    private static IReadOnlyList<RuntimeBody> ReadBodies(JsonElement values, double captureUtS)
    {
        var bodies = new List<RuntimeBody>();
        var ids = new HashSet<string>(StringComparer.Ordinal);
        foreach (var item in values.EnumerateArray())
        {
            if (item.ValueKind != JsonValueKind.Object) throw new SnapshotException("bodies entry must be an object.");
            var id = Text(item, "id", "bodies");
            if (!ids.Add(id)) throw new SnapshotException($"Duplicate body id '{id}'.");
            var parentValue = Field(item, "parent_id", $"body {id}");
            var parent = parentValue.ValueKind == JsonValueKind.Null ? null : Text(item, "parent_id", $"body {id}");
            var mu = Positive(Number(item, "mu_m3_s2", $"body {id}"), $"body {id}.mu_m3_s2");
            var radius = Positive(Number(item, "radius_m", $"body {id}"), $"body {id}.radius_m");
            var atmosphereValue = Field(item, "atmosphere_boundary_m", $"body {id}");
            double? atmosphere = atmosphereValue.ValueKind == JsonValueKind.Null ? null :
                Number(item, "atmosphere_boundary_m", $"body {id}");
            if (atmosphere < 0) throw new SnapshotException($"body {id}.atmosphere_boundary_m must be nonnegative.");
            var epoch = Number(item, "state_epoch_ut_s", $"body {id}");
            if (epoch != captureUtS) throw new SnapshotException($"body {id} state epoch differs from capture.capture_ut_s.");
            var position = Vector(item, "position_m", $"body {id}");
            var velocity = Vector(item, "velocity_mps", $"body {id}");
            bodies.Add(new(id, parent, mu, radius, atmosphere, epoch, new(position, velocity)));
        }
        if (bodies.Count < 2) throw new SnapshotException("Runtime snapshot requires at least two bodies.");
        var byId = bodies.ToDictionary(b => b.Id, StringComparer.Ordinal);
        foreach (var body in bodies)
        {
            var visited = new HashSet<string>(StringComparer.Ordinal);
            var cursor = body;
            while (cursor.ParentId is { } parent)
            {
                if (!visited.Add(cursor.Id)) throw new SnapshotException($"Body parent cycle at '{cursor.Id}'.");
                if (!byId.TryGetValue(parent, out var next))
                    throw new SnapshotException($"Body '{cursor.Id}' has missing parent '{parent}'.");
                cursor = next;
            }
        }
        if (bodies.Count(b => b.ParentId is null) != 1)
            throw new SnapshotException("Runtime snapshot must have exactly one root body.");
        for (var i = 0; i < bodies.Count; ++i)
        for (var j = i + 1; j < bodies.Count; ++j)
        {
            var a = bodies[i]; var b = bodies[j];
            var delta = a.State.PositionM.Zip(b.State.PositionM, (x, y) => x - y).ToArray();
            var distance = Math.Sqrt(delta.Sum(x => x * x));
            if (!double.IsFinite(distance) || distance <= a.RadiusM + b.RadiusM)
                throw new SnapshotException($"Bodies '{a.Id}' and '{b.Id}' overlap.");
        }
        return bodies;
    }

    private static RuntimeCalendar ReadCalendar(JsonElement value)
    {
        const string at = "calendar";
        var day = Positive(Number(value, "day_duration_s", at), "calendar.day_duration_s");
        var origin = Number(value, "display_origin_ut_s", at);
        if (Boolean(value, "use_leap_years", at)) throw new SnapshotException("calendar.use_leap_years must be false.");
        var months = Array(value, "month_lengths", at).EnumerateArray().Select(x =>
        {
            if (!x.TryGetInt32(out var days)) throw new SnapshotException("calendar.month_lengths must contain integers.");
            return days;
        }).ToArray();
        if (months.Length != 12 || months.Any(x => x <= 0) || months.Sum() != 365)
            throw new SnapshotException("calendar.month_lengths must contain 12 positive months totaling 365 days.");
        return new(day, origin, months);
    }

    private static void CheckBarycenter(IReadOnlyList<RuntimeBody> bodies)
    {
        var total = bodies.Sum(b => b.MuM3S2);
        if (!double.IsFinite(total) || total <= 0) throw new SnapshotException("Body gravitational parameters overflow barycenter sum.");
        for (var axis = 0; axis < 3; ++axis)
        {
            var position = bodies.Sum(b => (b.MuM3S2 / total) * b.State.PositionM[axis]);
            var velocity = bodies.Sum(b => (b.MuM3S2 / total) * b.State.VelocityMps[axis]);
            var positionScale = bodies.Max(b => Math.Abs(b.State.PositionM[axis]));
            var velocityScale = bodies.Max(b => Math.Abs(b.State.VelocityMps[axis]));
            if (Math.Abs(position) > Math.Max(1, positionScale * 1e-12) ||
                Math.Abs(velocity) > Math.Max(1e-6, velocityScale * 1e-12))
                throw new SnapshotException("Body states do not have the declared system_barycenter origin.");
        }
    }

    private static JsonElement Field(JsonElement value, string name, string at)
    {
        if (value.ValueKind != JsonValueKind.Object || !value.TryGetProperty(name, out var field))
            throw new SnapshotException($"Missing {at}.{name}.");
        return field;
    }
    private static JsonElement Object(JsonElement value, string name, string at)
    {
        var field = Field(value, name, at);
        if (field.ValueKind != JsonValueKind.Object) throw new SnapshotException($"{at}.{name} must be an object.");
        return field;
    }
    private static JsonElement Array(JsonElement value, string name, string at)
    {
        var field = Field(value, name, at);
        if (field.ValueKind != JsonValueKind.Array) throw new SnapshotException($"{at}.{name} must be an array.");
        return field;
    }
    private static string Text(JsonElement value, string name, string at)
    {
        var field = Field(value, name, at);
        if (field.ValueKind != JsonValueKind.String || string.IsNullOrWhiteSpace(field.GetString()))
            throw new SnapshotException($"{at}.{name} must be nonempty text.");
        return field.GetString()!;
    }
    private static double Number(JsonElement value, string name, string at)
    {
        var field = Field(value, name, at);
        if (field.ValueKind != JsonValueKind.Number || !field.TryGetDouble(out var number) || !double.IsFinite(number))
            throw new SnapshotException($"{at}.{name} must be a finite number.");
        return number;
    }
    private static int Integer(JsonElement value, string name, string at)
    {
        var field = Field(value, name, at);
        if (field.ValueKind != JsonValueKind.Number || !field.TryGetInt32(out var number))
            throw new SnapshotException($"{at}.{name} must be an integer.");
        return number;
    }
    private static bool Boolean(JsonElement value, string name, string at)
    {
        var field = Field(value, name, at);
        if (field.ValueKind is not (JsonValueKind.True or JsonValueKind.False))
            throw new SnapshotException($"{at}.{name} must be true or false.");
        return field.GetBoolean();
    }
    private static double[] Vector(JsonElement value, string name, string at)
    {
        var field = Array(value, name, at);
        var parts = field.EnumerateArray().ToArray();
        if (parts.Length != 3) throw new SnapshotException($"{at}.{name} must contain three coordinates.");
        return parts.Select((x, i) =>
        {
            if (x.ValueKind != JsonValueKind.Number || !x.TryGetDouble(out var number) || !double.IsFinite(number))
                throw new SnapshotException($"{at}.{name}[{i}] must be finite.");
            return number;
        }).ToArray();
    }
    private static double Positive(double number, string field)
    {
        if (number <= 0) throw new SnapshotException($"{field} must be positive.");
        return number;
    }
}
