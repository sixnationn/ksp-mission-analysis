using System.Text.Json;
using KspMission.Import;

if ((args.Length != 8 && args.Length != 12) || args[0] != "--reborn" || args[2] != "--jnsq" || args[4] != "--scale" || args[6] != "--principia"
    || (args.Length == 12 && (args[8] != "--reborn-archive" || args[10] != "--jnsq-archive")))
{
    Console.Error.WriteLine("Usage: KspMission.Import --reborn <Reborn config directory> --jnsq <JNSQ config directory> --scale Real --principia on|off [--reborn-archive <ZIP> --jnsq-archive <ZIP>]");
    return 2;
}
try
{
    bool? principia = args[7] switch { "on" => true, "off" => false, _ => null };
    var catalog = Importer.Import(new(args[1], args[3], args[5], principia, false,
        args.Length == 12 ? args[9] : null, args.Length == 12 ? args[11] : null));
    Console.WriteLine(JsonSerializer.Serialize(catalog, new JsonSerializerOptions { WriteIndented = true }));
    return 0;
}
catch (ImportException e)
{
    Console.Error.WriteLine(e.Message);
    return 1;
}
