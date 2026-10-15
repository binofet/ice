// Generated rich type projection; serialization/graph activation is a separate adapter.
#[derive(Clone, Debug)]
pub struct ObjectRef<T> { pub document: String, pub identity: String, marker: std::marker::PhantomData<fn() -> T> }
impl<T> ObjectRef<T> { pub fn new(document: String, identity: String) -> Self { Self { document, identity, marker: std::marker::PhantomData } } }
#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd)]
#[repr(i32)]
pub enum Mode {
Casual = 0,
Competitive = 7,
}
impl Mode { pub const TYPE_ID: &'static str = "party.mode"; pub const VERSION: u32 = 1; }
#[derive(Clone, Debug)]
pub struct Player {
pub r#id: String,
pub r#name: String,
pub r#mode: Mode,
pub r#friend: Option<ObjectRef<Player>>,
pub r#scores: Vec<Vec<u32>>,
}
impl Player { pub const TYPE_ID: &'static str = "party.player"; pub const VERSION: u32 = 1; }
#[derive(Clone, Debug)]
pub struct Session {
pub r#players: Vec<Player>,
pub r#settings: std::collections::BTreeMap<String, Vec<Option<i32>>>,
pub r#tags: std::collections::BTreeSet<String>,
pub r#host: ObjectRef<Player>,
pub r#guest: ObjectRef<Player>,
}
impl Session { pub const TYPE_ID: &'static str = "party.session"; pub const VERSION: u32 = 1; }
