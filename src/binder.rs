//! Service registration and message dispatch.

use crate::{ErrorCode, MAGIC, MessageHeader, MsgType, VERSION};

/// Size of an Error reply: the header and a `u32` code.
const ERROR_REPLY_SIZE: usize = MessageHeader::SIZE + size_of::<u32>();

/// Application code that handles requests for one service ID.
///
/// `Send` lets an application share a [`Binder`] between threads in a mutex.
pub trait Service: Send {
    /// Handles one request.
    ///
    /// `payload` comes from the peer and is not trusted. Write the reply to
    /// `response` and return its size. `Err` sends an Error reply to the peer.
    fn handle(
        &mut self,
        method_id: u32,
        payload: &[u8],
        response: &mut [u8],
    ) -> Result<usize, ErrorCode>;
}

/// Failure of [`Binder::process_message`].
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ProcessError {
    pub code: ErrorCode,
    /// Size of an Error reply in the output buffer, or 0 if there is no reply.
    pub reply_size: usize,
}

/// Dispatches messages to at most `N` services.
pub struct Binder<'a, const N: usize> {
    slots: [Option<(u32, &'a mut dyn Service)>; N],
}

impl<const N: usize> Default for Binder<'_, N> {
    fn default() -> Self {
        Self::new()
    }
}

impl<'a, const N: usize> Binder<'a, N> {
    /// Creates a binder with no services.
    pub fn new() -> Self {
        Self {
            slots: core::array::from_fn(|_| None),
        }
    }

    /// Registers `service` as `id`.
    ///
    /// Gives `InvalidOperation` for a duplicate ID and `OutOfMemory` when all
    /// slots are in use.
    pub fn register_service(
        &mut self,
        id: u32,
        service: &'a mut dyn Service,
    ) -> Result<(), ErrorCode> {
        if self.slots.iter().flatten().any(|(used, _)| *used == id) {
            return Err(ErrorCode::InvalidOperation);
        }
        let slot = self
            .slots
            .iter_mut()
            .find(|slot| slot.is_none())
            .ok_or(ErrorCode::OutOfMemory)?;
        *slot = Some((id, service));
        Ok(())
    }

    /// Processes one raw message and returns the size of the reply in `output`.
    ///
    /// Malformed input gets no reply. A failure after the header is valid
    /// gives an Error reply if `output` has room, and
    /// [`ProcessError::reply_size`] tells its size.
    pub fn process_message(
        &mut self,
        input: &[u8],
        output: &mut [u8],
    ) -> Result<usize, ProcessError> {
        let rejected = |code| ProcessError {
            code,
            reply_size: 0,
        };

        let (header, rest) = input
            .split_first_chunk()
            .ok_or(rejected(ErrorCode::InvalidArgument))?;
        let header = MessageHeader::from_bytes(header);
        if header.magic != MAGIC {
            return Err(rejected(ErrorCode::InvalidArgument));
        }
        let payload = usize::try_from(header.payload_size)
            .ok()
            .and_then(|size| rest.get(..size))
            .ok_or(rejected(ErrorCode::BufferTooSmall))?;

        let Some(service) = self.find(header.service_id) else {
            return Err(send_error(&header, ErrorCode::ServiceNotFound, output));
        };

        let Some((reply_header, body)) = output.split_first_chunk_mut() else {
            return Err(rejected(ErrorCode::BufferTooSmall));
        };
        let capacity = body.len().min(u32::MAX as usize);

        match service.handle(header.method_id, payload, &mut body[..capacity]) {
            Ok(size) if size <= capacity => {
                *reply_header = reply_to(&header, MsgType::Response, size as u32).to_bytes();
                Ok(MessageHeader::SIZE + size)
            }
            Ok(_) => Err(send_error(&header, ErrorCode::InternalError, output)),
            Err(code) => Err(send_error(&header, code, output)),
        }
    }

    fn find(&mut self, service_id: u32) -> Option<&mut (dyn Service + 'a)> {
        self.slots
            .iter_mut()
            .flatten()
            .find(|(id, _)| *id == service_id)
            .map(|(_, service)| &mut **service)
    }
}

fn reply_to(request: &MessageHeader, msg_type: MsgType, payload_size: u32) -> MessageHeader {
    MessageHeader {
        magic: MAGIC,
        version: VERSION,
        msg_type: msg_type as u16,
        service_id: request.service_id,
        method_id: request.method_id,
        payload_size,
        request_id: request.request_id,
    }
}

/// Writes an Error reply. `reply_size` is 0 if `output` has no room for it.
fn send_error(request: &MessageHeader, code: ErrorCode, output: &mut [u8]) -> ProcessError {
    let Some(reply) = output.first_chunk_mut::<ERROR_REPLY_SIZE>() else {
        return ProcessError {
            code,
            reply_size: 0,
        };
    };
    let header = reply_to(request, MsgType::Error, size_of::<u32>() as u32);
    reply[..MessageHeader::SIZE].copy_from_slice(&header.to_bytes());
    reply[MessageHeader::SIZE..].copy_from_slice(&(code as u32).to_le_bytes());
    ProcessError {
        code,
        reply_size: ERROR_REPLY_SIZE,
    }
}
