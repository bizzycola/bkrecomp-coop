use anyhow::Result;
use std::collections::HashMap;
use std::net::SocketAddr;
use std::sync::Arc;
use std::time::Duration;
use tokio::net::UdpSocket;
use tokio::time;
use tracing::{debug, error, info, warn};

use crate::config::Config;
use crate::packets::*;
use crate::protocol::PacketType;
use crate::state::ServerState;

pub struct NetworkServer {
    socket: Arc<UdpSocket>,
    state: Arc<ServerState>,

    last_reliable_seq: tokio::sync::Mutex<HashMap<(SocketAddr, u8), u32>>,

    reliable_next_seq: std::sync::atomic::AtomicU32,
    reliable_pending: tokio::sync::Mutex<HashMap<(SocketAddr, u32), PendingReliable>>,
}

#[derive(Clone)]
struct PendingReliable {
    packet_type: PacketType,
    payload: Vec<u8>,
    last_send_ms: u64,
    attempts: u8,
}

impl NetworkServer {
    pub fn new(socket: Arc<UdpSocket>, config: Config) -> Self {
        Self {
            socket,
            state: Arc::new(ServerState::new(config)),
            last_reliable_seq: tokio::sync::Mutex::new(HashMap::new()),
            reliable_next_seq: std::sync::atomic::AtomicU32::new(1),
            reliable_pending: tokio::sync::Mutex::new(HashMap::new()),
        }
    }

    fn is_reliable_type(packet_type: PacketType) -> bool {
        matches!(
            packet_type,
            PacketType::JiggyCollected
                | PacketType::NoteCollected
                | PacketType::NoteCollectedPos
                | PacketType::NoteSaveData
                | PacketType::FileProgressFlags
                | PacketType::AbilityProgress
                | PacketType::HoneycombScore
                | PacketType::MumboScore
                | PacketType::HoneycombCollected
                | PacketType::MumboTokenCollected
                | PacketType::LevelOpened
                | PacketType::FullSyncRequest
        )
    }

    async fn maybe_strip_reliable_prefix(
        &self,
        packet_type: PacketType,
        payload: &[u8],
        addr: SocketAddr,
    ) -> Result<Option<Vec<u8>>> {
        if !Self::is_reliable_type(packet_type) {
            return Ok(Some(payload.to_vec()));
        }

        if payload.len() < 4 {
            return Ok(None);
        }

        let seq = u32::from_le_bytes([payload[0], payload[1], payload[2], payload[3]]);

        self.send_packet(PacketType::ReliableAck, &seq.to_le_bytes(), addr)
            .await?;

        {
            let mut map = self.last_reliable_seq.lock().await;
            let key = (addr, packet_type as u8);
            let last = map.get(&key).copied().unwrap_or(0);
            if seq <= last {
                return Ok(None);
            }
            map.insert(key, seq);
        }

        Ok(Some(payload[4..].to_vec()))
    }

    pub async fn run(self) -> Result<()> {
        let server = Arc::new(self);

        let cleanup_server = server.clone();
        tokio::spawn(async move {
            cleanup_server.cleanup_loop().await;
        });

        let resend_server = server.clone();
        tokio::spawn(async move {
            resend_server.reliable_resend_loop().await;
        });

        server.receive_loop().await
    }

    fn now_ms() -> u64 {
        use std::time::{SystemTime, UNIX_EPOCH};
        SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap_or_default()
            .as_millis() as u64
    }

    async fn reliable_resend_loop(&self) {
        const SWEEP_MS: u64 = 250;
        const RETRY_TIMEOUT_MS: u64 = 600;
        const MAX_ATTEMPTS: u8 = 10;
        const MAX_PENDING: usize = 2048;

        let mut interval = time::interval(Duration::from_millis(SWEEP_MS));

        loop {
            tokio::select! {
                _ = interval.tick() => {},
                _ = tokio::signal::ctrl_c() => {
                    debug!("Reliable resend loop shutting down (ctrl-c)");
                    return;
                }
            }

            let now = Self::now_ms();

            let mut resend: Vec<(SocketAddr, u32, PacketType, Vec<u8>, u8)> = Vec::new();
            let mut to_remove: Vec<(SocketAddr, u32)> = Vec::new();

            {
                let mut pending = self.reliable_pending.lock().await;

                if pending.len() > MAX_PENDING {
                    pending.clear();
                }

                for (key, entry) in pending.iter_mut() {
                    if entry.attempts >= MAX_ATTEMPTS {
                        to_remove.push(*key);
                        continue;
                    }

                    if entry.last_send_ms == 0 || now.saturating_sub(entry.last_send_ms) >= RETRY_TIMEOUT_MS {
                        let (addr, seq) = *key;
                        entry.attempts = entry.attempts.saturating_add(1);
                        entry.last_send_ms = now;
                        resend.push((addr, seq, entry.packet_type, entry.payload.clone(), entry.attempts));
                    }
                }

                for k in to_remove.iter() {
                    pending.remove(k);
                }
            }

            for (addr, seq, packet_type, payload, attempt) in resend {
                debug!(
                    "reliable resend -> {} type={:?} seq={} attempt={} bytes={}",
                    addr,
                    packet_type,
                    seq,
                    attempt,
                    payload.len()
                );
                let _ = self
                    .send_packet_reliable_with_seq(packet_type, &payload, addr, seq)
                    .await;
            }
        }
    }

    async fn receive_loop(&self) -> Result<()> {
        let mut buf = vec![0u8; 2048];

        loop {
            match self.socket.recv_from(&mut buf).await {
                Ok((len, addr)) => {
                    if len == 0 {
                        continue;
                    }

                    self.state.update_player_last_seen(&addr).await;

                    if let Err(e) = self.handle_packet(&buf[..len], addr).await {
                        warn!("Error handling packet from {}: {}", addr, e);
                    }
                }
                Err(e) => {
                    error!("Socket receive error: {}", e);
                }
            }
        }
    }

    async fn cleanup_loop(&self) {
        let mut interval = time::interval(Duration::from_secs(30));

        loop {
            interval.tick().await;

            self.state.cleanup_timed_out_players().await;
            self.state.cleanup_idle_lobbies().await;

            if let Err(e) = self.state.save_all_lobbies().await {
                warn!("Failed to save lobbies: {}", e);
            }
        }
    }

    async fn handle_packet(&self, data: &[u8], addr: SocketAddr) -> Result<()> {
        if data.is_empty() {
            return Ok(());
        }

        let packet_type = PacketType::from(data[0]);
        let payload = &data[1..];

        let payload_buf_opt = match packet_type {
            PacketType::ReliableAck => {
                if payload.len() >= 4 {
                    let seq = u32::from_le_bytes([payload[0], payload[1], payload[2], payload[3]]);
                    let mut pending = self.reliable_pending.lock().await;
                    debug!("reliable ack <- {} seq={} (pending before={})", addr, seq, pending.len());
                    pending.remove(&(addr, seq));
                }
                Some(Vec::new())
            }
            _ => self
                .maybe_strip_reliable_prefix(packet_type, payload, addr)
                .await?,
        };

        if payload_buf_opt.is_none() {
            return Ok(());
        }

        let payload_buf = payload_buf_opt.unwrap();
        let payload = payload_buf.as_slice();

        match packet_type {
            PacketType::Handshake => self.handle_handshake(payload, addr).await?,
            PacketType::Ping => self.handle_ping(addr).await?,
            PacketType::JiggyCollected => self.handle_jiggy_collected(payload, addr).await?,
            PacketType::HoneycombCollected => {
                self.handle_honeycomb_collected(payload, addr).await?
            }
            PacketType::MumboTokenCollected => {
                self.handle_mumbo_token_collected(payload, addr).await?
            }
            PacketType::NoteCollectedPos => self.handle_note_collected_pos(payload, addr).await?,
            PacketType::NoteCollected => self.handle_note_collected(payload, addr).await?,
            PacketType::FullSyncRequest => self.handle_full_sync_request(addr).await?,
            PacketType::NoteSaveData => self.handle_note_save_data(payload, addr).await?,
            PacketType::FileProgressFlags => self.handle_file_progress_flags(payload, addr).await?,
            PacketType::AbilityProgress => self.handle_ability_progress(payload, addr).await?,
            PacketType::HoneycombScore => self.handle_honeycomb_score(payload, addr).await?,
            PacketType::MumboScore => self.handle_mumbo_score(payload, addr).await?,
            PacketType::PuppetUpdate => self.handle_puppet_update(payload, addr).await?,
            PacketType::PuppetSyncRequest => self.handle_puppet_sync_request(addr).await?,
            PacketType::LevelOpened => self.handle_level_opened(payload, addr).await?,
            PacketType::ReliableAck => {}
            _ => {
                debug!("Unknown packet type: {:?} from {}", packet_type, addr);
            }
        }

        Ok(())
    }

    async fn handle_handshake(&self, payload: &[u8], addr: SocketAddr) -> Result<()> {
        let login = LoginPacket::deserialize(payload)?;

        info!(
            "Handshake received from {} - User: {}, Lobby: {}",
            addr, login.username, login.lobby_name
        );

        let lobby = self
            .state
            .get_or_create_lobby(&login.lobby_name, &login.password)
            .await?;

        {
            let lob = lobby.read().await;
            if !lob.password.is_empty() && lob.password != login.password {
                warn!(
                    "Invalid password for lobby {} from socket {}",
                    login.lobby_name, addr
                );
                return Ok(());
            }

            if lob.player_count() >= self.state.config().server.max_players_per_lobby {
                warn!(
                    "Connection failed for {}: lobby {} is full",
                    addr, login.lobby_name
                );
                return Ok(());
            }
        }

        let player = self
            .state
            .get_or_create_player(addr, &login.username, &login.lobby_name)
            .await;

        let player_id = {
            let p = player.read().await;
            p.id
        };

        let lobby_needs_initial_save_data = {
            let mut lob = lobby.write().await;
            let needs = !lob.has_initial_save_data;
            lob.add_player(player_id, login.username.clone());
            needs
        };

        self.send_packet(PacketType::Pong, &[], addr).await?;

        if lobby_needs_initial_save_data {
            info!(
                "Lobby {} needs initial save data; requesting upload from {}",
                login.lobby_name, login.username
            );
            self.send_packet(PacketType::InitialSaveDataRequest, &[], addr)
                .await?;
        }

        self.broadcast_player_connected(&login.lobby_name, &login.username, player_id, addr)
            .await?;

        self.send_full_lobby_state(&login.lobby_name, addr).await?;

        Ok(())
    }

    async fn handle_ping(&self, addr: SocketAddr) -> Result<()> {
        self.send_packet(PacketType::Pong, &[], addr).await
    }

    async fn handle_jiggy_collected(&self, payload: &[u8], addr: SocketAddr) -> Result<()> {
        let jiggy = JiggyPacket::deserialize(payload)?;

        let player = self.state.get_player_by_addr(&addr);
        if player.is_none() {
            return Ok(());
        }

        let player_arc = player.unwrap();
        let (lobby_name, username, player_id) = {
            let p = player_arc.read().await;
            (p.lobby_name.clone(), p.username.clone(), p.id)
        };

        let lobby = self.state.get_lobby(&lobby_name);
        if lobby.is_none() {
            return Ok(());
        }

        let lobby_arc = lobby.unwrap();
        let added = {
            let mut lob = lobby_arc.write().await;
            lob.add_collected_jiggy(jiggy.jiggy_enum_id, jiggy.collected_value, username.clone())
        };

        if added {
            // Manually serialize broadcast
            let mut payload = Vec::new();
            payload.extend_from_slice(&player_id.to_be_bytes());
            payload.extend_from_slice(&jiggy.jiggy_enum_id.to_be_bytes());
            payload.extend_from_slice(&jiggy.collected_value.to_be_bytes());

            self.broadcast_to_lobby_except(
                &lobby_name,
                addr,
                PacketType::JiggyCollected,
                &payload,
            )
            .await?;
        }

        Ok(())
    }

    async fn handle_honeycomb_collected(&self, payload: &[u8], addr: SocketAddr) -> Result<()> {
        let hc = HoneycombCollectedPacket::deserialize(payload)?;

        let player = self.state.get_player_by_addr(&addr);
        if player.is_none() {
            return Ok(());
        }

        let player_arc = player.unwrap();
        let (lobby_name, username, player_id) = {
            let p = player_arc.read().await;
            (p.lobby_name.clone(), p.username.clone(), p.id)
        };

        let lobby = self.state.get_lobby(&lobby_name);
        if lobby.is_none() {
            return Ok(());
        }

        let lobby_arc = lobby.unwrap();
        let added = {
            let mut l = lobby_arc.write().await;
            l.add_collected_honeycomb(
                hc.map_id,
                hc.honeycomb_id,
                hc.x,
                hc.y,
                hc.z,
                username.clone(),
            )
        };

        if added {
            // Manually serialize the broadcast with player_id
            let mut payload = Vec::new();
            payload.extend_from_slice(&player_id.to_be_bytes());
            payload.extend_from_slice(&hc.map_id.to_be_bytes());
            payload.extend_from_slice(&hc.honeycomb_id.to_be_bytes());
            payload.extend_from_slice(&hc.x.to_be_bytes());
            payload.extend_from_slice(&hc.y.to_be_bytes());
            payload.extend_from_slice(&hc.z.to_be_bytes());

            self.broadcast_to_lobby_except(
                &lobby_name,
                addr,
                PacketType::HoneycombCollected,
                &payload,
            )
            .await?;
        }

        Ok(())
    }

    async fn handle_mumbo_token_collected(&self, payload: &[u8], addr: SocketAddr) -> Result<()> {
        let tok = MumboTokenCollectedPacket::deserialize(payload)?;

        let player = self.state.get_player_by_addr(&addr);
        if player.is_none() {
            return Ok(());
        }

        let player_arc = player.unwrap();
        let (lobby_name, username, player_id) = {
            let p = player_arc.read().await;
            (p.lobby_name.clone(), p.username.clone(), p.id)
        };

        let lobby = self.state.get_lobby(&lobby_name);
        if lobby.is_none() {
            return Ok(());
        }

        let lobby_arc = lobby.unwrap();
        let added = {
            let mut l = lobby_arc.write().await;
            l.add_collected_mumbo_token(
                tok.map_id,
                tok.token_id,
                tok.x,
                tok.y,
                tok.z,
                username.clone(),
            )
        };

        if added {
            // Manually serialize broadcast
            let mut payload = Vec::new();
            payload.extend_from_slice(&player_id.to_be_bytes());
            payload.extend_from_slice(&tok.map_id.to_be_bytes());
            payload.extend_from_slice(&tok.token_id.to_be_bytes());
            payload.extend_from_slice(&tok.x.to_be_bytes());
            payload.extend_from_slice(&tok.y.to_be_bytes());
            payload.extend_from_slice(&tok.z.to_be_bytes());

            self.broadcast_to_lobby_except(
                &lobby_name,
                addr,
                PacketType::MumboTokenCollected,
                &payload,
            )
            .await?;
        }

        Ok(())
    }

    async fn handle_level_opened(&self, payload: &[u8], addr: SocketAddr) -> Result<()> {
        let level = LevelOpenedPacket::deserialize(payload)?;

        let player = self.state.get_player_by_addr(&addr);
        if player.is_none() {
            return Ok(());
        }

        let player_arc = player.unwrap();
        let (lobby_name, username, player_id) = {
            let p = player_arc.read().await;
            (p.lobby_name.clone(), p.username.clone(), p.id)
        };

        let lobby = self.state.get_lobby(&lobby_name);
        if lobby.is_none() {
            return Ok(());
        }

        let lobby_arc = lobby.unwrap();
        let added = {
            let mut l = lobby_arc.write().await;
            l.add_opened_level(level.world_id, level.jiggy_cost)
        };

        if added {
            info!(
                "Level opened: World={}, Cost={} by {} in lobby {}",
                level.world_id, level.jiggy_cost, username, lobby_name
            );

            // Manually serialize broadcast
            let mut payload = Vec::new();
            payload.extend_from_slice(&player_id.to_be_bytes());
            payload.extend_from_slice(&level.world_id.to_be_bytes());
            payload.extend_from_slice(&level.jiggy_cost.to_be_bytes());

            self.broadcast_to_lobby_except(&lobby_name, addr, PacketType::LevelOpened, &payload)
                .await?;
        }

        Ok(())
    }

    async fn handle_note_collected_pos(&self, payload: &[u8], addr: SocketAddr) -> Result<()> {
        let note = NotePacketPos::deserialize(payload)?;

        let player = self.state.get_player_by_addr(&addr);
        if player.is_none() {
            return Ok(());
        }

        let player_arc = player.unwrap();
        let (lobby_name, username, player_id) = {
            let p = player_arc.read().await;
            (p.lobby_name.clone(), p.username.clone(), p.id)
        };

        let lobby = self.state.get_lobby(&lobby_name);
        if lobby.is_none() {
            return Ok(());
        }

        let lobby_arc = lobby.unwrap();
        let added = {
            let mut l = lobby_arc.write().await;
            l.add_collected_note(note.map_id, note.x as i16, note.y as i16, note.z as i16, username.clone())
        };

        if added {
            info!(
                "Note collected: Map={}, Pos=({},{},{}) by {} in lobby {}",
                note.map_id, note.x, note.y, note.z, username, lobby_name
            );

            // Manually serialize broadcast
            let mut payload = Vec::new();
            payload.extend_from_slice(&player_id.to_be_bytes());
            payload.extend_from_slice(&note.map_id.to_be_bytes());
            payload.extend_from_slice(&note.x.to_be_bytes());
            payload.extend_from_slice(&note.y.to_be_bytes());
            payload.extend_from_slice(&note.z.to_be_bytes());

            self.broadcast_to_lobby_except(
                &lobby_name,
                addr,
                PacketType::NoteCollectedPos,
                &payload,
            )
            .await?;
        }

        Ok(())
    }

    async fn handle_note_collected(&self, payload: &[u8], addr: SocketAddr) -> Result<()> {
        let note = NotePacket::deserialize(payload)?;

        let player = self.state.get_player_by_addr(&addr);
        if player.is_none() {
            return Ok(());
        }

        let player_arc = player.unwrap();
        let (lobby_name, username, player_id) = {
            let p = player_arc.read().await;
            (p.lobby_name.clone(), p.username.clone(), p.id)
        };

        info!(
            "Note collected (legacy): Map={}, Note={} by {} in lobby {}",
            note.map_id, note.note_index, username, lobby_name
        );

        // Manually serialize broadcast
        let mut payload = Vec::new();
        payload.extend_from_slice(&player_id.to_be_bytes());
        payload.extend_from_slice(&note.map_id.to_be_bytes());
        payload.extend_from_slice(&note.level_id.to_be_bytes());
        payload.extend_from_slice(&(if note.is_dynamic { 1i32 } else { 0i32 }).to_be_bytes());
        payload.extend_from_slice(&note.note_index.to_be_bytes());

        info!(
            "Broadcasting note: player_id={}, map={}, level={}, is_dynamic={}, note_index={}",
            player_id, note.map_id, note.level_id, note.is_dynamic, note.note_index
        );

        self.broadcast_to_lobby_except(&lobby_name, addr, PacketType::NoteCollected, &payload)
            .await?;

        Ok(())
    }

    async fn handle_note_save_data(&self, payload: &[u8], addr: SocketAddr) -> Result<()> {
        let data = NoteSaveDataPacket::deserialize(payload)?;

        let player = self.state.get_player_by_addr(&addr);
        if player.is_none() {
            return Ok(());
        }

        let player_arc = player.unwrap();
        let lobby_name = {
            let p = player_arc.read().await;
            p.lobby_name.clone()
        };

        let lobby = self.state.get_lobby(&lobby_name);
        if let Some(lobby_arc) = lobby {
            let mut l = lobby_arc.write().await;
            if l.update_note_save_data(data.level_index as usize, &data.save_data) {
                info!(
                    "Updated note save data for level {} in lobby {}",
                    data.level_index, lobby_name
                );

                l.has_initial_save_data = true;
            }
        }

        Ok(())
    }

    async fn handle_file_progress_flags(&self, payload: &[u8], addr: SocketAddr) -> Result<()> {
        let data = FileProgressFlagsPacket::deserialize(payload)?;

        let player = self.state.get_player_by_addr(&addr);
        if player.is_none() {
            return Ok(());
        }

        let player_arc = player.unwrap();
        let (lobby_name, player_id) = {
            let p = player_arc.read().await;
            (p.lobby_name.clone(), p.id)
        };

        let lobby = self.state.get_lobby(&lobby_name);
        if let Some(lobby_arc) = lobby {
            let mut l = lobby_arc.write().await;

            if l.update_file_progress_flags(&data.flags) {
                l.has_initial_save_data = true;
            }
        }

        // Manually serialize: player_id + raw bytes
        let mut payload = Vec::new();
        payload.extend_from_slice(&player_id.to_be_bytes());
        payload.extend_from_slice(&data.flags);

        self.broadcast_to_lobby_except(&lobby_name, addr, PacketType::FileProgressFlags, &payload)
            .await?;

        Ok(())
    }

    async fn handle_ability_progress(&self, payload: &[u8], addr: SocketAddr) -> Result<()> {
        let data = AbilityProgressPacket::deserialize(payload)?;

        let player = self.state.get_player_by_addr(&addr);
        if player.is_none() {
            return Ok(());
        }

        let player_arc = player.unwrap();
        let (lobby_name, player_id) = {
            let p = player_arc.read().await;
            (p.lobby_name.clone(), p.id)
        };

        let lobby = self.state.get_lobby(&lobby_name);
        if let Some(lobby_arc) = lobby {
            let mut l = lobby_arc.write().await;
            l.update_ability_progress(&data.bytes);
        }

        // Manually serialize: player_id + raw bytes
        let mut payload = Vec::new();
        payload.extend_from_slice(&player_id.to_be_bytes());
        payload.extend_from_slice(&data.bytes);

        self.broadcast_to_lobby_except(&lobby_name, addr, PacketType::AbilityProgress, &payload)
            .await?;

        Ok(())
    }

    async fn handle_honeycomb_score(&self, payload: &[u8], addr: SocketAddr) -> Result<()> {
        let data = HoneycombScorePacket::deserialize(payload)?;

        let player = self.state.get_player_by_addr(&addr);
        if player.is_none() {
            return Ok(());
        }

        let player_arc = player.unwrap();
        let (lobby_name, player_id) = {
            let p = player_arc.read().await;
            (p.lobby_name.clone(), p.id)
        };

        let lobby = self.state.get_lobby(&lobby_name);
        if let Some(lobby_arc) = lobby {
            let mut l = lobby_arc.write().await;
            if l.update_honeycomb_score(&data.bytes) {
                l.has_initial_save_data = true;
            }
        }

        // Manually serialize: player_id + raw bytes
        let mut payload = Vec::new();
        payload.extend_from_slice(&player_id.to_be_bytes());
        payload.extend_from_slice(&data.bytes);

        self.broadcast_to_lobby_except(&lobby_name, addr, PacketType::HoneycombScore, &payload)
            .await?;

        Ok(())
    }

    async fn handle_mumbo_score(&self, payload: &[u8], addr: SocketAddr) -> Result<()> {
        let data = MumboScorePacket::deserialize(payload)?;

        let player = self.state.get_player_by_addr(&addr);
        if player.is_none() {
            return Ok(());
        }

        let player_arc = player.unwrap();
        let (lobby_name, player_id) = {
            let p = player_arc.read().await;
            (p.lobby_name.clone(), p.id)
        };

        let lobby = self.state.get_lobby(&lobby_name);
        if let Some(lobby_arc) = lobby {
            let mut l = lobby_arc.write().await;
            if l.update_mumbo_score(&data.bytes) {
                l.has_initial_save_data = true;
            }
        }

        // Manually serialize: player_id + raw bytes
        let mut payload = Vec::new();
        payload.extend_from_slice(&player_id.to_be_bytes());
        payload.extend_from_slice(&data.bytes);

        self.broadcast_to_lobby_except(&lobby_name, addr, PacketType::MumboScore, &payload)
            .await?;

        Ok(())
    }

    async fn handle_full_sync_request(&self, addr: SocketAddr) -> Result<()> {
        let player = self.state.get_player_by_addr(&addr);
        if player.is_none() {
            return Ok(());
        }

        let player_arc = player.unwrap();
        let lobby_name = {
            let p = player_arc.read().await;
            p.lobby_name.clone()
        };

        self.send_full_lobby_state(&lobby_name, addr).await
    }

    async fn handle_puppet_update(&self, payload: &[u8], addr: SocketAddr) -> Result<()> {
        let player = self.state.get_player_by_addr(&addr);
        if player.is_none() {
            return Ok(());
        }

        let player_arc = player.unwrap();
        let (lobby_name, player_id) = {
            let p = player_arc.read().await;
            (p.lobby_name.clone(), p.id)
        };

        {
            let mut p = player_arc.write().await;
            p.last_puppet_state = Some(payload.to_vec());
        }

        let mut forwarded_payload = Vec::with_capacity(4 + payload.len());
        forwarded_payload.extend_from_slice(&player_id.to_le_bytes());
        forwarded_payload.extend_from_slice(payload);

        let addresses = self
            .state
            .get_lobby_players_except(&lobby_name, player_id)
            .await;

        for target_addr in addresses {
            if target_addr != addr {
                if let Err(e) = self
                    .send_packet(PacketType::PuppetUpdate, &forwarded_payload, target_addr)
                    .await
                {
                    debug!("Failed to forward puppet update to {}: {}", target_addr, e);
                }
            }
        }

        Ok(())
    }

    async fn handle_puppet_sync_request(&self, addr: SocketAddr) -> Result<()> {
        let player = self.state.get_player_by_addr(&addr);
        if player.is_none() {
            return Ok(());
        }

        let player_arc = player.unwrap();
        let lobby_name = {
            let p = player_arc.read().await;
            p.lobby_name.clone()
        };

        let players = self.state.get_lobby_players(&lobby_name).await;

        for other_player in players {
            let other_addr = other_player.read().await.address;

            if other_addr == addr {
                continue;
            }

            let puppet_state = {
                let p = other_player.read().await;
                p.last_puppet_state.clone()
            };

            if let Some(state) = puppet_state {
                let other_id = {
                    let p = other_player.read().await;
                    p.id
                };

                let mut forwarded_payload = Vec::with_capacity(4 + state.len());
                forwarded_payload.extend_from_slice(&other_id.to_le_bytes());
                forwarded_payload.extend_from_slice(&state);

                if let Err(e) = self
                    .send_packet(PacketType::PuppetUpdate, &forwarded_payload, addr)
                    .await
                {
                    warn!("Failed to send puppet state to {}: {}", addr, e);
                }
            }
        }

        info!("Sent puppet states to new player at {}", addr);
        Ok(())
    }

    async fn send_full_lobby_state(&self, lobby_name: &str, addr: SocketAddr) -> Result<()> {
        let lobby = self.state.get_lobby(lobby_name);
        if lobby.is_none() {
            return Ok(());
        }

        let lobby_arc = lobby.unwrap();
        let l = lobby_arc.read().await;

        for level_index in 0..9 {
            let save_data = &l.save_flags.note_save_data[level_index];
            let packet = NoteSaveDataPacket {
                level_index: level_index as i32,
                save_data: save_data.clone(),
            };
            let payload = packet.serialize();
            self.send_packet_serialized(PacketType::NoteSaveData, &payload, addr)
                .await?;
        }

        let fp = FileProgressFlagsPacket {
            flags: l.save_flags.file_progress_flags.clone(),
        };
        let fp_payload = fp.serialize();
        self.send_packet_serialized(PacketType::FileProgressFlags, &fp_payload, addr)
            .await?;

        let ap = AbilityProgressPacket {
            bytes: l.save_flags.ability_progress.clone(),
        };
        let ap_payload = ap.serialize();
        self.send_packet_serialized(PacketType::AbilityProgress, &ap_payload, addr)
            .await?;

        let hc = HoneycombScorePacket {
            bytes: l.save_flags.honeycomb_score.clone(),
        };
        let hc_payload = hc.serialize();
        self.send_packet_serialized(PacketType::HoneycombScore, &hc_payload, addr)
            .await?;

        let ms = MumboScorePacket {
            bytes: l.save_flags.mumbo_score.clone(),
        };
        let ms_payload = ms.serialize();
        self.send_packet_serialized(PacketType::MumboScore, &ms_payload, addr)
            .await?;

        for jiggy in &l.collected_jiggies {
            let broadcast = BroadcastJiggy {
                jiggy_enum_id: jiggy.level_id,
                collected_value: jiggy.jiggy_id,
                collector: jiggy.collected_by.clone(),
            };
            // Use dummy player_id 0 for historical data
            let jiggy_payload = broadcast.serialize(0);
            self.send_packet_serialized(PacketType::JiggyCollected, &jiggy_payload, addr)
                .await?;
        }

        for hc in &l.collected_honeycombs {
            // Manual serialization: player_id (dummy 0) + map_id + honeycomb_id + x + y + z
            let mut payload = Vec::new();
            payload.extend_from_slice(&0u32.to_be_bytes()); // dummy player_id
            payload.extend_from_slice(&hc.map_id.to_be_bytes());
            payload.extend_from_slice(&hc.honeycomb_id.to_be_bytes());
            payload.extend_from_slice(&hc.x.to_be_bytes());
            payload.extend_from_slice(&hc.y.to_be_bytes());
            payload.extend_from_slice(&hc.z.to_be_bytes());
            self.send_packet_serialized(PacketType::HoneycombCollected, &payload, addr)
                .await?;
        }

        for tok in &l.collected_mumbo_tokens {
            // Manual serialization: player_id (dummy 0) + map_id + token_id + x + y + z
            let mut payload = Vec::new();
            payload.extend_from_slice(&0u32.to_be_bytes()); // dummy player_id
            payload.extend_from_slice(&tok.map_id.to_be_bytes());
            payload.extend_from_slice(&tok.token_id.to_be_bytes());
            payload.extend_from_slice(&tok.x.to_be_bytes());
            payload.extend_from_slice(&tok.y.to_be_bytes());
            payload.extend_from_slice(&tok.z.to_be_bytes());
            self.send_packet_serialized(PacketType::MumboTokenCollected, &payload, addr)
                .await?;
        }

        for opened in &l.opened_levels {
            let packet = LevelOpenedPacket {
                world_id: opened.world_id,
                jiggy_cost: opened.jiggy_cost,
            };
            let payload = packet.serialize();
            self.send_packet_serialized(PacketType::LevelOpened, &payload, addr)
                .await?;
        }

        info!(
            "Sent full state to player at {} (9 levels of note data + save blobs + {} jiggies + {} honeycombs + {} tokens + {} opened levels)",
            addr,
            l.collected_jiggies.len(),
            l.collected_honeycombs.len(),
            l.collected_mumbo_tokens.len(),
            l.opened_levels.len()
        );

        Ok(())
    }

    async fn broadcast_player_connected(
        &self,
        lobby_name: &str,
        username: &str,
        player_id: u32,
        except_addr: SocketAddr,
    ) -> Result<()> {
        let broadcast = PlayerConnectedBroadcast {
            username: username.to_string(),
            player_id,
        };

        let payload = broadcast.serialize();

        self.broadcast_to_lobby_except(
            lobby_name,
            except_addr,
            PacketType::PlayerConnected,
            &payload,
        )
        .await
    }

    async fn broadcast_to_lobby_except(
        &self,
        lobby_name: &str,
        except_addr: SocketAddr,
        packet_type: PacketType,
        payload: &[u8],
    ) -> Result<()> {
        let addresses = self.state.get_lobby_players_except(lobby_name, 0).await;

        for addr in addresses {
            if addr != except_addr {
                if let Err(e) = self.send_packet_maybe_reliable(packet_type, payload, addr).await {
                    warn!("Failed to send to {}: {}", addr, e);
                }
            }
        }

        Ok(())
    }

    async fn send_packet_serialized(
        &self,
        packet_type: PacketType,
        payload: &[u8],
        addr: SocketAddr,
    ) -> Result<()> {
        self.send_packet_maybe_reliable(packet_type, payload, addr).await
    }

    async fn send_packet_maybe_reliable(
        &self,
        packet_type: PacketType,
        payload: &[u8],
        addr: SocketAddr,
    ) -> Result<()> {
        if Self::is_reliable_type(packet_type) {
            self.send_packet_reliable(packet_type, payload, addr).await
        } else {
            self.send_packet(packet_type, payload, addr).await
        }
    }

    async fn send_packet_reliable(
        &self,
        packet_type: PacketType,
        payload: &[u8],
        addr: SocketAddr,
    ) -> Result<()> {
        const MAX_PENDING_PER_ADDR: usize = 256;

        {
            let pending = self.reliable_pending.lock().await;
            let count = pending.keys().filter(|(a, _)| *a == addr).count();
            if count >= MAX_PENDING_PER_ADDR {
                return Ok(());
            }
        }

        let seq = self
            .reliable_next_seq
            .fetch_add(1, std::sync::atomic::Ordering::Relaxed);

        {
            let mut pending = self.reliable_pending.lock().await;
            pending.insert(
                (addr, seq),
                PendingReliable {
                    packet_type,
                    payload: payload.to_vec(),
                    last_send_ms: 0,
                    attempts: 0,
                },
            );
        }

        self.send_packet_reliable_with_seq(packet_type, payload, addr, seq)
            .await
    }

    async fn send_packet_reliable_with_seq(
        &self,
        packet_type: PacketType,
        payload: &[u8],
        addr: SocketAddr,
        seq: u32,
    ) -> Result<()> {
        let mut buf = Vec::with_capacity(1 + 4 + payload.len());
        buf.push(packet_type as u8);
        buf.extend_from_slice(&seq.to_le_bytes());
        buf.extend_from_slice(payload);
        self.socket.send_to(&buf, addr).await?;
        Ok(())
    }

    async fn send_packet(
        &self,
        packet_type: PacketType,
        payload: &[u8],
        addr: SocketAddr,
    ) -> Result<()> {
        let mut buf = Vec::with_capacity(1 + payload.len());
        buf.push(packet_type as u8);
        buf.extend_from_slice(payload);

        self.socket.send_to(&buf, addr).await?;
        Ok(())
    }
}
