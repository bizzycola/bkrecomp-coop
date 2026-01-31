mod config;
mod lobby;
mod network;
mod packets;
mod player;
mod protocol;
mod state;

use anyhow::Result;
use std::sync::Arc;
use tokio::net::UdpSocket;
use tracing::{info, error};

use crate::config::Config;
use crate::network::NetworkServer;

#[tokio::main]
async fn main() -> Result<()> {
    tracing_subscriber::fmt()
        .with_env_filter("info")
        .with_target(false)
        .init();

    info!("== Banjo-Kazooie Recomp Co-op Mod Server (long name edition) ==");
    info!("Starting..");

    let config = Config::load("config.toml").unwrap_or_else(|e| {
        error!("Failed to login config. Using defaults. Error: {}", e);
        Config::default()
    });

    info!("Config values:");
    info!("  Port: {}", config.server.port);
    info!("  Max lobbies: {}", config.server.max_lobbies);
    info!("  Max players per lobby: {}", config.server.max_players_per_lobby);
    info!("  Client timeout: {}s", config.server.client_timeout_seconds);
    info!("  Persistence: {}", config.server.enable_persistence);

    if config.server.enable_persistence {
        std::fs::create_dir_all(&config.server.persistence_dir)?;
        info!("  Lobby save dir: {}", config.server.persistence_dir);
    }

    let addr = format!("0.0.0.0:{}", config.server.port);
    let socket = UdpSocket::bind(&addr).await?;
    info!("Server now listening on {}", addr);

    let server = NetworkServer::new(Arc::new(socket), config);
    server.run().await?;

    Ok(())
}