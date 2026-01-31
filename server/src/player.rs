use std::net::SocketAddr;
use std::time::Instant;

#[derive(Debug, Clone)]
pub struct Player {
    pub id: u32,
    pub username: String,
    pub address: SocketAddr,
    pub lobby_name: String,
    pub last_seen: Instant,
    pub connected_at: Instant,
    pub last_puppet_state: Option<Vec<u8>>,
}

impl Player {
    pub fn new(id: u32, username: String, address: SocketAddr, lobby_name: String) -> Self {
        let now = Instant::now();

        Self {
            id,
            username,
            address,
            lobby_name,
            last_seen: now,
            connected_at: now,
            last_puppet_state: None,
        }
    }

    pub fn update_last_seen(&mut self) {
        self.last_seen = Instant::now();
    }

    pub fn is_timed_out(&self, timeout_secs: u64) -> bool {
        self.last_seen.elapsed().as_secs() >= timeout_secs
    }
}
