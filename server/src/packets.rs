use anyhow::{anyhow, Result};

// Helper function to read big-endian u32
fn read_u32_be(data: &[u8], offset: usize) -> Result<u32> {
    if offset + 4 > data.len() {
        return Err(anyhow!("Not enough data to read u32"));
    }
    Ok(((data[offset] as u32) << 24)
        | ((data[offset + 1] as u32) << 16)
        | ((data[offset + 2] as u32) << 8)
        | (data[offset + 3] as u32))
}

// Helper function to read big-endian i32
fn read_i32_be(data: &[u8], offset: usize) -> Result<i32> {
    Ok(read_u32_be(data, offset)? as i32)
}

// Helper function to read little-endian i32
fn read_i32_le(data: &[u8], offset: usize) -> Result<i32> {
    if offset + 4 > data.len() {
        return Err(anyhow!("Not enough data to read i32"));
    }
    Ok(i32::from_le_bytes([
        data[offset],
        data[offset + 1],
        data[offset + 2],
        data[offset + 3],
    ]))
}

// Helper function to read little-endian i16
fn read_i16_le(data: &[u8], offset: usize) -> Result<i16> {
    if offset + 2 > data.len() {
        return Err(anyhow!("Not enough data to read i16"));
    }
    Ok(i16::from_le_bytes([data[offset], data[offset + 1]]))
}

// Helper function to write big-endian u32
fn write_u32_be(buf: &mut Vec<u8>, value: u32) {
    buf.push((value >> 24) as u8);
    buf.push((value >> 16) as u8);
    buf.push((value >> 8) as u8);
    buf.push(value as u8);
}

// Helper function to write big-endian i32
fn write_i32_be(buf: &mut Vec<u8>, value: i32) {
    write_u32_be(buf, value as u32);
}

#[derive(Debug, Clone)]
pub struct LoginPacket {
    pub lobby_name: String,
    pub password: String,
    pub username: String,
}

impl LoginPacket {
    pub fn deserialize(data: &[u8]) -> Result<Self> {
        let mut offset = 0;

        // Read lobby_name
        if offset + 4 > data.len() {
            return Err(anyhow!(
                "Invalid LoginPacket: not enough data for lobby_name length"
            ));
        }
        let lobby_len = read_u32_be(data, offset)? as usize;
        offset += 4;

        if offset + lobby_len > data.len() {
            return Err(anyhow!(
                "Invalid LoginPacket: not enough data for lobby_name"
            ));
        }
        let lobby_name = String::from_utf8(data[offset..offset + lobby_len].to_vec())?;
        offset += lobby_len;

        // Read password
        if offset + 4 > data.len() {
            return Err(anyhow!(
                "Invalid LoginPacket: not enough data for password length"
            ));
        }
        let pass_len = read_u32_be(data, offset)? as usize;
        offset += 4;

        if offset + pass_len > data.len() {
            return Err(anyhow!("Invalid LoginPacket: not enough data for password"));
        }
        let password = String::from_utf8(data[offset..offset + pass_len].to_vec())?;
        offset += pass_len;

        // Read username
        if offset + 4 > data.len() {
            return Err(anyhow!(
                "Invalid LoginPacket: not enough data for username length"
            ));
        }
        let user_len = read_u32_be(data, offset)? as usize;
        offset += 4;

        if offset + user_len > data.len() {
            return Err(anyhow!("Invalid LoginPacket: not enough data for username"));
        }
        let username = String::from_utf8(data[offset..offset + user_len].to_vec())?;

        Ok(LoginPacket {
            lobby_name,
            password,
            username,
        })
    }
}

#[derive(Debug, Clone)]
pub struct JiggyPacket {
    pub jiggy_enum_id: i32,
    pub collected_value: i32,
}

impl JiggyPacket {
    pub fn deserialize(data: &[u8]) -> Result<Self> {
        if data.len() < 8 {
            return Err(anyhow!("Invalid JiggyPacket: expected 8 bytes"));
        }
        // Client uses std::memcpy (little-endian)
        Ok(JiggyPacket {
            jiggy_enum_id: read_i32_le(data, 0)?,
            collected_value: read_i32_le(data, 4)?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct NotePacket {
    pub map_id: i32,
    pub level_id: i32,
    pub is_dynamic: bool,
    pub note_index: i32,
}

impl NotePacket {
    pub fn deserialize(data: &[u8]) -> Result<Self> {
        if data.len() < 13 {
            return Err(anyhow!("Invalid NotePacket: expected 13 bytes, got {}", data.len()));
        }
        // Client sends: mapId(4 LE) + levelId(4 LE) + isDynamic(1) + noteIndex(4 LE)
        Ok(NotePacket {
            map_id: i32::from_le_bytes([data[0], data[1], data[2], data[3]]),
            level_id: i32::from_le_bytes([data[4], data[5], data[6], data[7]]),
            is_dynamic: data[8] != 0,
            note_index: i32::from_le_bytes([data[9], data[10], data[11], data[12]]),
        })
    }
}

#[derive(Debug, Clone)]
pub struct NotePacketPos {
    pub map_id: i32,
    pub x: i32,
    pub y: i32,
    pub z: i32,
}

impl NotePacketPos {
    pub fn deserialize(data: &[u8]) -> Result<Self> {
        if data.len() < 10 {
            return Err(anyhow!("Invalid NotePacketPos: expected 10 bytes"));
        }
        // Client sends: mapId(4 LE) + x(2 LE i16) + y(2 LE i16) + z(2 LE i16)
        Ok(NotePacketPos {
            map_id: read_i32_le(data, 0)?,
            x: read_i16_le(data, 4)? as i32,
            y: read_i16_le(data, 6)? as i32,
            z: read_i16_le(data, 8)? as i32,
        })
    }
}

#[derive(Debug, Clone)]
pub struct LevelOpenedPacket {
    pub world_id: i32,
    pub jiggy_cost: i32,
}

impl LevelOpenedPacket {
    pub fn deserialize(data: &[u8]) -> Result<Self> {
        if data.len() < 8 {
            return Err(anyhow!("Invalid LevelOpenedPacket: expected 8 bytes"));
        }
        // Client uses std::memcpy (little-endian)
        Ok(LevelOpenedPacket {
            world_id: read_i32_le(data, 0)?,
            jiggy_cost: read_i32_le(data, 4)?,
        })
    }

    pub fn serialize(&self) -> Vec<u8> {
        let mut payload = Vec::new();
        payload.extend_from_slice(&self.world_id.to_be_bytes());
        payload.extend_from_slice(&self.jiggy_cost.to_be_bytes());
        payload
    }
}

#[derive(Debug, Clone)]
pub struct FileProgressFlagsPacket {
    pub flags: Vec<u8>,
}

impl FileProgressFlagsPacket {
    pub fn deserialize(data: &[u8]) -> Result<Self> {
        Ok(FileProgressFlagsPacket {
            flags: data.to_vec(),
        })
    }

    pub fn serialize(&self) -> Vec<u8> {
        self.flags.clone()
    }
}

#[derive(Debug, Clone)]
pub struct AbilityProgressPacket {
    pub bytes: Vec<u8>,
}

impl AbilityProgressPacket {
    pub fn deserialize(data: &[u8]) -> Result<Self> {
        Ok(AbilityProgressPacket {
            bytes: data.to_vec(),
        })
    }

    pub fn serialize(&self) -> Vec<u8> {
        self.bytes.clone()
    }
}

#[derive(Debug, Clone)]
pub struct HoneycombScorePacket {
    pub bytes: Vec<u8>,
}

impl HoneycombScorePacket {
    pub fn deserialize(data: &[u8]) -> Result<Self> {
        Ok(HoneycombScorePacket {
            bytes: data.to_vec(),
        })
    }

    pub fn serialize(&self) -> Vec<u8> {
        self.bytes.clone()
    }
}

#[derive(Debug, Clone)]
pub struct MumboScorePacket {
    pub bytes: Vec<u8>,
}

impl MumboScorePacket {
    pub fn deserialize(data: &[u8]) -> Result<Self> {
        Ok(MumboScorePacket {
            bytes: data.to_vec(),
        })
    }

    pub fn serialize(&self) -> Vec<u8> {
        self.bytes.clone()
    }
}

#[derive(Debug, Clone)]
pub struct HoneycombCollectedPacket {
    pub map_id: i32,
    pub honeycomb_id: i32,
    pub x: i32,
    pub y: i32,
    pub z: i32,
}

impl HoneycombCollectedPacket {
    pub fn deserialize(data: &[u8]) -> Result<Self> {
        if data.len() < 20 {
            return Err(anyhow!(
                "Invalid HoneycombCollectedPacket: expected 20 bytes"
            ));
        }
        // Client uses std::memcpy (little-endian)
        Ok(HoneycombCollectedPacket {
            map_id: read_i32_le(data, 0)?,
            honeycomb_id: read_i32_le(data, 4)?,
            x: read_i32_le(data, 8)?,
            y: read_i32_le(data, 12)?,
            z: read_i32_le(data, 16)?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct MumboTokenCollectedPacket {
    pub map_id: i32,
    pub token_id: i32,
    pub x: i32,
    pub y: i32,
    pub z: i32,
}

impl MumboTokenCollectedPacket {
    pub fn deserialize(data: &[u8]) -> Result<Self> {
        if data.len() < 20 {
            return Err(anyhow!(
                "Invalid MumboTokenCollectedPacket: expected 20 bytes"
            ));
        }
        // Client uses std::memcpy (little-endian)
        Ok(MumboTokenCollectedPacket {
            map_id: read_i32_le(data, 0)?,
            token_id: read_i32_le(data, 4)?,
            x: read_i32_le(data, 8)?,
            y: read_i32_le(data, 12)?,
            z: read_i32_le(data, 16)?,
        })
    }
}

#[derive(Debug, Clone)]
pub struct NoteSaveDataPacket {
    pub level_index: i32,
    pub save_data: Vec<u8>,
}

impl NoteSaveDataPacket {
    pub fn deserialize(data: &[u8]) -> Result<Self> {
        if data.len() < 4 {
            return Err(anyhow!(
                "Invalid NoteSaveDataPacket: expected at least 4 bytes"
            ));
        }
        // Client uses std::memcpy (little-endian)
        Ok(NoteSaveDataPacket {
            level_index: read_i32_le(data, 0)?,
            save_data: data[4..].to_vec(),
        })
    }

    pub fn serialize(&self) -> Vec<u8> {
        let mut payload = Vec::new();
        payload.extend_from_slice(&self.level_index.to_be_bytes());
        payload.extend_from_slice(&self.save_data);
        payload
    }
}

// Broadcast structures for sending
#[derive(Debug, Clone)]
pub struct BroadcastJiggy {
    pub jiggy_enum_id: i32,
    pub collected_value: i32,
    pub collector: String,
}

impl BroadcastJiggy {
    pub fn serialize(&self, player_id: u32) -> Vec<u8> {
        let mut buf = Vec::new();
        write_u32_be(&mut buf, player_id);
        write_i32_be(&mut buf, self.jiggy_enum_id);
        write_i32_be(&mut buf, self.collected_value);
        buf
    }
}

#[derive(Debug, Clone)]
pub struct BroadcastNote {
    pub map_id: i32,
    pub level_id: i32,
    pub is_dynamic: bool,
    pub note_index: i32,
    pub collector: String,
}

impl BroadcastNote {
    pub fn serialize(&self, player_id: u32) -> Vec<u8> {
        let mut buf = Vec::new();
        write_u32_be(&mut buf, player_id);
        write_i32_be(&mut buf, self.map_id);
        write_i32_be(&mut buf, self.level_id);
        write_i32_be(&mut buf, if self.is_dynamic { 1 } else { 0 });
        write_i32_be(&mut buf, self.note_index);
        buf
    }
}

#[derive(Debug, Clone)]
pub struct BroadcastNotePos {
    pub map_id: i32,
    pub x: i32,
    pub y: i32,
    pub z: i32,
    pub collector: String,
}

impl BroadcastNotePos {
    pub fn serialize(&self, player_id: u32) -> Vec<u8> {
        let mut buf = Vec::new();
        write_u32_be(&mut buf, player_id);
        write_i32_be(&mut buf, self.map_id);
        write_i32_be(&mut buf, self.x);
        write_i32_be(&mut buf, self.y);
        write_i32_be(&mut buf, self.z);
        buf
    }
}

#[derive(Debug, Clone)]
pub struct BroadcastLevelOpened {
    pub world_id: i32,
    pub jiggy_cost: i32,
}

impl BroadcastLevelOpened {
    pub fn serialize(&self, player_id: u32) -> Vec<u8> {
        let mut buf = Vec::new();
        write_u32_be(&mut buf, player_id);
        write_i32_be(&mut buf, self.world_id);
        write_i32_be(&mut buf, self.jiggy_cost);
        buf
    }
}

#[derive(Debug, Clone)]
pub struct PlayerConnectedBroadcast {
    pub username: String,
    pub player_id: u32,
}

impl PlayerConnectedBroadcast {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::new();
        write_u32_be(&mut buf, self.player_id);
        write_u32_be(&mut buf, self.username.len() as u32);
        buf.extend_from_slice(self.username.as_bytes());
        buf
    }
}

#[derive(Debug, Clone)]
pub struct PlayerDisconnectedBroadcast {
    pub username: String,
    pub player_id: u32,
}

impl PlayerDisconnectedBroadcast {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::new();
        write_u32_be(&mut buf, self.player_id);
        write_u32_be(&mut buf, self.username.len() as u32);
        buf.extend_from_slice(self.username.as_bytes());
        buf
    }
}
