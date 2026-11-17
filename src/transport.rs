//! Transport abstraction. A port implements [`Transport`] for its platform.

/// Failure of a [`Transport`] operation.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TransportError {
    /// Nothing happened before the timeout.
    Timeout,
    /// A message does not fit in the buffer.
    NoMemory,
    InvalidArgument,
    Internal,
}

/// A link that delivers whole messages.
pub trait Transport {
    /// Sends `data` as one message.
    fn send(&mut self, data: &[u8], timeout_ms: u64) -> Result<(), TransportError>;

    /// Receives exactly one message to `buf` and returns its size.
    fn receive(&mut self, buf: &mut [u8], timeout_ms: u64) -> Result<usize, TransportError>;
}
