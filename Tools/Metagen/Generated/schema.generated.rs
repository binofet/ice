// Generated independent Rust consumer; do not edit.
// Schema SHA-256: 3cc146644d2f3075e27e82d4d69686dc40bad583080864f2c383f8280a08a776
use crate::wire::{Reader, put_text};
use std::collections::BTreeSet;
#[derive(Clone, Debug, PartialEq)]
pub struct PaddleSettings {
pub r#maximum_speed: f32,
pub r#predict_rebounds: bool,
pub r#winning_score: u32,
pub r#player_name: String,
pub r#boost: Option<f32>,
pub r#simulation_seed: i32,
}
impl Default for PaddleSettings {
fn default() -> Self { Self {
r#maximum_speed: 2.799999952316284f32,
r#predict_rebounds: true,
r#winning_score: 500u32,
r#player_name: "Player".to_owned(),
r#boost: None,
r#simulation_seed: -7i32,
} } }
impl PaddleSettings {
pub fn validate(&self) -> Result<(), &'static str> {
{ let value = &self.r#maximum_speed;
if !value.is_finite() || (*value as f64) < 0.0f64 || (*value as f64) > 20.0f64 { return Err("Invalid field 1"); }
}
{ let value = &self.r#winning_score;
if (*value as f64) < 1.0f64 || (*value as f64) > 10000.0f64 { return Err("Invalid field 3"); }
}
{ let value = &self.r#player_name;
if value.len() > 4096 || value.contains('\0') { return Err("Invalid field 4"); }
}
if let Some(value) = &self.r#boost {
if !value.is_finite() || (*value as f64) < 0.0f64 || (*value as f64) > 10.0f64 { return Err("Invalid field 5"); }
}
{ let value = &self.r#simulation_seed;
if (*value as f64) < -1000.0f64 || (*value as f64) > 1000.0f64 { return Err("Invalid field 6"); }
}
Ok(()) }
pub fn encode(&self) -> Result<Vec<u8>, &'static str> {
self.validate()?;
let mut output = b"ICM1".to_vec();
put_text(&mut output, "playground.paddle_settings");
output.extend_from_slice(&2u32.to_le_bytes());
output.extend_from_slice(&6u32.to_le_bytes());
output.extend_from_slice(&1u32.to_le_bytes());
output.push(3);
output.push(1);
{ let value = &self.r#maximum_speed;
output.extend_from_slice(&value.to_le_bytes());
}
output.extend_from_slice(&2u32.to_le_bytes());
output.push(0);
output.push(1);
{ let value = &self.r#predict_rebounds;
output.push(u8::from(*value));
}
output.extend_from_slice(&3u32.to_le_bytes());
output.push(2);
output.push(1);
{ let value = &self.r#winning_score;
output.extend_from_slice(&value.to_le_bytes());
}
output.extend_from_slice(&4u32.to_le_bytes());
output.push(5);
output.push(1);
{ let value = &self.r#player_name;
put_text(&mut output, value);
}
output.extend_from_slice(&5u32.to_le_bytes());
output.push(3);
if let Some(value) = &self.r#boost { output.push(1);
output.extend_from_slice(&value.to_le_bytes());
} else { output.push(0); }
output.extend_from_slice(&6u32.to_le_bytes());
output.push(1);
output.push(1);
{ let value = &self.r#simulation_seed;
output.extend_from_slice(&value.to_le_bytes());
}
Ok(output) }
pub fn decode(bytes: &[u8]) -> Result<Self, &'static str> {
if bytes.len() > 1024 * 1024 { return Err("Byte budget"); }
let mut source = Reader::new(bytes);
if source.take(4)? != b"ICM1" { return Err("Magic"); }
if source.text()? != "playground.paddle_settings" || source.u32()? != 2 { return Err("Schema"); }
let count = source.u32()?;
if count > 128 { return Err("Field budget"); }
let mut seen = BTreeSet::new();
let mut result = Self::default();
for _ in 0..count {
let id = source.u32()?;
let kind = source.u8()?;
let present = source.u8()?;
if present > 1 || !seen.insert(id) { return Err("Duplicate/presence"); }
match id {
1 => {
if kind != 3 { return Err("Field kind"); }
if present == 0 { return Err("Required value is null"); }
result.r#maximum_speed = source.f32()?;
}
2 => {
if kind != 0 { return Err("Field kind"); }
if present == 0 { return Err("Required value is null"); }
result.r#predict_rebounds = source.boolean()?;
}
3 => {
if kind != 2 { return Err("Field kind"); }
if present == 0 { return Err("Required value is null"); }
result.r#winning_score = source.u32()?;
}
4 => {
if kind != 5 { return Err("Field kind"); }
if present == 0 { return Err("Required value is null"); }
result.r#player_name = source.text()?;
}
5 => {
if kind != 3 { return Err("Field kind"); }
result.r#boost = if present == 1 { Some(source.f32()?) } else { None };
}
6 => {
if kind != 1 { return Err("Field kind"); }
if present == 0 { return Err("Required value is null"); }
result.r#simulation_seed = source.i32()?;
}
_ => return Err("Unknown field"),
} }
if !source.finished() { return Err("Trailing bytes"); }
result.validate()?;
Ok(result)
} }
