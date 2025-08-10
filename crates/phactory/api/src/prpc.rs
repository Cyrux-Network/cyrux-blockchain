pub use crate::proto_generated::*;
use alloc::vec::Vec;
use cyrux_types::messaging::{MessageOrigin, SignedMessage};
pub use prpc::{client, server, Message};
pub type EgressMessages = Vec<(MessageOrigin, Vec<SignedMessage>)>;
