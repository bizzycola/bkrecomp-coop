use anyhow::Result;
use serde::{Deserialize, Serialize};
use std::fs;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Config {
    pub server: ServerConfig,
    pub logging: LoggingConfig,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ServerConfig {
    pub port: u16,
    pub max_lobbies: usize,
    pub max_players_per_lobby: usize,
    pub client_timeout_seconds: u64,
    pub lobby_idle_timeout_seconds: u64,
    pub enable_persistence: bool,
    pub persistence_dir: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LoggingConfig {
    pub level: String,
    pub log_to_file: bool,
    pub log_file: String,
}

impl Default for Config {
    fn default() -> Self {
        Self {
            server: ServerConfig {
                port: 8756,
                max_lobbies: 100,
                max_players_per_lobby: 16,
                client_timeout_seconds: 60,
                lobby_idle_timeout_seconds: 300,
                enable_persistence: true,
                persistence_dir: "./lobbies".to_string(),
            },
            logging: LoggingConfig {
                level:"info".to_string(),
                log_to_file: true,
                log_file: "server.log".to_string(),
            },
        }
    }
}

/**
 * Manage server config.
 * Now with 50% more loading AND saving!
 */
impl Config {
    /**
     * Load config from a TOML file
     */
    pub fn load(path: &str) -> Result<Self> {
        let content = fs::read_to_string(path)?;
        let config: Config = toml::from_str(&content)?;

        Ok(config)
    }

    /**
     * Save config values to a TOML file
     */
    pub fn save(&self, path: &str) -> Result<()> {
        let content = toml::to_string_pretty(self)?;
        fs::write(path, content);

        Ok(())
    }
}
