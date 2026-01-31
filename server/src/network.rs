use anyhow::Result;
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
}

impl NetworkServer {
    pub fn new(socket: Arc<UdpSocket>, config: Config) -> Self {
        Self {
            socket,
            state: Arc::new(ServerState::new(config)),
        }
    }

    pub async fn run(self) -> Result<()> {
        let server = Arc::new(self);

        let cleanup_server = server.clone();
        tokio::spawn(async move {
            cleanup_server.cleanup_loop().await;
        });

        server.receive_loop().await
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

        match packet_type {
            PacketType::Handshake => self.handle_handshake(payload, addr).await?,
            PacketType::Ping => self.handle_ping(addr).await?,
            PacketType::JiggyCollected => self.handle_jiggy_collected(payload, addr).await?,
            PacketType::NoteCollectedPos => self.handle_note_collected_pos(payload, addr).await?,
            PacketType::NoteCollected => self.handle_note_collected(payload, addr).await?,
            PacketType::FullSyncRequest => self.handle_full_sync_request(addr).await?,
            PacketType::NoteSaveData => self.handle_note_save_data(payload, addr).await?,
            PacketType::PuppetUpdate => self.handle_puppet_update(payload, addr).await?,
            PacketType::PuppetSyncRequest => self.handle_puppet_sync_request(addr).await?,
            PacketType::LevelOpened => self.handle_level_opened(payload, addr).await?,
            _ => {
                debug!("Unknown packet type: {:?} from {}", packet_type, addr);
            }
        }

        Ok(())
    }

    async fn handle_handshake(&self, payload: &[u8], addr: SocketAddr) -> Result<()> {
        let login: LoginPacket = rmp_serde::from_slice(payload)?;

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

        let is_first_player = {
            let mut lob = lobby.write().await;
            let was_empty = lob.player_count() == 0;

            lob.add_player(player_id, login.username.clone());

            was_empty && !lob.has_initial_save_data
        };

        self.send_packet(PacketType::Pong, &[], addr).await?;

        if is_first_player {
            info!(
                "First player {} joined lobby {}, getting save data from player",
                login.username, login.lobby_name
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
        let jiggy: JiggyPacket = rmp_serde::from_slice(payload)?;

        let player = self.state.get_player_by_addr(&addr);
        if player.is_none() {
            return Ok(());
        }

        let player_arc = player.unwrap();
        let (lobby_name, username) = {
            let p = player_arc.read().await;
            (p.lobby_name.clone(), p.username.clone())
        };

        let lobby = self.state.get_lobby(&lobby_name);
        if lobby.is_none() {
            return Ok(());
        }

        let lobby_arc = lobby.unwrap();
        let added = {
            let mut lob = lobby_arc.write().await;
            lob.add_collected_jiggy(jiggy.level_id, jiggy.jiggy_id, username.clone())
        };

        if added {
            debug!(
                "Jiggy collected: Level={}, Jiggy={} by {} in lobby {}",
                jiggy.level_id, jiggy.jiggy_id, username, lobby_name
            );

            let broadcast = BroadcastJiggy {
                level_id: jiggy.level_id,
                jiggy_id: jiggy.jiggy_id,
                collector: username,
            };

            self.broadcast_to_lobby_except(
                &lobby_name,
                addr,
                PacketType::JiggyCollected,
                &broadcast,
            )
            .await?;
        }

        Ok(())
    }

    async fn handle_level_opened(&self, payload: &[u8], addr: SocketAddr) -> Result<()> {
        let level: LevelOpenedPacket = rmp_serde::from_slice(payload)?;

        let player = self.state.get_player_by_addr(&addr);
        if player.is_none() {
            return Ok(());
        }

        let player_arc = player.unwrap();
        let (lobby_name, username) = {
            let p = player_arc.read().await;
            (p.lobby_name.clone(), p.username.clone())
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

            self.broadcast_to_lobby_except(&lobby_name, addr, PacketType::LevelOpened, &level)
                .await?;
        }

        Ok(())
    }

    async fn handle_note_collected_pos(&self, payload: &[u8], addr: SocketAddr) -> Result<()> {
        let note: NotePacketPos = rmp_serde::from_slice(payload)?;

        let player = self.state.get_player_by_addr(&addr);
        if player.is_none() {
            return Ok(());
        }

        let player_arc = player.unwrap();
        let (lobby_name, username) = {
            let p = player_arc.read().await;
            (p.lobby_name.clone(), p.username.clone())
        };

        let lobby = self.state.get_lobby(&lobby_name);
        if lobby.is_none() {
            return Ok(());
        }

        let lobby_arc = lobby.unwrap();
        let added = {
            let mut l = lobby_arc.write().await;
            l.add_collected_note(note.map_id, note.x, note.y, note.z, username.clone())
        };

        if added {
            info!(
                "Note collected: Map={}, Pos=({},{},{}) by {} in lobby {}",
                note.map_id, note.x, note.y, note.z, username, lobby_name
            );

            let broadcast = BroadcastNotePos {
                map_id: note.map_id,
                x: note.x,
                y: note.y,
                z: note.z,
                collector: username,
            };

            self.broadcast_to_lobby_except(
                &lobby_name,
                addr,
                PacketType::NoteCollectedPos,
                &broadcast,
            )
            .await?;
        }

        Ok(())
    }

    async fn handle_note_collected(&self, payload: &[u8], addr: SocketAddr) -> Result<()> {
        let note: NotePacket = rmp_serde::from_slice(payload)?;

        let player = self.state.get_player_by_addr(&addr);
        if player.is_none() {
            return Ok(());
        }

        let player_arc = player.unwrap();
        let (lobby_name, username) = {
            let p = player_arc.read().await;
            (p.lobby_name.clone(), p.username.clone())
        };

        info!(
            "Note collected (legacy): Map={}, Note={} by {} in lobby {}",
            note.map_id, note.note_index, username, lobby_name
        );

        let broadcast = BroadcastNote {
            map_id: note.map_id,
            level_id: note.level_id,
            is_dynamic: note.is_dynamic,
            note_index: note.note_index,
            collector: username,
        };

        self.broadcast_to_lobby_except(&lobby_name, addr, PacketType::NoteCollected, &broadcast)
            .await?;

        Ok(())
    }

    async fn handle_note_save_data(&self, payload: &[u8], addr: SocketAddr) -> Result<()> {
        let data: NoteSaveDataPacket = rmp_serde::from_slice(payload)?;

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
                // Forward with player_id prepended
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
            self.send_packet_serialized(PacketType::NoteSaveData, &packet, addr)
                .await?;
        }

        for jiggy in &l.collected_jiggies {
            let broadcast = BroadcastJiggy {
                level_id: jiggy.level_id,
                jiggy_id: jiggy.jiggy_id,
                collector: jiggy.collected_by.clone(),
            };
            self.send_packet_serialized(PacketType::JiggyCollected, &broadcast, addr)
                .await?;
        }

        info!(
            "Sent full state to player at {} (9 levels of note data, {} jiggies)",
            addr,
            l.collected_jiggies.len()
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

        self.broadcast_to_lobby_except(
            lobby_name,
            except_addr,
            PacketType::PlayerConnected,
            &broadcast,
        )
        .await
    }

    async fn broadcast_to_lobby_except<T: serde::Serialize>(
        &self,
        lobby_name: &str,
        except_addr: SocketAddr,
        packet_type: PacketType,
        data: &T,
    ) -> Result<()> {
        let payload = rmp_serde::to_vec(data)?;
        let addresses = self.state.get_lobby_players_except(lobby_name, 0).await;

        for addr in addresses {
            if addr != except_addr {
                if let Err(e) = self.send_packet(packet_type, &payload, addr).await {
                    warn!("Failed to send to {}: {}", addr, e);
                }
            }
        }

        Ok(())
    }

    async fn send_packet_serialized<T: serde::Serialize>(
        &self,
        packet_type: PacketType,
        data: &T,
        addr: SocketAddr,
    ) -> Result<()> {
        let payload = rmp_serde::to_vec(data)?;
        self.send_packet(packet_type, &payload, addr).await
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
