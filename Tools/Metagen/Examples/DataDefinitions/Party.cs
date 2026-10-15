using System.ComponentModel;
using Metagen;
using Metagen.PortableData;
namespace MetagenExamples;

[MetaSchema("party.mode", 1, "mode")]
public enum Mode {
    [MetaField(1, "casual")] Casual = 0,
    [MetaField(2, "competitive")] Competitive = 7
}
[MetaSchema("party.player", 1, "player"), MetaPrototype]
public sealed class Player {
    [MetaField(1, "id")] public string Id { get; set; } = "";
    [MetaField(2, "name"), DefaultValue("Guest")] public string Name { get; set; } = "Guest";
    [MetaField(3, "mode"), DefaultValue(0)] public Mode Mode { get; set; }
    [MetaField(4, "friend"), MetaReference("any"), DefaultValue(null)] public Player? Friend { get; set; }
    [MetaField(5, "scores")] public List<List<uint>> Scores { get; set; } = [];
}
[MetaSchema("party.session", 1, "session"), MetaPrototype]
public sealed class Session {
    [MetaField(1, "players"), MetaMerge("keyed", "id")] public List<Player> Players { get; set; } = [];
    [MetaField(2, "settings"), MetaMerge("map")] public Dictionary<string, List<int?>> Settings { get; set; } = [];
    [MetaField(3, "tags"), MetaMerge("set")] public HashSet<string> Tags { get; set; } = [];
    [MetaField(4, "host"), MetaReference("local")] public Player Host { get; set; } = new();
    [MetaField(5, "guest"), MetaReference("external")] public Player Guest { get; set; } = new();
}
