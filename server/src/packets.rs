use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LoginPacket {
    #[serde(rename = "LobbyName")]
    pub lobby_name: String,

    #[serde(rename = "Password")]
    pub password: String,

    #[serde(rename = "Username")]
    pub username: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct JiggyPacket {
    #[serde(rename = "LevelId")]
    pub level_id: i32,

    #[serde(rename = "JiggyId")]
    pub jiggy_id: i32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NotePacket {
    #[serde(rename = "MapId")]
    pub map_id: i32,

    #[serde(rename = "LevelId")]
    pub level_id: i32,

    #[serde(rename = "IsDynamic")]
    pub is_dynamic: bool,

    #[serde(rename = "NoteIndex")]
    pub note_index: i32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NotePacketPos {
    #[serde(rename = "MapId")]
    pub map_id: i32,

    #[serde(rename = "X")]
    pub x: i16,

    #[serde(rename = "Y")]
    pub y: i16,

    #[serde(rename = "Z")]
    pub z: i16
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LevelOpenedPacket {
    #[serde(rename = "WorldId")]
    pub world_id: i32,

    #[serde(rename = "JiggyCost")]
    pub jiggy_cost: i32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BroadcastJiggy {
    pub level_id: i32,
    pub jiggy_id: i32,
    pub collector: String
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BroadcastNote {
    pub map_id: i32,
    pub level_id: i32,
    pub is_dynamic: bool,
    pub note_index: i32,
    pub collector: String
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BroadcastNotePos {
    pub map_id: i32,
    pub x: i16,
    pub y: i16,
    pub z: i16,
    pub collector: String
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NoteSaveDataPacket {
    pub level_index: i32,
    pub save_data: Vec<u8>
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PuppetUpdatePacket {
    pub x: f32,
    pub y: f32,
    pub z: f32,
    pub yaw: f32,
    pub pitch: f32,
    pub roll: f32,
    pub map_id: i16,
    pub level_id: i16,
    pub anim_id: i16,
    pub model_id: u8,
    pub flags: u8,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlayerConnectedBroadcast {
    pub username: String,
    pub player_id: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlayerDisconnectedBroadcast {
    pub username: String,
    pub player_id: u32,
}
