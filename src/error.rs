//! Error codes shared by the library and the wire format.

/// Failure reason. The discriminants are wire values: an Error reply carries
/// one as a little-endian `u32`.
#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ErrorCode {
    Unknown = 1,
    InvalidArgument = 2,
    OutOfMemory = 3,
    BufferTooSmall = 4,
    ServiceNotFound = 5,
    MethodNotFound = 6,
    SerializationError = 7,
    DeserializationError = 8,
    InternalError = 9,
    InvalidOperation = 10,
}
