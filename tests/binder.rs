//! `Binder` message handling.

use embbinder::{
    Binder, ErrorCode, MAGIC, MessageHeader, MsgType, ProcessError, Reader, Service, VERSION,
};

/// Writes `claim` bytes of 0xAB. With `lie`, it only reports them.
#[derive(Default)]
struct FillService {
    seen_capacity: usize,
    claim: usize,
    lie: bool,
    fail: Option<ErrorCode>,
}

impl Service for FillService {
    fn handle(
        &mut self,
        _method_id: u32,
        _payload: &[u8],
        response: &mut [u8],
    ) -> Result<usize, ErrorCode> {
        self.seen_capacity = response.len();
        if let Some(code) = self.fail {
            return Err(code);
        }
        if !self.lie {
            response[..self.claim].fill(0xAB);
        }
        Ok(self.claim)
    }
}

fn request(service_id: u32, payload_size: u32) -> [u8; MessageHeader::SIZE] {
    MessageHeader {
        magic: MAGIC,
        version: VERSION,
        msg_type: MsgType::Request as u16,
        service_id,
        method_id: 1,
        payload_size,
        request_id: 42,
    }
    .to_bytes()
}

fn header_of(frame: &[u8]) -> MessageHeader {
    MessageHeader::from_bytes(frame.first_chunk().unwrap())
}

/// Runs one request against `service` on ID 10.
fn process(
    service: &mut FillService,
    input: &[u8],
    output: &mut [u8],
) -> Result<usize, ProcessError> {
    let mut binder = Binder::<1>::new();
    binder.register_service(10, service).unwrap();
    binder.process_message(input, output)
}

#[test]
fn normal_request_gives_response() {
    let mut service = FillService {
        claim: 4,
        ..Default::default()
    };
    let mut out = [0u8; 32];
    assert_eq!(process(&mut service, &request(10, 0), &mut out), Ok(28));
    assert_eq!(service.seen_capacity, 8);
    let reply = header_of(&out);
    assert_eq!(reply.msg_type, MsgType::Response as u16);
    assert_eq!(reply.request_id, 42);
    assert_eq!(out[24..28], [0xAB; 4]);
}

#[test]
fn oversized_claim_gives_internal_error() {
    let mut service = FillService {
        claim: 9,
        lie: true,
        ..Default::default()
    };
    let mut out = [0u8; 32];
    assert_eq!(
        process(&mut service, &request(10, 0), &mut out),
        Err(ProcessError {
            code: ErrorCode::InternalError,
            reply_size: 28
        })
    );
    assert_eq!(header_of(&out).msg_type, MsgType::Error as u16);
}

#[test]
fn usize_max_claim_gives_internal_error() {
    let mut service = FillService {
        claim: usize::MAX,
        lie: true,
        ..Default::default()
    };
    let mut out = [0u8; 32];
    let err = process(&mut service, &request(10, 0), &mut out).unwrap_err();
    assert_eq!(err.code, ErrorCode::InternalError);
}

#[test]
fn service_error_gives_error_reply() {
    let mut service = FillService {
        fail: Some(ErrorCode::MethodNotFound),
        ..Default::default()
    };
    let mut out = [0u8; 32];
    assert_eq!(
        process(&mut service, &request(10, 0), &mut out),
        Err(ProcessError {
            code: ErrorCode::MethodNotFound,
            reply_size: 28
        })
    );
    assert_eq!(out[24..28], [6, 0, 0, 0]);
}

#[test]
fn unknown_service_gives_error_reply() {
    let mut service = FillService::default();
    let mut out = [0u8; 32];
    let err = process(&mut service, &request(999, 0), &mut out).unwrap_err();
    assert_eq!(err.code, ErrorCode::ServiceNotFound);
    assert_eq!(err.reply_size, 28);
    let reply = header_of(&out);
    assert_eq!(reply.msg_type, MsgType::Error as u16);
    assert_eq!(reply.request_id, 42);
    assert_eq!(reply.service_id, 999);
    assert_eq!(reply.payload_size, 4);
    assert_eq!(
        Reader::new(&out[24..28]).read_u32(),
        Ok(ErrorCode::ServiceNotFound as u32)
    );
}

#[test]
fn huge_payload_size_is_rejected() {
    let mut service = FillService::default();
    let mut out = [0u8; 32];
    assert_eq!(
        process(&mut service, &request(10, 0xFFFF_FFF0), &mut out),
        Err(ProcessError {
            code: ErrorCode::BufferTooSmall,
            reply_size: 0
        })
    );
}

#[test]
fn short_input_is_rejected() {
    let mut service = FillService::default();
    let mut out = [0u8; 32];
    assert_eq!(
        process(&mut service, &request(10, 0)[..23], &mut out),
        Err(ProcessError {
            code: ErrorCode::InvalidArgument,
            reply_size: 0
        })
    );
}

#[test]
fn bad_magic_is_rejected() {
    let mut service = FillService::default();
    let mut input = request(10, 0);
    input[0] ^= 0xFF;
    let mut out = [0u8; 32];
    assert_eq!(
        process(&mut service, &input, &mut out),
        Err(ProcessError {
            code: ErrorCode::InvalidArgument,
            reply_size: 0
        })
    );
}

#[test]
fn small_output_is_rejected() {
    let mut service = FillService::default();
    let mut out = [0u8; 23];
    assert_eq!(
        process(&mut service, &request(10, 0), &mut out),
        Err(ProcessError {
            code: ErrorCode::BufferTooSmall,
            reply_size: 0
        })
    );
}

#[test]
fn error_reply_needs_28_bytes() {
    let mut service = FillService::default();
    let mut out = [0u8; 27];
    assert_eq!(
        process(&mut service, &request(999, 0), &mut out),
        Err(ProcessError {
            code: ErrorCode::ServiceNotFound,
            reply_size: 0
        })
    );
}

#[test]
fn every_request_prefix_is_rejected() {
    let mut frame = [0u8; 32];
    frame[..24].copy_from_slice(&request(10, 8));
    for len in 0..frame.len() {
        let mut service = FillService::default();
        let mut out = [0u8; 64];
        assert!(process(&mut service, &frame[..len], &mut out).is_err());
    }
}

#[test]
fn register_rejects_duplicate_and_full() {
    let (mut a, mut b, mut c) = (
        FillService::default(),
        FillService::default(),
        FillService::default(),
    );
    let mut binder = Binder::<1>::new();
    assert_eq!(binder.register_service(1, &mut a), Ok(()));
    assert_eq!(
        binder.register_service(1, &mut b),
        Err(ErrorCode::InvalidOperation)
    );
    assert_eq!(
        binder.register_service(2, &mut c),
        Err(ErrorCode::OutOfMemory)
    );
}

#[test]
fn zero_slot_binder_finds_no_service() {
    let mut service = FillService::default();
    let mut binder = Binder::<0>::new();
    assert_eq!(
        binder.register_service(1, &mut service),
        Err(ErrorCode::OutOfMemory)
    );
    let mut out = [0u8; 32];
    let err = binder
        .process_message(&request(1, 0), &mut out)
        .unwrap_err();
    assert_eq!(err.code, ErrorCode::ServiceNotFound);
}

#[test]
fn binder_is_send() {
    fn assert_send<T: Send>() {}
    assert_send::<Binder<'static, 2>>();
}
