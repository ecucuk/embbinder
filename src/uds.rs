//! Transport over Unix domain sockets.
//!
//! [`UdsTransport::open`] connects as a client if a server listens at the
//! path. Otherwise it becomes a server that accepts one peer at a time. Each
//! [`Transport::receive`] returns exactly one framed message.

use crate::{MessageHeader, Transport, TransportError};
use std::fs;
use std::io::{self, ErrorKind, Read};
use std::os::fd::{AsRawFd, RawFd};
use std::os::unix::fs::FileTypeExt;
use std::os::unix::net::{UnixListener, UnixStream};
use std::time::Duration;

/// After the first byte of a frame, the rest must arrive within this time.
const FRAME_TIMEOUT_MS: u64 = 1000;

/// `std` sets `SO_NOSIGPIPE` on Apple sockets, which have no `MSG_NOSIGNAL`.
#[cfg(target_vendor = "apple")]
const SEND_FLAGS: libc::c_int = 0;
#[cfg(not(target_vendor = "apple"))]
const SEND_FLAGS: libc::c_int = libc::MSG_NOSIGNAL;

/// A server or client end of a Unix domain socket.
pub struct UdsTransport {
    /// `None` in client mode.
    listener: Option<UnixListener>,
    /// `None` while a server has no peer.
    conn: Option<UnixStream>,
}

impl UdsTransport {
    /// Connects to `path`, or starts a server there if nothing listens.
    ///
    /// This function replaces a stale socket file. It never replaces a
    /// regular file.
    pub fn open(path: &str) -> Result<Self, TransportError> {
        match UnixStream::connect(path) {
            Ok(conn) => {
                return Ok(Self {
                    listener: None,
                    conn: Some(conn),
                });
            }
            Err(error) if error.kind() == ErrorKind::InvalidInput => {
                return Err(TransportError::InvalidArgument);
            }
            Err(_) => {}
        }

        if fs::symlink_metadata(path).is_ok_and(|meta| meta.file_type().is_socket()) {
            let _ = fs::remove_file(path);
        }
        let listener = UnixListener::bind(path).map_err(|_| TransportError::Internal)?;
        Ok(Self {
            listener: Some(listener),
            conn: None,
        })
    }

    /// Closes the peer in server mode, which goes back to accepting.
    fn drop_connection(&mut self) {
        if self.listener.is_some() {
            self.conn = None;
        }
    }

    /// Accepts a peer if a server has none, and returns the peer.
    fn connection(&mut self, timeout_ms: u64) -> Result<&mut UnixStream, TransportError> {
        if self.conn.is_none() {
            let listener = self.listener.as_ref().ok_or(TransportError::Internal)?;
            wait_fd(listener.as_raw_fd(), libc::POLLIN, timeout_ms)?;
            let (conn, _) = listener.accept().map_err(|_| TransportError::Internal)?;
            self.conn = Some(conn);
        }
        self.conn.as_mut().ok_or(TransportError::Internal)
    }
}

impl Transport for UdsTransport {
    fn send(&mut self, data: &[u8], timeout_ms: u64) -> Result<(), TransportError> {
        let conn = self.conn.as_ref().ok_or(TransportError::Internal)?;
        let fd = conn.as_raw_fd();
        // A zero duration is an error, so the shortest wait is 1 ms.
        let timeout = Duration::from_millis(timeout_ms.max(1));
        if conn.set_write_timeout(Some(timeout)).is_err() {
            // macOS rejects this call when the peer has closed.
            self.drop_connection();
            return Err(TransportError::Internal);
        }

        let mut rest = data;
        while !rest.is_empty() {
            match send_once(fd, rest) {
                Ok(size) if size > 0 => rest = &rest[size..],
                Err(error) if error.kind() == ErrorKind::Interrupted => {}
                Err(error)
                    if matches!(error.kind(), ErrorKind::WouldBlock | ErrorKind::TimedOut) =>
                {
                    // The stream is in sync only if no byte of the frame left.
                    if rest.len() < data.len() {
                        self.drop_connection();
                    }
                    return Err(TransportError::Timeout);
                }
                _ => {
                    self.drop_connection();
                    return Err(TransportError::Internal);
                }
            }
        }
        Ok(())
    }

    fn receive(&mut self, buf: &mut [u8], timeout_ms: u64) -> Result<usize, TransportError> {
        let Some((header, rest)) = buf.split_first_chunk_mut::<{ MessageHeader::SIZE }>() else {
            return Err(TransportError::InvalidArgument);
        };

        let conn = self.connection(timeout_ms)?;
        // Wait for the first byte with the timeout of the caller.
        wait_fd(conn.as_raw_fd(), libc::POLLIN, timeout_ms)?;

        if read_exact(conn, header).is_err() {
            // The peer closed or stalled: a server goes back to accepting.
            self.drop_connection();
            return Err(if self.listener.is_some() {
                TransportError::Timeout
            } else {
                TransportError::Internal
            });
        }

        let header = MessageHeader::from_bytes(header);
        let Some(payload) = usize::try_from(header.payload_size)
            .ok()
            .and_then(|size| rest.get_mut(..size))
        else {
            // The payload cannot be skipped and stay in sync.
            self.drop_connection();
            return Err(TransportError::NoMemory);
        };

        if read_exact(conn, payload).is_err() {
            self.drop_connection();
            return Err(TransportError::Internal);
        }
        Ok(MessageHeader::SIZE + payload.len())
    }
}

/// Reads all of `buf`. Fails if the peer closes or stalls during the frame.
fn read_exact(conn: &mut UnixStream, buf: &mut [u8]) -> Result<(), TransportError> {
    let mut done = 0;
    while done < buf.len() {
        wait_fd(conn.as_raw_fd(), libc::POLLIN, FRAME_TIMEOUT_MS)?;
        match conn.read(&mut buf[done..]) {
            Ok(0) => return Err(TransportError::Internal),
            Ok(size) => done += size,
            Err(error) if error.kind() == ErrorKind::Interrupted => {}
            Err(_) => return Err(TransportError::Internal),
        }
    }
    Ok(())
}

/// Sends once. `std` writes can raise `SIGPIPE` on Linux.
#[allow(unsafe_code)]
fn send_once(fd: RawFd, data: &[u8]) -> io::Result<usize> {
    // SAFETY: `data` is valid for `data.len()` bytes during the call.
    let sent = unsafe { libc::send(fd, data.as_ptr().cast(), data.len(), SEND_FLAGS) };
    usize::try_from(sent).map_err(|_| io::Error::last_os_error())
}

/// Waits for `events` on `fd`. `UnixListener` has no accept timeout, so the
/// transport needs `poll(2)`.
#[allow(unsafe_code)]
fn wait_fd(fd: RawFd, events: i16, timeout_ms: u64) -> Result<(), TransportError> {
    let mut pollfd = libc::pollfd {
        fd,
        events,
        revents: 0,
    };
    let timeout = i32::try_from(timeout_ms).unwrap_or(i32::MAX);
    loop {
        // SAFETY: `pollfd` is valid during the call, and the count is 1.
        let ready = unsafe { libc::poll(&mut pollfd, 1, timeout) };
        match ready {
            0 => return Err(TransportError::Timeout),
            1.. => return Ok(()),
            _ if io::Error::last_os_error().kind() == ErrorKind::Interrupted => {}
            _ => return Err(TransportError::Internal),
        }
    }
}
