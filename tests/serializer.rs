//! `Writer` and `Reader`: wire formats and bound checks.

use embbinder::{ErrorCode, Reader, Writer};

const FORMATS: [u8; 27] = [
    0x0A, 0x00, 0x00, 0x00, // i32 10
    0x02, 0x00, 0x00, 0x00, 0x68, 0x69, // "hi"
    0x01, // true
    0x00, 0x00, 0xC0, 0x3F, // f32 1.5
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x3F, // f64 1.5
    0xFF, 0xFF, 0xFF, 0xFF, // low half of i64 -1 follows below
];

fn write_all(writer: &mut Writer<'_>) -> Result<(), ErrorCode> {
    writer.write_i32(10)?;
    writer.write_string("hi")?;
    writer.write_bool(true)?;
    writer.write_f32(1.5)?;
    writer.write_f64(1.5)?;
    writer.write_i64(-1)
}

#[test]
fn writer_formats_match_wire() {
    let mut buf = [0u8; 31];
    let mut writer = Writer::new(&mut buf);
    write_all(&mut writer).unwrap();
    assert_eq!(writer.len(), 31);
    assert_eq!(buf[..27], FORMATS);
    assert_eq!(buf[27..], [0xFF; 4]);
}

#[test]
fn reader_reads_writer_output() {
    let mut buf = [0u8; 31];
    write_all(&mut Writer::new(&mut buf)).unwrap();
    let mut reader = Reader::new(&buf);
    assert_eq!(reader.read_i32(), Ok(10));
    assert_eq!(reader.read_string(), Ok("hi"));
    assert_eq!(reader.read_bool(), Ok(true));
    assert_eq!(reader.read_f32(), Ok(1.5));
    assert_eq!(reader.read_f64(), Ok(1.5));
    assert_eq!(reader.read_i64(), Ok(-1));
}

#[test]
fn unsigned_values_round_trip() {
    let mut buf = [0u8; 12];
    let mut writer = Writer::new(&mut buf);
    writer.write_u32(u32::MAX).unwrap();
    writer.write_u64(u64::MAX).unwrap();
    let mut reader = Reader::new(&buf);
    assert_eq!(reader.read_u32(), Ok(u32::MAX));
    assert_eq!(reader.read_u64(), Ok(u64::MAX));
}

#[test]
fn write_bytes_appends_raw_data() {
    let mut buf = [0u8; 2];
    let mut writer = Writer::new(&mut buf);
    assert!(writer.is_empty());
    writer.write_bytes(&[7, 8]).unwrap();
    assert_eq!(buf, [7, 8]);
}

#[test]
fn write_past_end_gives_buffer_too_small() {
    let mut small = [0u8; 3];
    assert_eq!(
        Writer::new(&mut small).write_u32(1),
        Err(ErrorCode::BufferTooSmall)
    );
    let mut five = [0u8; 5];
    assert_eq!(
        Writer::new(&mut five).write_string("hi"),
        Err(ErrorCode::BufferTooSmall)
    );
}

#[test]
fn oversized_string_length_is_rejected() {
    let mut reader = Reader::new(&[0xFF, 0xFF, 0xFF, 0xFF, 0x78]);
    assert_eq!(reader.read_string(), Err(ErrorCode::BufferTooSmall));
    assert!(reader.read_u32().is_err());
}

#[test]
fn read_past_end_gives_buffer_too_small() {
    let mut reader = Reader::new(&[1, 2, 3]);
    assert_eq!(reader.read_i32(), Err(ErrorCode::BufferTooSmall));
}

#[test]
fn invalid_utf8_gives_deserialization_error() {
    let mut reader = Reader::new(&[0x01, 0x00, 0x00, 0x00, 0xFF]);
    assert_eq!(reader.read_string(), Err(ErrorCode::DeserializationError));
}

#[test]
fn read_bool_treats_nonzero_as_true() {
    assert_eq!(Reader::new(&[2]).read_bool(), Ok(true));
}
