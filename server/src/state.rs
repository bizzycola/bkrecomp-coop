use anyhow::Result;
use dashmap::DashMap;
use std::net::SocketAddr;
use std::sync::atomic::{AtomicU32, Ordering};
use std::sync::Arc;
use tokio::sync::RwLock;
use tracing::{info, warn};

use crate::config::Config;
use crate::lobby::Lobby;
use crate::player::Player;

pub struct ServerState {
    config: Config,

    lobbies: DashMap<String, Arc<RwLock<Lobby>>>,

    players: DashMap<u32, Arc<RwLock<Player>>>,

    addr_to_player: DashMap<SocketAddr, u32>,

    next_player_id: AtomicU32,
}

impl ServerState {
    pub fn new(config: Config) -> Self {
        let lobbies = DashMap::new();

        if config.server.enable_persistence {
            if let Err(e) = Self::load_lobbies(&lobbies, &config.server.persistence_dir) {
                warn!("Failed to load saved lobbies: {}", e);
            }
        }

        Self {
            config,
            lobbies,
            players: DashMap::new(),
            addr_to_player: DashMap::new(),
            next_player_id: AtomicU32::new(1),
        }
    }

    pub fn config(&self) -> &Config {
        &self.config
    }

    pub async fn get_or_create_lobby(
        &self,
        name: &str,
        password: &str,
    ) -> Result<Arc<RwLock<Lobby>>> {
        if let Some(lobby) = self.lobbies.get(name) {
            return Ok(lobby.clone());
        }

        if self.lobbies.len() >= self.config.server.max_lobbies {
            anyhow::bail!("Max number of lobbies reached");
        }

        let lobby = Arc::new(RwLock::new(Lobby::new(
            name.to_string(),
            password.to_string(),
        )));
        self.lobbies.insert(name.to_string(), lobby.clone());

        info!("Created lobby {}", name);

        Ok(lobby)
    }

    pub fn get_lobby(&self, name: &str) -> Option<Arc<RwLock<Lobby>>> {
        self.lobbies.get(name).map(|l| l.clone())
    }

    pub async fn get_or_create_player(
        &self,
        addr: SocketAddr,
        username: &str,
        lobby_name: &str,
    ) -> Arc<RwLock<Player>> {
        if let Some(player_id) = self.addr_to_player.get(&addr) {
            if let Some(player) = self.players.get(player_id.value()) {
                let mut p = player.write().await;
                p.update_last_seen();

                return player.clone();
            }
        }

        let player_id = self.next_player_id.fetch_add(1, Ordering::SeqCst);
        let player = Arc::new(RwLock::new(Player::new(
            player_id,
            username.to_string(),
            addr,
            lobby_name.to_string(),
        )));

        self.players.insert(player_id, player.clone());
        self.addr_to_player.insert(addr, player_id);

        info!(
            "Player {} ({}) joined lobby {}",
            player_id, username, lobby_name
        );

        player
    }

    pub fn get_player_by_addr(&self, addr: &SocketAddr) -> Option<Arc<RwLock<Player>>> {
        self.addr_to_player
            .get(addr)
            .and_then(|id| self.players.get(id.value()).map(|p| p.clone()))
    }

    pub async fn get_player_by_id(&self, player_id: u32) -> Option<Arc<RwLock<Player>>> {
        self.players.get(&player_id).map(|p| p.clone())
    }

    pub async fn update_player_last_seen(&self, addr: &SocketAddr) {
        if let Some(player) = self.get_player_by_addr(addr) {
            player.write().await.update_last_seen();
        }
    }

    pub async fn cleanup_timed_out_players(&self) {
        let timeout = self.config.server.client_timeout_seconds;
        let mut to_remove = Vec::new();

        for entry in self.players.iter() {
            let player = entry.value().read().await;

            if player.is_timed_out(timeout) {
                to_remove.push((player.id, player.address, player.lobby_name.clone()));
            }
        }

        for (player_id, addr, lobby_name) in to_remove {
            self.remove_player(player_id, addr, &lobby_name).await;
        }
    }

    pub async fn remove_player(&self, player_id: u32, addr: SocketAddr, lobby_name: &str) {
        if let Some(lobby) = self.get_lobby(lobby_name) {
            lobby.write().await.remove_player(player_id);
        }

        self.players.remove(&player_id);
        self.addr_to_player.remove(&addr);

        info!(
            "Player {} disconnected from lobby {}",
            player_id, lobby_name
        );
    }

    pub async fn get_lobby_players_except(
        &self,
        lobby_name: &str,
        except_id: u32,
    ) -> Vec<SocketAddr> {
        let mut addresses = Vec::new();

        for entry in self.players.iter() {
            let player = entry.value().read().await;

            if player.lobby_name == lobby_name && player.id != except_id {
                addresses.push(player.address);
            }
        }

        addresses
    }

    pub async fn get_all_lobby_players(&self, lobby_name: &str) -> Vec<SocketAddr> {
        let mut addresses = Vec::new();

        for entry in self.players.iter() {
            let player = entry.value().read().await;

            if player.lobby_name == lobby_name {
                addresses.push(player.address);
            }
        }

        addresses
    }

    pub async fn get_lobby_players(&self, lobby_name: &str) -> Vec<Arc<RwLock<Player>>> {
        let mut players = Vec::new();

        for entry in self.players.iter() {
            let player_arc = entry.value().clone();
            let player = player_arc.read().await;

            if player.lobby_name == lobby_name {
                drop(player);
                players.push(player_arc);
            }
        }

        players
    }

    pub async fn cleanup_idle_lobbies(&self) {
        let timeout = self.config.server.lobby_idle_timeout_seconds;
        let mut to_remove = Vec::new();

        for entry in self.lobbies.iter() {
            let lobby = entry.value().read().await;

            if lobby.player_count() == 0 && lobby.is_idle(timeout) {
                to_remove.push(lobby.name.clone());
            }
        }

        for name in to_remove {
            if self.config.server.enable_persistence {
                if let Some(lobby) = self.lobbies.get(&name) {
                    let lob = lobby.read().await;

                    if let Err(e) = self.save_lobby(&lob) {
                        warn!("Failed to save lobby {} to disk: {}", name, e);
                    }
                }
            }

            self.lobbies.remove(&name);
            info!("Lobby closed for inactivity: {}", name);
        }
    }

    fn save_lobby(&self, lobby: &Lobby) -> Result<()> {
        let path = format!("{}/{}.json", self.config.server.persistence_dir, lobby.name);
        let json = serde_json::to_string_pretty(lobby)?;

        std::fs::write(path, json)?;
        Ok(())
    }

    fn load_lobbies(lobbies: &DashMap<String, Arc<RwLock<Lobby>>>, dir: &str) -> Result<()> {
        if !std::path::Path::new(dir).exists() {
            return Ok(());
        }

        for entry in std::fs::read_dir(dir)? {
            let entry = entry?;
            let path = entry.path();

            if path.extension().and_then(|s| s.to_str()) == Some("json") {
                match std::fs::read_to_string(&path) {
                    Ok(json) => match serde_json::from_str::<Lobby>(&json) {
                        Ok(lobby) => {
                            let name = lobby.name.clone();
                            lobbies.insert(name.clone(), Arc::new(RwLock::new(lobby)));

                            info!("Loaded lobby: {}", name);
                        }
                        Err(e) => warn!("Failed to parse lobby file: {:?}: {}", path, e),
                    },
                    Err(e) => warn!("Failed to read lobby file: {:?}: {}", path, e),
                }
            }
        }

        Ok(())
    }

    pub async fn save_all_lobbies(&self) -> Result<()> {
        if !self.config.server.enable_persistence {
            return Ok(());
        }

        for entry in self.lobbies.iter() {
            let lobby = entry.value().read().await;

            if let Err(e) = self.save_lobby(&lobby) {
                warn!("Failed to save lobby {}: {}", lobby.name, e);
            }
        }

        Ok(())
    }
}
