//! Byte-exact checks against output of the C++ implementation.

use embbinder::{ErrorCode, MAGIC, MessageHeader};

const REQUEST: [u8; 32] = [
    0x42, 0x42, 0x4D, 0x45, 0x01, 0x00, 0x01, 0x00, 0x64, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
];

#[test]
fn header_round_trip() {
    let bytes: [u8; 24] = REQUEST[..24].try_into().unwrap();
    let header = MessageHeader::from_bytes(&bytes);
    assert_eq!(header.magic, MAGIC);
    assert_eq!(header.version, 1);
    assert_eq!(header.msg_type, 1);
    assert_eq!(header.service_id, 100);
    assert_eq!(header.method_id, 1);
    assert_eq!(header.payload_size, 8);
    assert_eq!(header.request_id, 1);
    assert_eq!(header.to_bytes(), bytes);
}

#[test]
fn error_code_wire_values() {
    assert_eq!(ErrorCode::Unknown as u32, 1);
    assert_eq!(ErrorCode::ServiceNotFound as u32, 5);
    assert_eq!(ErrorCode::InvalidOperation as u32, 10);
}
