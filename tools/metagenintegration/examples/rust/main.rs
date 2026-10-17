mod wire;
#[path = "../../generated/schema.generated.rs"]
mod generated;
#[path = "../../generated/data.generated.rs"]
pub mod data_generated;
use generated::PaddleSettings;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    use data_generated::{Mode, ObjectRef, Player, Session};
    let player = Player { id: "alice".into(), name: "Alice".into(), mode: Mode::Competitive,
        friend: Some(ObjectRef::new("party://session".into(), "host".into())), scores: vec![vec![10, 20], vec![]] };
    let session = Session { players: vec![player], settings: std::collections::BTreeMap::from([("limits".into(), vec![None, Some(-2)])]),
        tags: std::collections::BTreeSet::from(["pub".into()]),
        host: ObjectRef::new("party://session".into(), "host".into()),
        guest: ObjectRef::new("party://guests".into(), "guest".into()) };
    assert_eq!(session.players[0].mode as i32, 7);
    assert_eq!(session.players[0].scores[0], vec![10, 20]);
    assert_eq!(session.settings["limits"], vec![None, Some(-2)]);
    let args: Vec<_> = std::env::args_os().collect();
    if args.len() != 5 { return Err("Expected two C++ inputs and two Rust outputs".into()); }
    for (input, output) in [(&args[1], &args[3]), (&args[2], &args[4])] {
        let bytes = std::fs::read(input)?;
        let decoded = PaddleSettings::decode(&bytes)?;
        assert_eq!(decoded.encode()?, bytes);
        for size in 0..bytes.len() { assert!(PaddleSettings::decode(&bytes[..size]).is_err()); }
        let mut trailing = bytes.clone();
        trailing.push(0);
        assert!(PaddleSettings::decode(&trailing).is_err());
        std::fs::write(output, decoded.encode()?)?;
    }
    let defaults = PaddleSettings::decode(&std::fs::read(&args[1])?)?;
    assert_eq!(defaults, PaddleSettings::default());
    let changed = PaddleSettings::decode(&std::fs::read(&args[2])?)?;
    assert_eq!(changed.maximum_speed, 6.25);
    assert_eq!(changed.simulation_seed, -54);
    assert_eq!(changed.boost, Some(5.0));
    assert_eq!(changed.player_name, "Snowman ☃");
    let mut invalid = defaults.clone();
    invalid.maximum_speed = -1.0;
    assert!(invalid.encode().is_err());
    invalid.maximum_speed = f32::NAN;
    assert!(invalid.encode().is_err());
    println!("Rust: C++ bytes decoded, validated, regenerated identically; truncated/invalid input rejected");
    Ok(())
}
