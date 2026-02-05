#ifndef LIB_PACKETS_HPP
#define LIB_PACKETS_HPP

#include <string>
#include <cstdint>
#include <vector>

enum class PacketType : uint8_t
{
    Handshake = 1,
    PlayerConnected = 3,
    PlayerDisconnected = 4,
    Ping = 5,
    Pong = 6,
    FullSyncRequest = 10,
    NoteSaveData = 11,
    InitialSaveDataRequest = 12,

    FileProgressFlags = 13,

    AbilityProgress = 14,
    HoneycombScore = 15,
    MumboScore = 16,
    HoneycombCollected = 17,
    MumboTokenCollected = 18,
    PuppetUpdate = 20,
    PuppetSyncRequest = 21,
    PlayerPosition = 50,
    JiggyCollected = 51,
    NoteCollected = 52,
    NoteCollectedPos = 53,
    LevelOpened = 54,

    PlayerInfoRequest = 55,
    PlayerInfoResponse = 56,
    PlayerListUpdate = 57,

    ReliableAck = 60,
};

struct FileProgressFlagsPacket
{
    std::vector<uint8_t> Flags;
};

struct AbilityProgressPacket
{
    std::vector<uint8_t> Moves;
};

struct HoneycombScorePacket
{
    std::vector<uint8_t> Flags;
};

struct MumboScorePacket
{
    std::vector<uint8_t> Flags;
};

struct HoneycombCollectedPacket
{
    int MapId;
    int HoneycombId;
    int X;
    int Y;
    int Z;
};

struct MumboTokenCollectedPacket
{
    int MapId;
    int TokenId;
    int X;
    int Y;
    int Z;
};

struct JiggyPacket
{
    int JiggyEnumId;
    int CollectedValue;
};

struct NotePacket
{
    int MapId;
    int LevelId;
    bool IsDynamic;
    int NoteIndex;
};

struct NotePacketPos
{
    int MapId;
    int16_t X;
    int16_t Y;
    int16_t Z;
};

struct NoteSaveDataPacket
{
    int LevelIndex;
    std::vector<uint8_t> SaveData;
};

struct LevelOpenedPacket
{
    int WorldId;
    int JiggyCost;
};

struct LoginPacket
{
    std::string LobbyName;
    std::string Password;
    std::string Username;
};

#pragma pack(push, 1)
struct PositionPacketRaw
{
    float x, y, z;
    float rot;
    int currentLevel;
    int characterId;
};
#pragma pack(pop)

struct PuppetUpdatePacket
{
    float x, y, z;
    float yaw, pitch, roll;
    float anim_duration;
    float anim_timer;
    int16_t map_id;
    int16_t level_id;
    int16_t anim_id;
    uint8_t model_id;
    uint8_t flags;
    uint8_t playback_type;
    uint8_t playback_direction;
};

struct BroadcastJiggy
{
    int jiggy_enum_id;
    int collected_value;
    std::string collector;
};

struct BroadcastNote
{
    int map_id;
    int level_id;
    bool is_dynamic;
    int note_index;
    std::string collector;
};

struct BroadcastNotePos
{
    int map_id;
    int16_t x;
    int16_t y;
    int16_t z;
    std::string collector;
};

struct PlayerConnectedBroadcast
{
    std::string username;
    uint32_t player_id;
};

struct PlayerDisconnectedBroadcast
{
    std::string username;
    uint32_t player_id;
};

struct PlayerInfoRequestPacket
{
    uint32_t target_player_id;
    uint32_t requester_player_id;
};

struct PlayerInfoResponsePacket
{
    uint32_t target_player_id;
    int16_t map_id;
    int16_t level_id;
    float x;
    float y;
    float z;
    float yaw;
};

struct PlayerListEntryPacket
{
    uint32_t player_id;
    std::string username;
};

#endif
