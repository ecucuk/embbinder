//! RPC/binder library for embedded systems.
#![cfg_attr(not(feature = "std"), no_std)]
#![deny(unsafe_code)]

mod binder;
mod endpoint;
mod error;
mod message;
mod serializer;
mod transport;
#[cfg(all(feature = "std", unix))]
mod uds;

pub use binder::{Binder, ProcessError, Service};
pub use endpoint::Endpoint;
pub use error::ErrorCode;
pub use message::{MAGIC, MessageHeader, MsgType, VERSION};
pub use serializer::{Reader, Writer};
pub use transport::{Transport, TransportError};
#[cfg(all(feature = "std", unix))]
pub use uds::UdsTransport;
