#ifndef LIB_PACKETS_HPP
#define LIB_PACKETS_HPP

#include <string>
#include <cstdint>

#ifndef MSGPACK_NO_BOOST
#define MSGPACK_NO_BOOST
#endif
#include <msgpack.hpp>

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

    // Save-derived blobs (reliable, low-rate).
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

    ReliableAck = 60,
};

struct FileProgressFlagsPacket
{
    // Raw bytes from fileProgressFlag_getSizeAndPtr (BK decomp). Expected size: 0x25.
    std::vector<uint8_t> Flags;

    MSGPACK_DEFINE(Flags);
};

struct AbilityProgressPacket
{
    // Raw bytes from ability_getSizeAndPtr (expected size: 8).
    std::vector<uint8_t> Moves;

    MSGPACK_DEFINE(Moves);
};

struct HoneycombScorePacket
{
    // Raw bytes from honeycombscore_getSizeAndPtr (expected size: 0x03).
    std::vector<uint8_t> Flags;

    MSGPACK_DEFINE(Flags);
};

struct MumboScorePacket
{
    // Raw bytes from mumboscore_getSizeAndPtr (expected size: 0x10).
    std::vector<uint8_t> Flags;

    MSGPACK_DEFINE(Flags);
};
struct HoneycombCollectedPacket
{
    int MapId;
    int HoneycombId;
    int X;
    int Y;
    int Z;

    MSGPACK_DEFINE(MapId, HoneycombId, X, Y, Z);
};

struct MumboTokenCollectedPacket
{
    int MapId;
    int TokenId;
    int X;
    int Y;
    int Z;

    MSGPACK_DEFINE(MapId, TokenId, X, Y, Z);
};

struct JiggyPacket
{
    int JiggyEnumId;
    int CollectedValue;

    MSGPACK_DEFINE(JiggyEnumId, CollectedValue);
};

struct NotePacket
{
    int MapId;
    int LevelId;
    bool IsDynamic;
    int NoteIndex;

    MSGPACK_DEFINE(MapId, LevelId, IsDynamic, NoteIndex);
};

struct NotePacketPos
{
    int MapId;
    int16_t X;
    int16_t Y;
    int16_t Z;

    MSGPACK_DEFINE(MapId, X, Y, Z);
};

struct NoteSaveDataPacket
{
    int LevelIndex;
    std::vector<uint8_t> SaveData;

    MSGPACK_DEFINE(LevelIndex, SaveData);
};

struct LevelOpenedPacket
{
    int WorldId;
    int JiggyCost;

    MSGPACK_DEFINE(WorldId, JiggyCost);
};

struct LoginPacket
{
    std::string LobbyName;
    std::string Password;
    std::string Username;

    MSGPACK_DEFINE(LobbyName, Password, Username);
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

    MSGPACK_DEFINE(x, y, z, yaw, pitch, roll, anim_duration, anim_timer, map_id, level_id, anim_id, model_id, flags, playback_type, playback_direction);
};

struct BroadcastJiggy
{
    int jiggy_enum_id;
    int collected_value;
    std::string collector;

    MSGPACK_DEFINE(jiggy_enum_id, collected_value, collector);
};

struct BroadcastNote
{
    int map_id;
    int level_id;
    bool is_dynamic;
    int note_index;
    std::string collector;

    MSGPACK_DEFINE(map_id, level_id, is_dynamic, note_index, collector);
};

struct BroadcastNotePos
{
    int map_id;
    int16_t x;
    int16_t y;
    int16_t z;
    std::string collector;

    MSGPACK_DEFINE(map_id, x, y, z, collector);
};

struct PlayerConnectedBroadcast
{
    std::string username;
    uint32_t player_id;

    MSGPACK_DEFINE(username, player_id);
};

struct PlayerDisconnectedBroadcast
{
    std::string username;
    uint32_t player_id;

    MSGPACK_DEFINE(username, player_id);
};

#endif
