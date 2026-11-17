//! Wire message header.

/// `'EMBB'`, the first field of each message.
pub const MAGIC: u32 = 0x454D_4242;
/// Protocol version.
pub const VERSION: u16 = 1;

/// Message kind. The discriminants are wire values: add new ones at the end.
#[repr(u16)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MsgType {
    Request = 1,
    Response = 2,
    Event = 3,
    /// Payload: little-endian `u32` [`ErrorCode`](crate::ErrorCode).
    Error = 4,
}

/// Fixed header of each message. All fields are little-endian on the wire.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct MessageHeader {
    pub magic: u32,
    pub version: u16,
    /// Raw [`MsgType`] value. It stays raw because it comes from the wire.
    pub msg_type: u16,
    pub service_id: u32,
    pub method_id: u32,
    pub payload_size: u32,
    pub request_id: u32,
}

impl MessageHeader {
    /// Size of the header on the wire. Changing it requires a [`VERSION`] bump.
    pub const SIZE: usize = 24;

    /// Decodes a header.
    pub fn from_bytes(bytes: &[u8; Self::SIZE]) -> Self {
        let u32_at =
            |i: usize| u32::from_le_bytes([bytes[i], bytes[i + 1], bytes[i + 2], bytes[i + 3]]);
        let u16_at = |i: usize| u16::from_le_bytes([bytes[i], bytes[i + 1]]);
        Self {
            magic: u32_at(0),
            version: u16_at(4),
            msg_type: u16_at(6),
            service_id: u32_at(8),
            method_id: u32_at(12),
            payload_size: u32_at(16),
            request_id: u32_at(20),
        }
    }

    /// Encodes the header.
    pub fn to_bytes(&self) -> [u8; Self::SIZE] {
        let mut bytes = [0; Self::SIZE];
        bytes[0..4].copy_from_slice(&self.magic.to_le_bytes());
        bytes[4..6].copy_from_slice(&self.version.to_le_bytes());
        bytes[6..8].copy_from_slice(&self.msg_type.to_le_bytes());
        bytes[8..12].copy_from_slice(&self.service_id.to_le_bytes());
        bytes[12..16].copy_from_slice(&self.method_id.to_le_bytes());
        bytes[16..20].copy_from_slice(&self.payload_size.to_le_bytes());
        bytes[20..24].copy_from_slice(&self.request_id.to_le_bytes());
        bytes
    }
}
