#pragma once

#include <queue>
#include <mutex>
#include <cstring>
#include <cstdint>

enum class MessageType : uint8_t
{
    NONE = 0,
    PLAYER_CONNECTED = 1,
    PLAYER_DISCONNECTED = 2,
    JIGGY_COLLECTED = 3,
    NOTE_COLLECTED = 4,
    PUPPET_UPDATE = 5,
    LEVEL_OPENED = 6,
    NOTE_SAVE_DATA = 7,
    CONNECTION_STATUS = 8,
    CONNECTION_ERROR = 9,
};

constexpr size_t MAX_MESSAGE_DATA_SIZE = 256;

struct GameMessage
{
    uint8_t type;
    int32_t playerId;
    int32_t param1;
    int32_t param2;
    int32_t param3;
    int32_t param4;
    float paramF1;
    float paramF2;
    float paramF3;
    uint16_t dataSize;
    uint8_t data[MAX_MESSAGE_DATA_SIZE];

    GameMessage() : type(0), playerId(-1),
                    param1(0), param2(0), param3(0), param4(0),
                    paramF1(0.0f), paramF2(0.0f), paramF3(0.0f),
                    dataSize(0)
    {
        memset(data, 0, MAX_MESSAGE_DATA_SIZE);
    }
};

class MessageQueue
{
private:
    std::queue<GameMessage> m_queue;
    mutable std::mutex m_mutex;
    static constexpr size_t MAX_QUEUE_SIZE = 1024;

public:
    MessageQueue() = default;
    ~MessageQueue() = default;

    bool Push(const GameMessage &msg)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.size() >= MAX_QUEUE_SIZE)
        {
            return false;
        }
        m_queue.push(msg);
        return true;
    }

    bool Pop(GameMessage &msg)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty())
        {
            return false;
        }
        msg = m_queue.front();
        m_queue.pop();
        return true;
    }

    bool HasMessages() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return !m_queue.empty();
    }

    size_t Size() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

    void Clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        while (!m_queue.empty())
        {
            m_queue.pop();
        }
    }
};

extern MessageQueue g_messageQueue;

inline GameMessage CreatePlayerConnectedMsg(int playerId, const char *username)
{
    GameMessage msg;
    msg.type = static_cast<uint8_t>(MessageType::PLAYER_CONNECTED);
    msg.playerId = playerId;

    if (username)
    {
        size_t len = strlen(username);
        if (len > MAX_MESSAGE_DATA_SIZE - 1)
        {
            len = MAX_MESSAGE_DATA_SIZE - 1;
        }
        memcpy(msg.data, username, len);
        msg.data[len] = '\0';
        msg.dataSize = static_cast<uint16_t>(len + 1);
    }

    return msg;
}

inline GameMessage CreatePlayerDisconnectedMsg(int playerId)
{
    GameMessage msg;
    msg.type = static_cast<uint8_t>(MessageType::PLAYER_DISCONNECTED);
    msg.playerId = playerId;
    return msg;
}

inline GameMessage CreateJiggyCollectedMsg(int playerId, int levelId, int jiggyId)
{
    GameMessage msg;
    msg.type = static_cast<uint8_t>(MessageType::JIGGY_COLLECTED);
    msg.playerId = playerId;
    msg.param1 = levelId;
    msg.param2 = jiggyId;
    return msg;
}

inline GameMessage CreateNoteCollectedMsg(int playerId, int mapId, int levelId, bool isDynamic, int noteIndex)
{
    GameMessage msg;
    msg.type = static_cast<uint8_t>(MessageType::NOTE_COLLECTED);
    msg.playerId = playerId;
    msg.param1 = mapId;
    msg.param2 = levelId;
    msg.param3 = isDynamic ? 1 : 0;
    msg.param4 = noteIndex;
    return msg;
}

inline GameMessage CreatePuppetUpdateMsg(int playerId, float x, float y, float z,
                                         float yaw, float pitch, float roll,
                                         int16_t mapId, int16_t levelId)
{
    GameMessage msg;
    msg.type = static_cast<uint8_t>(MessageType::PUPPET_UPDATE);
    msg.playerId = playerId;
    msg.paramF1 = x;
    msg.paramF2 = y;
    msg.paramF3 = z;
    msg.param1 = mapId;
    msg.param2 = levelId;

    float *rotData = reinterpret_cast<float *>(msg.data);
    rotData[0] = yaw;
    rotData[1] = pitch;
    rotData[2] = roll;
    msg.dataSize = sizeof(float) * 3;

    return msg;
}

inline GameMessage CreateLevelOpenedMsg(int playerId, int worldId, int jiggyCost)
{
    GameMessage msg;
    msg.type = static_cast<uint8_t>(MessageType::LEVEL_OPENED);
    msg.playerId = playerId;
    msg.param1 = worldId;
    msg.param2 = jiggyCost;
    return msg;
}

inline GameMessage CreateConnectionStatusMsg(const char *status_text)
{
    GameMessage msg;
    msg.type = static_cast<uint8_t>(MessageType::CONNECTION_STATUS);
    msg.playerId = -1;

    if (status_text)
    {
        size_t len = strlen(status_text);
        if (len > MAX_MESSAGE_DATA_SIZE - 1)
        {
            len = MAX_MESSAGE_DATA_SIZE - 1;
        }
        memcpy(msg.data, status_text, len);
        msg.data[len] = '\0';
        msg.dataSize = static_cast<uint16_t>(len + 1);
    }

    return msg;
}

inline GameMessage CreateConnectionErrorMsg(const char *error_text)
{
    GameMessage msg;
    msg.type = static_cast<uint8_t>(MessageType::CONNECTION_ERROR);
    msg.playerId = -1;

    if (error_text)
    {
        size_t len = strlen(error_text);
        if (len > MAX_MESSAGE_DATA_SIZE - 1)
        {
            len = MAX_MESSAGE_DATA_SIZE - 1;
        }
        memcpy(msg.data, error_text, len);
        msg.data[len] = '\0';
        msg.dataSize = static_cast<uint16_t>(len + 1);
    }

    return msg;
}
