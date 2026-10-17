namespace Ice.NetworkMetadata;

// These are ICE consumer vocabulary, deliberately outside the generic Metagen SDK.
[AttributeUsage(AttributeTargets.Class | AttributeTargets.Struct)]
public sealed class ReplicatedAttribute(string writer) : Attribute {
    public string Writer { get; } = writer;
    public string Audience { get; set; } = "";
    public string Delivery { get; set; } = "";
    public string Conflict { get; set; } = "";
}
[AttributeUsage(AttributeTargets.Class | AttributeTargets.Struct)]
public sealed class NetworkEventAttribute(string direction) : Attribute {
    public string Direction { get; } = direction;
    public string RequiredCapability { get; set; } = "";
    public string Delivery { get; set; } = "";
    public string Deduplication { get; set; } = "";
}
[AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
public sealed class WireCodecAttribute(string codec) : Attribute {
    public string Codec { get; } = codec;
    public uint Version { get; set; } = 1;
    public string Unit { get; set; } = "";
    public string Interpolation { get; set; } = "";
}
[AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
public sealed class EntityReferenceAttribute(string validator) : Attribute {
    public string Validator { get; } = validator;
}
