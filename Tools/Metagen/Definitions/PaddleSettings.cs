using Metagen.PortableData;
using System.ComponentModel;
using Metagen;
namespace PlaygroundDefinitions;

[AttributeUsage(AttributeTargets.Class)]
public sealed class DescriptionOnlyAttribute : Attribute {
    public DescriptionOnlyAttribute(string description) => throw new InvalidOperationException("Do not execute metadata constructors");
    public string? Category { get; set; }
}
[DescriptionOnly("Paddle controls", Category = "Gameplay")]
[MetaSchema("playground.paddle_settings", 2, "paddle_settings")]
[MetaTag("game.category", "gameplay")]
public sealed class PaddleSettings {
    [MetaField(1, "maximum_speed"), MetaRange(0, 20), DefaultValue(2.8f)]
    public float MaximumSpeed { get; set; }
    [MetaField(2, "predict_rebounds"), DefaultValue(true)]
    public bool PredictRebounds { get; set; }
    [MetaField(3, "winning_score"), MetaRange(1, 10000), DefaultValue(500u)]
    public uint WinningScore { get; set; }
    [MetaField(4, "player_name"), DefaultValue("Player"), MetaTag("editor.label", "Player name")]
    public string PlayerName { get; set; } = "Player";
    [MetaField(5, "boost"), MetaRange(0, 10), DefaultValue(null)]
    public float? Boost { get; set; }
    [MetaField(6, "simulation_seed"), MetaRange(-1000, 1000), DefaultValue(-7)]
    public int SimulationSeed { get; set; }
}
