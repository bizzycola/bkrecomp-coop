use anyhow::{anyhow, Result};

fn read_u32_be(data: &[u8], offset: usize) -> Result<u32> {
    if offset + 4 > data.len() {
        return Err(anyhow!("Not enough data to read u32"));
    }
    Ok(((data[offset] as u32) << 24)
        | ((data[offset + 1] as u32) << 16)
        | ((data[offset + 2] as u32) << 8)
        | (data[offset + 3] as u32))
}

fn read_i32_be(data: &[u8], offset: usize) -> Result<i32> {
    Ok(read_u32_be(data, offset)? as i32)
}

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

fn read_i16_le(data: &[u8], offset: usize) -> Result<i16> {
    if offset + 2 > data.len() {
        return Err(anyhow!("Not enough data to read i16"));
    }
    Ok(i16::from_le_bytes([data[offset], data[offset + 1]]))
}

pub fn write_u32_be(buf: &mut Vec<u8>, value: u32) {
    buf.push((value >> 24) as u8);
    buf.push((value >> 16) as u8);
    buf.push((value >> 8) as u8);
    buf.push(value as u8);
}

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
            return Err(anyhow!(
                "Invalid NotePacket: expected 13 bytes, got {}",
                data.len()
            ));
        }
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

fn write_f32_be(buf: &mut Vec<u8>, value: f32) {
    write_u32_be(buf, value.to_bits());
}

fn read_f32_be(data: &[u8], offset: usize) -> Result<f32> {
    Ok(f32::from_bits(read_u32_be(data, offset)?))
}

#[derive(Debug, Clone)]
pub struct PlayerInfoRequest {
    pub target_player_id: u32,
    pub requester_player_id: u32,
}

impl PlayerInfoRequest {
    pub fn deserialize(data: &[u8]) -> Result<Self> {
        if data.len() < 8 {
            return Err(anyhow!("Invalid PlayerInfoRequest: expected 8 bytes"));
        }
        Ok(PlayerInfoRequest {
            target_player_id: read_u32_be(data, 0)?,
            requester_player_id: read_u32_be(data, 4)?,
        })
    }

    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::new();
        write_u32_be(&mut buf, self.target_player_id);
        write_u32_be(&mut buf, self.requester_player_id);
        buf
    }
}

#[derive(Debug, Clone)]
pub struct PlayerInfoResponse {
    pub target_player_id: u32,
    pub map_id: i16,
    pub level_id: i16,
    pub x: f32,
    pub y: f32,
    pub z: f32,
    pub yaw: f32,
}

impl PlayerInfoResponse {
    pub fn deserialize(data: &[u8]) -> Result<Self> {
        if data.len() < 24 {
            return Err(anyhow!("Invalid PlayerInfoResponse: expected 24 bytes"));
        }
        let target_player_id = read_u32_be(data, 0)?;
        let map_id = i16::from_be_bytes([data[4], data[5]]);
        let level_id = i16::from_be_bytes([data[6], data[7]]);
        let x = read_f32_be(data, 8)?;
        let y = read_f32_be(data, 12)?;
        let z = read_f32_be(data, 16)?;
        let yaw = read_f32_be(data, 20)?;

        Ok(PlayerInfoResponse {
            target_player_id,
            map_id,
            level_id,
            x,
            y,
            z,
            yaw,
        })
    }

    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::new();
        write_u32_be(&mut buf, self.target_player_id);
        buf.extend_from_slice(&self.map_id.to_be_bytes());
        buf.extend_from_slice(&self.level_id.to_be_bytes());
        write_f32_be(&mut buf, self.x);
        write_f32_be(&mut buf, self.y);
        write_f32_be(&mut buf, self.z);
        write_f32_be(&mut buf, self.yaw);
        buf
    }
}

#[derive(Debug, Clone)]
pub struct PlayerListEntry {
    pub player_id: u32,
    pub username: String,
}

impl PlayerListEntry {
    pub fn serialize(&self) -> Vec<u8> {
        let mut buf = Vec::new();
        write_u32_be(&mut buf, self.player_id);
        write_u32_be(&mut buf, self.username.len() as u32);
        buf.extend_from_slice(self.username.as_bytes());
        buf
    }
}
