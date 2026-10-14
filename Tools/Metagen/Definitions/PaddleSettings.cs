using System.ComponentModel;
namespace PlaygroundDefinitions;

[AttributeUsage(AttributeTargets.Class)]
public sealed class DescriptionOnlyAttribute : Attribute {
    // Reading the schema must not instantiate attributes.
    public DescriptionOnlyAttribute(string description) => throw new InvalidOperationException("Do not execute metadata constructors");
    public string? Category { get; set; }
}
[DescriptionOnly("Paddle controls", Category = "Gameplay")]
public sealed class PaddleSettings {
    [DefaultValue(2.8f)] public float MaximumSpeed { get; set; }
    [DefaultValue(true)] public bool PredictRebounds { get; set; }
}
