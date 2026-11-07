//! `Endpoint` over the UDS transport: accept, framing and failed peers.
#![cfg(all(feature = "std", unix))]

use embbinder::{
    Binder, Endpoint, ErrorCode, MAGIC, MessageHeader, MsgType, Reader, Service, Transport,
    TransportError, UdsTransport, VERSION, Writer,
};
use std::fs;
use std::os::unix::net::UnixListener;
use std::sync::mpsc;
use std::thread;
use std::time::{Duration, Instant};

const MAX_MESSAGE: usize = 256;

/// Socket path that is removed at the end of a test. It is in `/tmp` because
/// the repository path can be too long for `sun_path`.
struct TestPath(String);

impl TestPath {
    fn new(test: &str) -> Self {
        Self(format!("/tmp/embb_{test}_{}.sock", std::process::id()))
    }
}

impl Drop for TestPath {
    fn drop(&mut self) {
        let _ = fs::remove_file(&self.0);
    }
}

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

/// A frame with an 8-byte payload. `payload_size` can claim another size.
fn request(service_id: u32, payload_size: u32) -> Vec<u8> {
    let header = MessageHeader {
        magic: MAGIC,
        version: VERSION,
        msg_type: MsgType::Request as u16,
        service_id,
        method_id: 1,
        payload_size,
        request_id: 7,
    };
    let mut payload = [0u8; 8];
    let mut writer = Writer::new(&mut payload);
    writer.write_i32(10).unwrap();
    writer.write_i32(20).unwrap();
    [header.to_bytes().as_slice(), payload.as_slice()].concat()
}

fn server(path: &TestPath) -> Endpoint<UdsTransport, MAX_MESSAGE> {
    Endpoint::new(UdsTransport::open(&path.0).unwrap())
}

fn client(path: &TestPath) -> UdsTransport {
    UdsTransport::open(&path.0).unwrap()
}

fn receive_reply(client: &mut UdsTransport) -> (MessageHeader, Vec<u8>) {
    let mut buf = [0u8; MAX_MESSAGE];
    let size = client.receive(&mut buf, 1000).unwrap();
    let header = MessageHeader::from_bytes(buf.first_chunk().unwrap());
    assert_eq!(size, MessageHeader::SIZE + header.payload_size as usize);
    (header, buf[MessageHeader::SIZE..size].to_vec())
}

/// Checks that a new client gets a correct reply.
fn assert_serves_new_client(
    path: &TestPath,
    endpoint: &mut Endpoint<UdsTransport, MAX_MESSAGE>,
    binder: &mut Binder<'_, 1>,
) {
    let mut peer = client(path);
    peer.send(&request(1, 8), 1000).unwrap();
    assert_eq!(endpoint.poll(binder, 1000), Ok(true));
    let (reply, payload) = receive_reply(&mut peer);
    assert_eq!(reply.msg_type, MsgType::Response as u16);
    assert_eq!(Reader::new(&payload).read_i32(), Ok(30));
}

#[test]
fn poll_without_client_times_out() {
    let path = TestPath::new("no_client");
    let mut binder = Binder::<1>::new();
    assert_eq!(server(&path).poll(&mut binder, 0), Ok(false));
}

#[test]
fn split_request_is_reassembled() {
    let path = TestPath::new("split");
    let mut service = AddService;
    let mut binder = Binder::<1>::new();
    binder.register_service(1, &mut service).unwrap();
    let mut endpoint = server(&path);
    let mut peer = client(&path);

    let frame = request(1, 8);
    peer.send(&frame[..10], 1000).unwrap();
    peer.send(&frame[10..], 1000).unwrap();
    assert_eq!(endpoint.poll(&mut binder, 1000), Ok(true));

    let (reply, payload) = receive_reply(&mut peer);
    assert_eq!(reply.msg_type, MsgType::Response as u16);
    assert_eq!(Reader::new(&payload).read_i32(), Ok(30));
}

#[test]
fn unknown_service_gets_error_reply() {
    let path = TestPath::new("unknown");
    let mut binder = Binder::<1>::new();
    let mut endpoint = server(&path);
    let mut peer = client(&path);

    peer.send(&request(999, 8), 1000).unwrap();
    assert_eq!(
        endpoint.poll(&mut binder, 1000),
        Err(ErrorCode::ServiceNotFound)
    );
    assert_eq!(receive_reply(&mut peer).0.msg_type, MsgType::Error as u16);
}

#[test]
fn oversized_frame_drops_peer_and_keeps_serving() {
    let path = TestPath::new("oversized");
    let mut service = AddService;
    let mut binder = Binder::<1>::new();
    binder.register_service(1, &mut service).unwrap();
    let mut endpoint = server(&path);

    let mut peer = client(&path);
    peer.send(&request(1, 1000), 1000).unwrap();
    assert_eq!(
        endpoint.poll(&mut binder, 1000),
        Err(ErrorCode::InternalError)
    );
    drop(peer);

    assert_serves_new_client(&path, &mut endpoint, &mut binder);
}

#[test]
fn client_close_without_data_gives_false() {
    let path = TestPath::new("close_empty");
    let mut service = AddService;
    let mut binder = Binder::<1>::new();
    binder.register_service(1, &mut service).unwrap();
    let mut endpoint = server(&path);

    drop(client(&path));
    assert_eq!(endpoint.poll(&mut binder, 1000), Ok(false));

    assert_serves_new_client(&path, &mut endpoint, &mut binder);
}

#[test]
fn close_mid_header_gives_false() {
    let path = TestPath::new("close_header");
    let mut service = AddService;
    let mut binder = Binder::<1>::new();
    binder.register_service(1, &mut service).unwrap();
    let mut endpoint = server(&path);

    let mut peer = client(&path);
    peer.send(&request(1, 8)[..10], 1000).unwrap();
    drop(peer);
    assert_eq!(endpoint.poll(&mut binder, 1000), Ok(false));

    assert_serves_new_client(&path, &mut endpoint, &mut binder);
}

#[test]
fn stalled_payload_gives_error_and_keeps_serving() {
    let path = TestPath::new("stall");
    let mut service = AddService;
    let mut binder = Binder::<1>::new();
    binder.register_service(1, &mut service).unwrap();
    let mut endpoint = server(&path);

    let mut peer = client(&path);
    peer.send(&request(1, 8)[..26], 1000).unwrap();
    let start = Instant::now();
    assert_eq!(
        endpoint.poll(&mut binder, 1000),
        Err(ErrorCode::InternalError)
    );
    assert!(start.elapsed() >= Duration::from_millis(900));

    assert_serves_new_client(&path, &mut endpoint, &mut binder);
    drop(peer);
}

#[test]
fn client_gone_before_reply_gives_error() {
    let path = TestPath::new("gone");
    let mut service = AddService;
    let mut binder = Binder::<1>::new();
    binder.register_service(1, &mut service).unwrap();
    let mut endpoint = server(&path);

    let mut peer = client(&path);
    peer.send(&request(1, 8), 1000).unwrap();
    drop(peer);
    assert_eq!(
        endpoint.poll(&mut binder, 1000),
        Err(ErrorCode::InternalError)
    );

    assert_serves_new_client(&path, &mut endpoint, &mut binder);
}

#[test]
fn send_to_client_that_does_not_read_times_out() {
    let path = TestPath::new("no_read");
    let mut server = UdsTransport::open(&path.0).unwrap();
    let mut peer = client(&path);

    // Accept the peer and leave it connected with nothing read.
    peer.send(&request(1, 8), 1000).unwrap();
    let mut buf = [0u8; MAX_MESSAGE];
    server.receive(&mut buf, 1000).unwrap();

    // The thread lets the test fail, not hang, if send blocks.
    let (done, result) = mpsc::channel();
    thread::spawn(move || {
        let outcome = server.send(&vec![0u8; 1 << 24], 200);
        let _ = done.send((outcome, server));
    });
    let (outcome, mut server) = result
        .recv_timeout(Duration::from_secs(5))
        .expect("send did not obey its timeout");
    assert_eq!(outcome, Err(TransportError::Timeout));

    // A partial frame cannot continue, so the server dropped the peer.
    assert_eq!(server.send(&[0], 200), Err(TransportError::Internal));
    drop(peer);
}

#[test]
fn regular_file_is_never_removed() {
    let path = TestPath::new("plain");
    fs::write(&path.0, "keep").unwrap();
    assert!(UdsTransport::open(&path.0).is_err());
    assert_eq!(fs::read_to_string(&path.0).unwrap(), "keep");
}

#[test]
fn stale_socket_file_is_replaced() {
    let path = TestPath::new("stale");
    drop(UnixListener::bind(&path.0).unwrap());

    let mut endpoint = server(&path);
    let mut service = AddService;
    let mut binder = Binder::<1>::new();
    binder.register_service(1, &mut service).unwrap();
    assert_serves_new_client(&path, &mut endpoint, &mut binder);
}

#[test]
fn long_path_is_invalid_argument() {
    assert_eq!(
        UdsTransport::open(&"x".repeat(200)).err(),
        Some(TransportError::InvalidArgument)
    );
}

#[test]
fn nul_in_path_is_invalid_argument() {
    assert_eq!(
        UdsTransport::open("a\0b").err(),
        Some(TransportError::InvalidArgument)
    );
}
