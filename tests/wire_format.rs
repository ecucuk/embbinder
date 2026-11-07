//! Byte-exact checks against output of the C++ implementation.

use embbinder::{Binder, ErrorCode, MAGIC, MessageHeader, Reader, Service, Writer};

const REQUEST: [u8; 32] = [
    0x42, 0x42, 0x4D, 0x45, 0x01, 0x00, 0x01, 0x00, 0x64, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
];
const RESPONSE: [u8; 28] = [
    0x42, 0x42, 0x4D, 0x45, 0x01, 0x00, 0x02, 0x00, 0x64, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00,
];
const ERROR_REPLY: [u8; 28] = [
    0x42, 0x42, 0x4D, 0x45, 0x01, 0x00, 0x04, 0x00, 0xE7, 0x03, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
];

struct AddService;

impl Service for AddService {
    fn handle(
        &mut self,
        _method_id: u32,
        payload: &[u8],
        response: &mut [u8],
    ) -> Result<usize, ErrorCode> {
        let mut request = Reader::new(payload);
        let sum = request.read_i32()? + request.read_i32()?;
        let mut writer = Writer::new(response);
        writer.write_i32(sum)?;
        Ok(writer.len())
    }
}

#[test]
fn response_matches_cpp() {
    let mut service = AddService;
    let mut binder = Binder::<1>::new();
    binder.register_service(100, &mut service).unwrap();
    let mut out = [0u8; 64];
    let size = binder.process_message(&REQUEST, &mut out).unwrap();
    assert_eq!(out[..size], RESPONSE);
}

#[test]
fn error_reply_matches_cpp() {
    let mut header = MessageHeader::from_bytes(REQUEST.first_chunk().unwrap());
    header.service_id = 999;
    header.payload_size = 0;
    header.request_id = 42;
    let mut binder = Binder::<1>::new();
    let mut out = [0u8; 64];
    let err = binder
        .process_message(&header.to_bytes(), &mut out)
        .unwrap_err();
    assert_eq!(out[..err.reply_size], ERROR_REPLY);
}

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
