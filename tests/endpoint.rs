//! `Endpoint` poll logic over a scripted transport.

use embbinder::{
    Binder, Endpoint, ErrorCode, MAGIC, MessageHeader, MsgType, Service, Transport, TransportError,
    VERSION,
};
use std::cell::RefCell;
use std::collections::VecDeque;
use std::rc::Rc;

type Received = Result<Vec<u8>, TransportError>;

#[derive(Default)]
struct State {
    incoming: VecDeque<Received>,
    /// Size that `receive` reports instead of the real size.
    reported_size: Option<usize>,
    sent: Vec<Vec<u8>>,
    send_timeouts: Vec<u64>,
    send_error: Option<TransportError>,
}

#[derive(Clone, Default)]
struct ScriptedTransport(Rc<RefCell<State>>);

impl Transport for ScriptedTransport {
    fn send(&mut self, data: &[u8], timeout_ms: u64) -> Result<(), TransportError> {
        let mut state = self.0.borrow_mut();
        state.send_timeouts.push(timeout_ms);
        match state.send_error {
            Some(error) => Err(error),
            None => {
                state.sent.push(data.to_vec());
                Ok(())
            }
        }
    }

    fn receive(&mut self, buf: &mut [u8], _timeout_ms: u64) -> Result<usize, TransportError> {
        let mut state = self.0.borrow_mut();
        let frame = state.incoming.pop_front().expect("script ended")?;
        buf[..frame.len()].copy_from_slice(&frame);
        Ok(state.reported_size.unwrap_or(frame.len()))
    }
}

struct EmptyReply;

impl Service for EmptyReply {
    fn handle(&mut self, _: u32, _: &[u8], _: &mut [u8]) -> Result<usize, ErrorCode> {
        Ok(0)
    }
}

fn request(service_id: u32) -> Vec<u8> {
    MessageHeader {
        magic: MAGIC,
        version: VERSION,
        msg_type: MsgType::Request as u16,
        service_id,
        method_id: 1,
        payload_size: 0,
        request_id: 7,
    }
    .to_bytes()
    .to_vec()
}

/// Runs one poll with `incoming` as the next `receive` result.
fn poll_once(transport: &ScriptedTransport, incoming: Received) -> Result<bool, ErrorCode> {
    transport.0.borrow_mut().incoming.push_back(incoming);
    let mut service = EmptyReply;
    let mut binder = Binder::<1>::new();
    binder.register_service(1, &mut service).unwrap();
    Endpoint::<_, 64>::new(transport.clone()).poll(&mut binder, 0)
}

fn sent_type(transport: &ScriptedTransport) -> u16 {
    let state = transport.0.borrow();
    assert_eq!(state.sent.len(), 1);
    MessageHeader::from_bytes(state.sent[0].first_chunk().unwrap()).msg_type
}

#[test]
fn timeout_gives_false() {
    let transport = ScriptedTransport::default();
    assert_eq!(
        poll_once(&transport, Err(TransportError::Timeout)),
        Ok(false)
    );
}

#[test]
fn empty_receive_gives_false() {
    let transport = ScriptedTransport::default();
    assert_eq!(poll_once(&transport, Ok(Vec::new())), Ok(false));
}

#[test]
fn request_sends_reply() {
    let transport = ScriptedTransport::default();
    assert_eq!(poll_once(&transport, Ok(request(1))), Ok(true));
    assert_eq!(sent_type(&transport), MsgType::Response as u16);
}

#[test]
fn unknown_service_sends_reply_then_errors() {
    let transport = ScriptedTransport::default();
    assert_eq!(
        poll_once(&transport, Ok(request(999))),
        Err(ErrorCode::ServiceNotFound)
    );
    assert_eq!(sent_type(&transport), MsgType::Error as u16);
}

#[test]
fn malformed_frame_sends_nothing() {
    let transport = ScriptedTransport::default();
    let mut frame = request(1);
    frame[0] ^= 0xFF;
    assert_eq!(
        poll_once(&transport, Ok(frame)),
        Err(ErrorCode::InvalidArgument)
    );
    assert!(transport.0.borrow().sent.is_empty());
}

#[test]
fn receive_failure_gives_internal_error() {
    let transport = ScriptedTransport::default();
    assert_eq!(
        poll_once(&transport, Err(TransportError::NoMemory)),
        Err(ErrorCode::InternalError)
    );
}

#[test]
fn impossible_receive_size_gives_internal_error() {
    let transport = ScriptedTransport::default();
    transport.0.borrow_mut().reported_size = Some(65);
    assert_eq!(
        poll_once(&transport, Ok(request(1))),
        Err(ErrorCode::InternalError)
    );
}

#[test]
fn send_failure_gives_internal_error() {
    let transport = ScriptedTransport::default();
    transport.0.borrow_mut().send_error = Some(TransportError::Internal);
    assert_eq!(
        poll_once(&transport, Ok(request(1))),
        Err(ErrorCode::InternalError)
    );
}

#[test]
fn send_uses_one_second_timeout() {
    let transport = ScriptedTransport::default();
    poll_once(&transport, Ok(request(1))).unwrap();
    assert_eq!(transport.0.borrow().send_timeouts, [1000]);
}
