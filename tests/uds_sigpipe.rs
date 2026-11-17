//! A closed peer must not end the process. This test is in its own binary
//! because it changes the signal state of the process.
#![cfg(all(feature = "std", unix))]

use embbinder::{MAGIC, MessageHeader, MsgType, Transport, TransportError, UdsTransport, VERSION};

#[test]
fn closed_peer_does_not_raise_sigpipe() {
    // SAFETY: this is the only test in the binary, so no other thread reads
    // the signal state.
    unsafe { libc::signal(libc::SIGPIPE, libc::SIG_DFL) };

    let path = format!("/tmp/embb_sigpipe_{}.sock", std::process::id());
    let mut server = UdsTransport::open(&path).unwrap();
    let mut client = UdsTransport::open(&path).unwrap();

    let frame = MessageHeader {
        magic: MAGIC,
        version: VERSION,
        msg_type: MsgType::Request as u16,
        service_id: 1,
        method_id: 1,
        payload_size: 0,
        request_id: 1,
    }
    .to_bytes();
    client.send(&frame, 1000).unwrap();
    let mut buf = [0u8; MessageHeader::SIZE];
    server.receive(&mut buf, 1000).unwrap();
    drop(client);

    assert_eq!(server.send(&frame, 1000), Err(TransportError::Internal));
    let _ = std::fs::remove_file(&path);
}
