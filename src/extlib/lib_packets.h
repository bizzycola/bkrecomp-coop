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
    PuppetUpdate = 20,
    PuppetSyncRequest = 21,
    PlayerPosition = 50,
    JiggyCollected = 51,
    NoteCollected = 52,
    NoteCollectedPos = 53,
    LevelOpened = 54,
};

struct JiggyPacket
{
    int LevelId;
    int JiggyId;

    MSGPACK_DEFINE(LevelId, JiggyId);
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

#pragma pack(push, 1)
struct PuppetUpdatePacket
{
    float x, y, z;
    float yaw, pitch, roll;
    int16_t map_id;
    int16_t level_id;
    int16_t anim_id;
    uint8_t model_id;
    uint8_t flags;

    MSGPACK_DEFINE(x, y, z, yaw, pitch, roll, map_id, level_id, anim_id, model_id, flags);
};
#pragma pack(pop)

// Broadcast packets (received from server)
struct BroadcastJiggy
{
    int level_id;
    int jiggy_id;
    std::string collector;

    MSGPACK_DEFINE(level_id, jiggy_id, collector);
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