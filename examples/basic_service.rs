//! A calculator service and one simulated client request.

use embbinder::{
    Binder, ErrorCode, MAGIC, MessageHeader, MsgType, Reader, Service, VERSION, Writer,
};

const CALCULATOR_ID: u32 = 100;

const ADD: u32 = 1;
const SUBTRACT: u32 = 2;
const MULTIPLY: u32 = 3;
const DIVIDE: u32 = 4;

struct CalculatorService;

impl Service for CalculatorService {
    fn handle(
        &mut self,
        method_id: u32,
        payload: &[u8],
        response: &mut [u8],
    ) -> Result<usize, ErrorCode> {
        let mut request = Reader::new(payload);
        let a = request.read_i32()?;
        let b = request.read_i32()?;

        // Wrapping and checked forms: client input must not cause a panic.
        let result = match method_id {
            ADD => a.wrapping_add(b),
            SUBTRACT => a.wrapping_sub(b),
            MULTIPLY => a.wrapping_mul(b),
            DIVIDE => a.checked_div(b).ok_or(ErrorCode::InvalidArgument)?,
            _ => return Err(ErrorCode::MethodNotFound),
        };
        println!("Service: method {method_id}: {a}, {b} = {result}");

        let mut writer = Writer::new(response);
        writer.write_i32(result)?;
        Ok(writer.len())
    }
}

fn main() {
    let mut calculator = CalculatorService;
    let mut binder = Binder::<4>::new();
    binder
        .register_service(CALCULATOR_ID, &mut calculator)
        .expect("a new binder has a free slot");

    println!("Client: Sending Request (10 + 20)");
    let mut message = [0u8; MessageHeader::SIZE + 8];
    let (header, payload) = message.split_at_mut(MessageHeader::SIZE);
    let mut writer = Writer::new(payload);
    writer.write_i32(10).expect("payload fits");
    writer.write_i32(20).expect("payload fits");
    let payload_size = writer.len() as u32;
    header.copy_from_slice(
        &MessageHeader {
            magic: MAGIC,
            version: VERSION,
            msg_type: MsgType::Request as u16,
            service_id: CALCULATOR_ID,
            method_id: ADD,
            payload_size,
            request_id: 1,
        }
        .to_bytes(),
    );

    let mut output = [0u8; 256];
    match binder.process_message(&message, &mut output) {
        Ok(size) => {
            println!("Client: Receive Success. Size: {size}");
            let body = &output[MessageHeader::SIZE..size];
            match Reader::new(body).read_i32() {
                Ok(sum) => println!("Client: Result = {sum}"),
                Err(error) => println!("Client: Bad response: {error:?}"),
            }
        }
        Err(error) => println!("Client: Error processing message: {error:?}"),
    }
}
