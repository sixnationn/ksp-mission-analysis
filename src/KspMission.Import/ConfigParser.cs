namespace KspMission.Import;

internal sealed record ConfigField(string Key, string Value, string File, int Line, string? Condition);
internal sealed class ConfigNode(string name, string file, int line)
{
    public string Name { get; } = name;
    public string File { get; } = file;
    public int Line { get; } = line;
    public List<ConfigNode> Children { get; } = [];
    public List<ConfigField> Fields { get; } = [];
    public IEnumerable<ConfigNode> Descendants()
    {
        foreach (var child in Children) { yield return child; foreach (var descendant in child.Descendants()) yield return descendant; }
    }
    public ConfigNode? Child(string name) => Children.FirstOrDefault(x => x.Name.Equals(name, StringComparison.OrdinalIgnoreCase));
}

internal static class ConfigParser
{
    public static ConfigNode Parse(string path)
    {
        var root = new ConfigNode("ROOT", path, 0);
        var stack = new Stack<ConfigNode>(); stack.Push(root);
        string? pendingName = null; int pendingLine = 0;
        var lines = File.ReadAllLines(path);
        for (var index = 0; index < lines.Length; index++)
        {
            var line = lines[index].Split("//", 2)[0].Trim();
            if (line.Length == 0) continue;
            if (line == "{")
            {
                if (pendingName is null) throw new ImportException($"{path}:{index + 1}: unnamed node.");
                var node = new ConfigNode(pendingName, path, pendingLine);
                stack.Peek().Children.Add(node); stack.Push(node); pendingName = null;
            }
            else if (line == "}")
            {
                if (stack.Count == 1) throw new ImportException($"{path}:{index + 1}: unexpected closing brace.");
                stack.Pop();
            }
            else if (line.Contains('='))
            {
                var equal = line.IndexOf('=');
                var key = line[..equal].Trim();
                var value = line[(equal + 1)..].Trim();
                string? condition = null;
                var needs = key.IndexOf(":NEEDS[", StringComparison.OrdinalIgnoreCase);
                if (needs >= 0)
                {
                    var end = key.IndexOf(']', needs);
                    if (end < 0) throw new ImportException($"{path}:{index + 1}: malformed NEEDS condition.");
                    condition = key[(needs + 7)..end]; key = key[..needs] + key[(end + 1)..];
                }
                stack.Peek().Fields.Add(new ConfigField(key, value, path, index + 1, condition));
            }
            else { pendingName = line; pendingLine = index + 1; }
        }
        if (stack.Count != 1) throw new ImportException($"{path}: unclosed node.");
        return root;
    }
}
