using KspMission.Snapshot;

if (args.Length != 1)
{
    Console.Error.WriteLine("Usage: KspMission.Snapshot.Tool <runtime-snapshot.json>");
    return 2;
}
try
{
    var snapshot = SnapshotReader.Read(File.ReadAllText(args[0]));
    Console.WriteLine($"Schema valid: {snapshot.Bodies.Count} bodies at UT {snapshot.Capture.CaptureUtS:R} s");
    Console.WriteLine($"Confidence claim: {snapshot.Confidence} (not independently verified)");
    Console.WriteLine($"Source SHA-256: {snapshot.SourceSha256}");
    Console.WriteLine($"Frame: {snapshot.Frame.Origin}, {snapshot.Frame.Axes}, {snapshot.Frame.Handedness}");
    return 0;
}
catch (Exception e) when (e is SnapshotException or IOException or UnauthorizedAccessException)
{
    Console.Error.WriteLine(e.Message);
    return 1;
}
