//! Poll loop that joins a [`Transport`] to a [`Binder`].

use crate::{Binder, ErrorCode, MessageHeader, ProcessError, Transport, TransportError};

const SEND_TIMEOUT_MS: u64 = 1000;

/// Serves messages from `T`. `M` is the maximum message size.
///
/// The struct stores two `[u8; M]` buffers. Put a large one in a `static`
/// on targets with a small stack.
pub struct Endpoint<T: Transport, const M: usize> {
    transport: T,
    rx: [u8; M],
    tx: [u8; M],
}

impl<T: Transport, const M: usize> Endpoint<T, M> {
    /// Creates an endpoint that uses `transport`.
    pub fn new(transport: T) -> Self {
        const { assert!(M >= MessageHeader::SIZE, "M is smaller than a header") };
        Self {
            transport,
            rx: [0; M],
            tx: [0; M],
        }
    }

    /// Serves at most one message.
    ///
    /// Gives `Ok(true)` if it processed a message and `Ok(false)` if no
    /// message arrived. An Error reply goes to the peer before this function
    /// returns its code.
    pub fn poll<const N: usize>(
        &mut self,
        binder: &mut Binder<'_, N>,
        timeout_ms: u64,
    ) -> Result<bool, ErrorCode> {
        let input = match self.transport.receive(&mut self.rx, timeout_ms) {
            Ok(0) | Err(TransportError::Timeout) => return Ok(false),
            Ok(size) => self.rx.get(..size).ok_or(ErrorCode::InternalError)?,
            Err(error) => {
                log::warn!("Endpoint: receive error: {error:?}");
                return Err(ErrorCode::InternalError);
            }
        };

        let (reply_size, result) = match binder.process_message(input, &mut self.tx) {
            Ok(size) => (size, Ok(true)),
            Err(ProcessError { code, reply_size }) => (reply_size, Err(code)),
        };

        // Error replies go out too, so that clients do not wait forever.
        if reply_size > 0 {
            let reply = &self.tx[..reply_size];
            if let Err(error) = self.transport.send(reply, SEND_TIMEOUT_MS) {
                log::error!("Endpoint: send error: {error:?}");
                return Err(ErrorCode::InternalError);
            }
        }
        result
    }
}
