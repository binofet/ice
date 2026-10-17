// Independent ICM1 decoding primitives. Standard library only; no engine or FFI dependency.
pub struct Reader<'a> { bytes: &'a [u8], offset: usize }
impl<'a> Reader<'a> {
    pub fn new(bytes: &'a [u8]) -> Self { Self { bytes, offset: 0 } }
    pub fn take(&mut self, count: usize) -> Result<&'a [u8], &'static str> {
        let end = self.offset.checked_add(count).ok_or("Length overflow")?;
        let data = self.bytes.get(self.offset..end).ok_or("Truncated input")?;
        self.offset = end;
        Ok(data)
    }
    pub fn u8(&mut self) -> Result<u8, &'static str> { Ok(self.take(1)?[0]) }
    pub fn u32(&mut self) -> Result<u32, &'static str> { Ok(u32::from_le_bytes(self.take(4)?.try_into().map_err(|_| "u32")?)) }
    pub fn i32(&mut self) -> Result<i32, &'static str> { Ok(i32::from_le_bytes(self.take(4)?.try_into().map_err(|_| "i32")?)) }
    pub fn f32(&mut self) -> Result<f32, &'static str> { Ok(f32::from_le_bytes(self.take(4)?.try_into().map_err(|_| "f32")?)) }
    // Kept for any schema that uses float64, not just this particular fixture.
    #[allow(dead_code)]
    pub fn f64(&mut self) -> Result<f64, &'static str> { Ok(f64::from_le_bytes(self.take(8)?.try_into().map_err(|_| "f64")?)) }
    pub fn boolean(&mut self) -> Result<bool, &'static str> {
        match self.u8()? { 0 => Ok(false), 1 => Ok(true), _ => Err("Boolean") }
    }
    pub fn text(&mut self) -> Result<String, &'static str> {
        let size = self.u32()? as usize;
        if size > 4096 { return Err("String budget"); }
        let text = std::str::from_utf8(self.take(size)?).map_err(|_| "UTF-8")?;
        if text.contains('\0') { return Err("NUL"); }
        Ok(text.to_owned())
    }
    pub fn finished(&self) -> bool { self.offset == self.bytes.len() }
}
pub fn put_text(output: &mut Vec<u8>, value: &str) {
    output.extend_from_slice(&(value.len() as u32).to_le_bytes());
    output.extend_from_slice(value.as_bytes());
}
