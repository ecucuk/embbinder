//! RPC/binder library for embedded systems.
#![cfg_attr(not(feature = "std"), no_std)]
#![deny(unsafe_code)]

mod binder;
mod error;
mod message;
mod serializer;

pub use binder::{Binder, ProcessError, Service};
pub use error::ErrorCode;
pub use message::{MAGIC, MessageHeader, MsgType, VERSION};
pub use serializer::{Reader, Writer};
