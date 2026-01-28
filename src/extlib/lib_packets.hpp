#ifndef LIB_PACKETS_HPP
#define LIB_PACKETS_HPP

#include <string>
#include <cstdint>

#ifndef MSGPACK_NO_BOOST
#define MSGPACK_NO_BOOST
#endif
#include <msgpack.hpp>

enum class PacketType : uint8_t {
    Handshake = 1,
    PlayerConnected = 3,
    PlayerDisconnected = 4,
    Ping = 5,
    Pong = 6,
    PlayerPosition = 50,
    JiggyCollected = 20,
    FullSyncRequest = 10,
};

struct JiggyPacket {
    int LevelId;
    int JiggyId;

    MSGPACK_DEFINE(LevelId, JiggyId); 
};

struct LoginPacket {
    std::string LobbyName;
    std::string Password;
    std::string Username;

    MSGPACK_DEFINE(LobbyName, Password, Username);
};

#pragma pack(push, 1)
struct PositionPacketRaw {
    float x, y, z;
    float rot;
    int currentLevel;
    int characterId;
};
#pragma pack(pop)

#endif