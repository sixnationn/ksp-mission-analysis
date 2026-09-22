using System.Text.Json.Nodes;
using KspMission.Snapshot;

var checks = 0;
void Check(bool condition, string name)
{
    if (!condition) throw new Exception(name);
    checks++;
}
void Reject(Action<JsonNode> edit, string expected)
{
    var node = JsonNode.Parse(Fixture())!;
    edit(node);
    try { SnapshotReader.Read(node.ToJsonString()); }
    catch (SnapshotException e)
    {
        Check(e.Message.Contains(expected, StringComparison.OrdinalIgnoreCase), $"Expected '{expected}', got '{e.Message}'");
        return;
    }
    throw new Exception($"Accepted invalid snapshot: {expected}");
}

var loaded = SnapshotReader.Read(Fixture());
Check(loaded.Bodies.Count == 2 && loaded.Bodies[1].Id == "Planet", "valid body list");
Check(loaded.SourceSha256.Length == 64 && loaded.CanSeedIndependentNBody, "source hash and model gate");
Check(loaded.Confidence == "runtime_observed_uncompared", "no confidence promotion");
Check(SnapshotReader.Read(Fixture().Replace("Planet", "PlanetB", StringComparison.Ordinal)).SourceSha256 != loaded.SourceSha256,
    "changed bytes change cache identity");

Reject(n => n["confidence"] = "raw_config_provisional", "confidence");
Reject(n => n["confidence"] = "runtime_verified", "comparison");
Reject(n => n["capture"]!["save_id"] = "", "save_id");
Reject(n => n["capture"]!["mods"] = new JsonArray(), "mods");
Reject(n => n["capture"]!["capture_ut_s"] = null, "capture_ut_s");
Reject(n => n["capture"]!["state_source"] = "ksp_orbits", "state_source");
Reject(n => n["frame"]!["handedness"] = "left", "handedness");
Reject(n => n["frame"]!["inertial"] = false, "inertial");
Reject(n => n["frame"]!["source_frame"] = "World", "source_frame");
Reject(n => n["frame"]!["transform_method"] = "none", "transform_method");
Reject(n => n["frame"]!["transform_version"] = "", "transform_version");
Reject(n => n["bodies"]![1]!["id"] = "Sun", "duplicate");
Reject(n => n["bodies"]![1]!["parent_id"] = "Missing", "parent");
Reject(n => n["bodies"]![0]!["parent_id"] = "Planet", "cycle");
Reject(n => n["bodies"]![1]!["state_epoch_ut_s"] = 1, "epoch");
Reject(n => n["bodies"]![1]!["mu_m3_s2"] = -1, "mu_m3_s2");
Reject(n => n["bodies"]![1]!["radius_m"] = 0, "radius_m");
Reject(n => n["bodies"]![1]!["atmosphere_boundary_m"] = -1, "atmosphere");
Reject(n => n["bodies"]![1]!["position_m"] = new JsonArray(-1000000000, 0, 0), "overlap");
Reject(n => n["calendar"]!["day_duration_s"] = 0, "day_duration_s");
Reject(n => n["calendar"]!["display_origin_ut_s"] = null, "display_origin_ut_s");
Reject(n => n["calendar"]!["month_lengths"]![1] = 29, "month_lengths");

Console.WriteLine($"PASS {checks} runtime snapshot checks");

static string Fixture() => """
{
  "schema_version": 1,
  "confidence": "runtime_observed_uncompared",
  "capture": {
    "exporter_id": "KspMission.RuntimeExporter",
    "exporter_version": "1",
    "game_version": "1.12.5",
    "save_id": "synthetic-test-save",
    "mods": [
      {"id":"Principia","version":"2026091103-Levy-test"},
      {"id":"JNSQ","version":"0.10.2"},
      {"id":"JNSQ-Reborn","version":"1.0.1"}
    ],
    "capture_ut_s": 0,
    "principia_loaded": true,
    "state_source": "principia_celestial_from_parent"
  },
  "frame": {
    "origin": "system_barycenter",
    "axes": "principia_alicesun_frozen_at_capture",
    "handedness": "right",
    "inertial": true,
    "source_frame": "Principia/AliceSun",
    "transform_method": "parent_relative_sum_then_com_translation",
    "transform_version": "1"
  },
  "bodies": [
    {"id":"Sun","parent_id":null,"mu_m3_s2":1e20,"radius_m":1e7,"atmosphere_boundary_m":null,"state_epoch_ut_s":0,"position_m":[-1e9,0,0],"velocity_mps":[-300,0,0]},
    {"id":"Planet","parent_id":"Sun","mu_m3_s2":1e18,"radius_m":1e6,"atmosphere_boundary_m":100000,"state_epoch_ut_s":0,"position_m":[1e11,0,0],"velocity_mps":[30000,0,0]}
  ],
  "calendar": {"day_duration_s":86400,"display_origin_ut_s":0,"use_leap_years":false,"month_lengths":[31,28,31,30,31,30,31,31,30,31,30,31]}
}
""";
