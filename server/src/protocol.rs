#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PacketType {
    Handshake = 1,
    PlayerConnected = 3,
    PlayerDisconnected = 4,
    Ping = 5,
    Pong = 6,
    FullSyncRequest = 10,
    NoteSaveData = 11,
    InitialSaveDataRequest = 12,
    PuppetUpdate = 20,
    PuppetSyncRequest = 21,
    PlayerPosition = 50,
    JiggyCollected = 51,
    NoteCollected = 52,
    NoteCollectedPos = 53,
    LevelOpened = 54,
    Unknown = 255
}

impl From<u8> for PacketType {
    fn from(val: u8) -> Self {
        match val {
            1 => PacketType::Handshake,
            3 => PacketType::PlayerConnected,
            4 => PacketType::PlayerDisconnected,
            5 => PacketType::Ping,
            6 => PacketType::Pong,
            10 => PacketType::FullSyncRequest,
            11 => PacketType::NoteSaveData,
            12 => PacketType::InitialSaveDataRequest,
            20 => PacketType::PuppetUpdate,
            21 => PacketType::PuppetSyncRequest,
            50 => PacketType::PlayerPosition,
            51 => PacketType::JiggyCollected,
            52 => PacketType::NoteCollected,
            53 => PacketType::NoteCollectedPos,
            54 => PacketType::LevelOpened,
            _ => PacketType::Unknown
        }
    }
}

impl From<PacketType> for u8 {
    fn from(val: PacketType) -> Self {
        val as u8
    }
}
