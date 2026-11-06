//! RPC/binder library for embedded systems.
#![cfg_attr(not(feature = "std"), no_std)]
#![deny(unsafe_code)]

mod error;
mod message;

pub use error::ErrorCode;
pub use message::{MAGIC, MessageHeader, MsgType, VERSION};
