using Metagen;
using Metagen.PortableData;
using Ice.NetworkMetadata;
namespace Ice.NetworkFixtures;

[MetaSchema("demo.transform", 1, "transform")]
[Replicated("authority", Audience = "relevant", Delivery = "snapshot", Conflict = "whole_field_cas")]
public sealed class Transform {
    [MetaField(1, "x"), MetaRange(-300, 300)]
    [WireCodec("signed16_cm", Unit = "m", Interpolation = "linear")]
    public float X { get; set; }
    [MetaField(2, "y"), MetaRange(-300, 300)]
    [WireCodec("signed16_cm", Unit = "m", Interpolation = "linear")]
    public float Y { get; set; }
    [MetaField(3, "z"), MetaRange(-300, 300)]
    [WireCodec("signed16_cm", Unit = "m", Interpolation = "linear")]
    public float Z { get; set; }
}

[MetaSchema("demo.health", 1, "health")]
[Replicated("authority", Audience = "relevant", Delivery = "reliable_change", Conflict = "whole_field_cas")]
public sealed class Health {
    [MetaField(1, "points"), MetaRange(0, 65535), WireCodec("uint16_exact", Unit = "points", Interpolation = "none")]
    public uint Points { get; set; }
}

[MetaSchema("demo.fire_event", 1, "fire_event")]
[NetworkEvent("client_to_authority", RequiredCapability = "weapon.fire", Delivery = "reliable_event", Deduplication = "sequence_window_epoch")]
public sealed class FireEvent {
    // The scalar provider supplies string storage; this explicit attribute projects a typed
    // room/epoch/spawn reference. Arbitrary strings never become entity identities.
    [MetaField(1, "weapon"), WireCodec("entity_v1", Unit = "entity", Interpolation = "none"), EntityReference("owned_weapon")]
    public string Weapon { get; set; } = "";
    [MetaField(2, "input_sequence"), MetaRange(0, uint.MaxValue), WireCodec("uint32_exact", Unit = "sequence", Interpolation = "none")]
    public uint InputSequence { get; set; }
    [MetaField(3, "aim_x"), MetaRange(-1, 1), WireCodec("float32_finite", Unit = "unit_vector", Interpolation = "none")]
    public float AimX { get; set; }
    [MetaField(4, "aim_y"), MetaRange(-1, 1), WireCodec("float32_finite", Unit = "unit_vector", Interpolation = "none")]
    public float AimY { get; set; }
    [MetaField(5, "aim_z"), MetaRange(-1, 1), WireCodec("float32_finite", Unit = "unit_vector", Interpolation = "none")]
    public float AimZ { get; set; }
}
