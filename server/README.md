# BK Recomp Co-op Server

Server for the BK Co-Op Mod.

### Building

```bash
cd server
cargo build --release
```

### Running

```bash
# Run with Rust directly
cargo run --release

# Run the compiled binary
./target/release/bkrecomp-coop-server
```

```toml
[server]
port = 8756                          # port to listen on
max_lobbies = 100                    # max number of lobbies that can be open at once
max_players_per_lobby = 16           # max players per lobby
client_timeout_seconds = 60          # disconnect timeout if no packets are received 
lobby_idle_timeout_seconds = 300     # how long to wait before deleting empty lobbies
enable_persistence = true            # whether to persist lobbies (in case of server restarts)
persistence_dir = "./lobbies"        # where to persist lobbies (saved as JSON files)

[logging]
level = "info"                       # trace, debug, info, warn, error
log_to_file = true                   # enable file log
log_file = "server.log"              # which file to save logs to
```

### Systemd UNIT file

Create `/etc/systemd/system/bkrecomp-server.service`:

```ini
[Unit]
Description=Banjo-Kazooie Recomp Co-op Server
After=network.target

[Service]
Type=simple
User=bkrecomp
WorkingDirectory=/opt/bkrecomp-server
ExecStart=/opt/bkrecomp-server/bkrecomp-coop-server
Restart=always

[Install]
WantedBy=multi-user.target
```
