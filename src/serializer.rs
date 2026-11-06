//! Little-endian payload encoding over caller-owned buffers.

use crate::ErrorCode;

/// Appends values to a byte buffer.
pub struct Writer<'a> {
    buf: &'a mut [u8],
    pos: usize,
}

impl<'a> Writer<'a> {
    /// Starts writing at the beginning of `buf`.
    pub fn new(buf: &'a mut [u8]) -> Self {
        Self { buf, pos: 0 }
    }

    /// Bytes written.
    pub fn len(&self) -> usize {
        self.pos
    }

    /// True if nothing was written.
    pub fn is_empty(&self) -> bool {
        self.pos == 0
    }

    pub fn write_i32(&mut self, value: i32) -> Result<(), ErrorCode> {
        self.write_bytes(&value.to_le_bytes())
    }

    pub fn write_u32(&mut self, value: u32) -> Result<(), ErrorCode> {
        self.write_bytes(&value.to_le_bytes())
    }

    pub fn write_i64(&mut self, value: i64) -> Result<(), ErrorCode> {
        self.write_bytes(&value.to_le_bytes())
    }

    pub fn write_u64(&mut self, value: u64) -> Result<(), ErrorCode> {
        self.write_bytes(&value.to_le_bytes())
    }

    pub fn write_f32(&mut self, value: f32) -> Result<(), ErrorCode> {
        self.write_bytes(&value.to_le_bytes())
    }

    pub fn write_f64(&mut self, value: f64) -> Result<(), ErrorCode> {
        self.write_bytes(&value.to_le_bytes())
    }

    /// Writes one byte: 1 for true, 0 for false.
    pub fn write_bool(&mut self, value: bool) -> Result<(), ErrorCode> {
        self.write_bytes(&[u8::from(value)])
    }

    /// Writes a `u32` length and the bytes, with no terminator and no padding.
    pub fn write_string(&mut self, value: &str) -> Result<(), ErrorCode> {
        let len = u32::try_from(value.len()).map_err(|_| ErrorCode::SerializationError)?;
        self.write_u32(len)?;
        self.write_bytes(value.as_bytes())
    }

    /// Writes raw bytes.
    pub fn write_bytes(&mut self, data: &[u8]) -> Result<(), ErrorCode> {
        if data.len() > self.buf.len() - self.pos {
            return Err(ErrorCode::BufferTooSmall);
        }
        self.buf[self.pos..self.pos + data.len()].copy_from_slice(data);
        self.pos += data.len();
        Ok(())
    }
}

/// Reads values from a byte slice.
pub struct Reader<'a> {
    data: &'a [u8],
}

impl<'a> Reader<'a> {
    /// Starts reading at the beginning of `data`.
    pub fn new(data: &'a [u8]) -> Self {
        Self { data }
    }

    pub fn read_i32(&mut self) -> Result<i32, ErrorCode> {
        self.take_array().map(i32::from_le_bytes)
    }

    pub fn read_u32(&mut self) -> Result<u32, ErrorCode> {
        self.take_array().map(u32::from_le_bytes)
    }

    pub fn read_i64(&mut self) -> Result<i64, ErrorCode> {
        self.take_array().map(i64::from_le_bytes)
    }

    pub fn read_u64(&mut self) -> Result<u64, ErrorCode> {
        self.take_array().map(u64::from_le_bytes)
    }

    pub fn read_f32(&mut self) -> Result<f32, ErrorCode> {
        self.take_array().map(f32::from_le_bytes)
    }

    pub fn read_f64(&mut self) -> Result<f64, ErrorCode> {
        self.take_array().map(f64::from_le_bytes)
    }

    /// Reads one byte. All nonzero values are true.
    pub fn read_bool(&mut self) -> Result<bool, ErrorCode> {
        let [byte] = self.take_array()?;
        Ok(byte != 0)
    }

    /// Reads a string with no copy. The length is consumed also on failure.
    pub fn read_string(&mut self) -> Result<&'a str, ErrorCode> {
        let len = usize::try_from(self.read_u32()?).map_err(|_| ErrorCode::BufferTooSmall)?;
        let bytes = self.take(len)?;
        core::str::from_utf8(bytes).map_err(|_| ErrorCode::DeserializationError)
    }

    fn take(&mut self, len: usize) -> Result<&'a [u8], ErrorCode> {
        let (head, tail) = self
            .data
            .split_at_checked(len)
            .ok_or(ErrorCode::BufferTooSmall)?;
        self.data = tail;
        Ok(head)
    }

    fn take_array<const N: usize>(&mut self) -> Result<[u8; N], ErrorCode> {
        let (head, tail) = self
            .data
            .split_first_chunk::<N>()
            .ok_or(ErrorCode::BufferTooSmall)?;
        self.data = tail;
        Ok(*head)
    }
}
