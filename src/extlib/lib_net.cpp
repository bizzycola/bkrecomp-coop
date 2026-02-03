#include "lib_net.h"
#include "debug_log.h"
#include <iostream>
#include <chrono>
#include <sstream>

extern void coop_dll_log(const char *msg);

const uint32_t HANDSHAKE_INTERVAL_MS = 1000;
const uint32_t PING_INTERVAL_MS = 10000;

NetworkClient::NetworkClient()
    : m_udpSocket(INVALID_SOCKET), m_isConnected(false), m_needsInit(false),
      m_lastHandshakeTime(0), m_lastPingTime(0), m_lastPacketSentTime(0)
{
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0)
    {
        // WSAStartup failed
    }
#endif
}

NetworkClient::~NetworkClient()
{
    if (m_udpSocket != INVALID_SOCKET)
    {
#ifdef _WIN32
        closesocket(m_udpSocket);
        WSACleanup();
#else
        close(m_udpSocket);
#endif
    }
}

void NetworkClient::Configure(const std::string &host, const std::string &user, const std::string &lobby, const std::string &pass)
{
    m_host = host;
    m_user = user;
    m_lobby = lobby;
    m_pass = pass;
    m_needsInit = true;

    // Reset state
    m_isConnected = false;
    m_lastHandshakeTime = 0;
}

uint32_t NetworkClient::GetClockMS()
{
    using namespace std::chrono;
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

bool NetworkClient::PerformLazyInit()
{
    if (m_udpSocket != INVALID_SOCKET)
    {
#ifdef _WIN32
        closesocket(m_udpSocket);
#else
        close(m_udpSocket);
#endif
    }

    m_udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_udpSocket == INVALID_SOCKET)
    {
        return false;
    }

#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(m_udpSocket, FIONBIO, &mode);
#else
    int flags = fcntl(m_udpSocket, F_GETFL, 0);
    fcntl(m_udpSocket, F_SETFL, flags | O_NONBLOCK);
#endif

    memset(&m_serverAddr, 0, sizeof(m_serverAddr));
    m_serverAddr.sin_family = AF_INET;
    m_serverAddr.sin_port = htons(8756);

    if (inet_pton(AF_INET, m_host.c_str(), &m_serverAddr.sin_addr) <= 0)
    {
        return false;
    }

    return true;
}

void NetworkClient::SendRawPacket(PacketType type, const void *data, size_t size)
{
    if (m_udpSocket == INVALID_SOCKET)
    {
        return;
    }

    std::vector<uint8_t> buffer(1 + size);
    buffer[0] = static_cast<uint8_t>(type);

    if (size > 0 && data != nullptr)
    {
        std::memcpy(&buffer[1], data, size);
    }

    int sent = sendto(m_udpSocket, (const char *)buffer.data(), (int)buffer.size(), 0,
                      (struct sockaddr *)&m_serverAddr, sizeof(m_serverAddr));

    if (sent >= 0)
    {
        m_lastPacketSentTime = GetClockMS();
    }
}

void NetworkClient::SendPing()
{
    SendRawPacket(PacketType::Ping, nullptr, 0);
}

void NetworkClient::Update()
{
    if (m_needsInit)
    {
        if (PerformLazyInit())
        {
            m_needsInit = false;
        }
        else
        {
            return;
        }
    }

    if (m_udpSocket == INVALID_SOCKET)
    {
        return;
    }

    uint32_t now = GetClockMS();

    if (!m_isConnected && (now - m_lastHandshakeTime > HANDSHAKE_INTERVAL_MS))
    {
        LoginPacket login;
        login.Username = m_user;
        login.LobbyName = m_lobby;
        login.Password = m_pass;
        msgpack::sbuffer sbuf;
        msgpack::pack(sbuf, login);

        SendRawPacket(PacketType::Handshake, sbuf.data(), sbuf.size());
        m_lastHandshakeTime = now;
    }

    if (m_isConnected)
    {
        if (now - m_lastPacketSentTime > PING_INTERVAL_MS)
        {
            SendPing();
            m_lastPingTime = now;
        }
    }

    struct sockaddr_in from;
    int fromLen = sizeof(from);
    uint8_t buf[2048];

    while (true)
    {
        int len = recvfrom(m_udpSocket, (char *)buf, sizeof(buf), 0, (struct sockaddr *)&from, &fromLen);

        if (len > 0)
        {
            uint8_t type = buf[0];
            uint8_t *payload = &buf[1];
            int payload_len = len - 1;

            if (!m_isConnected)
            {
                m_isConnected = true;
                m_lastPacketSentTime = now;
                RequestFullSync();
            }

            switch (static_cast<PacketType>(type))
            {
            case PacketType::PlayerConnected:
                HandlePlayerConnected(payload, payload_len);
                break;
            case PacketType::PlayerDisconnected:
                HandlePlayerDisconnected(payload, payload_len);
                break;
            case PacketType::JiggyCollected:
                HandleJiggyCollected(payload, payload_len);
                break;
            case PacketType::NoteCollected:
                HandleNoteCollected(payload, payload_len);
                break;
            case PacketType::NoteCollectedPos:
                HandleNoteCollectedPos(payload, payload_len);
                break;
            case PacketType::NoteSaveData:
                HandleNoteSaveData(payload, payload_len);
                break;
            case PacketType::PuppetUpdate:
                HandlePuppetUpdate(payload, payload_len);
                break;
            case PacketType::LevelOpened:
                HandleLevelOpened(payload, payload_len);
                break;
            case PacketType::Pong:
                break;
            case PacketType::InitialSaveDataRequest:
                // TODO: implement handler
                break;
            case PacketType::FileProgressFlags:
                // TODO: implement handler
                break;
            case PacketType::AbilityProgress:
                // TODO: implement handler
                break;
            case PacketType::HoneycombScore:
                // TODO: implement handler
                break;
            case PacketType::MumboScore:
                // TODO: implement handler
                break;
            default:
                break;
            }
        }
        else
        {
            break;
        }
    }
}

void NetworkClient::EnqueueEvent(PacketType type, const std::string &text, const std::vector<int32_t> &data, int playerId)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);

    NetEvent e;
    e.type = type;
    e.textData = text;
    e.intData = data;
    e.playerId = playerId;
    m_eventQueue.push(e);
}

bool NetworkClient::HasEvents()
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    return !m_eventQueue.empty();
}

NetEvent NetworkClient::PopEvent()
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    if (m_eventQueue.empty())
        return {};
    NetEvent e = m_eventQueue.front();
    m_eventQueue.pop();
    return e;
}

void NetworkClient::HandlePlayerConnected(const uint8_t *data, int len)
{
    try
    {
        msgpack::object_handle oh = msgpack::unpack((const char *)data, len);
        msgpack::object obj = oh.get();

        PlayerConnectedBroadcast broadcast;
        obj.convert(broadcast);

        EnqueueEvent(PacketType::PlayerConnected, broadcast.username, {}, broadcast.player_id);
    }
    catch (...)
    {
    }
}

void NetworkClient::HandlePlayerDisconnected(const uint8_t *data, int len)
{
    try
    {
        msgpack::object_handle oh = msgpack::unpack((const char *)data, len);
        msgpack::object obj = oh.get();

        PlayerDisconnectedBroadcast broadcast;
        obj.convert(broadcast);

        EnqueueEvent(PacketType::PlayerDisconnected, broadcast.username, {}, broadcast.player_id);
    }
    catch (...)
    {
    }
}

void NetworkClient::HandleJiggyCollected(const uint8_t *data, int len)
{
    try
    {
        msgpack::object_handle oh = msgpack::unpack((const char *)data, len);
        msgpack::object obj = oh.get();

        BroadcastJiggy broadcast;
        obj.convert(broadcast);

        EnqueueEvent(PacketType::JiggyCollected, broadcast.collector, {broadcast.jiggy_enum_id, broadcast.collected_value});
    }
    catch (...)
    {
    }
}

void NetworkClient::HandleNoteCollected(const uint8_t *data, int len)
{
    try
    {
        msgpack::object_handle oh = msgpack::unpack((const char *)data, len);
        msgpack::object obj = oh.get();

        BroadcastNote broadcast;
        obj.convert(broadcast);

        EnqueueEvent(PacketType::NoteCollected, broadcast.collector, {broadcast.map_id, broadcast.level_id, broadcast.is_dynamic ? 1 : 0, broadcast.note_index});
    }
    catch (...)
    {
    }
}

void NetworkClient::HandleNoteCollectedPos(const uint8_t *data, int len)
{
    try
    {
        msgpack::object_handle oh = msgpack::unpack((const char *)data, len);
        msgpack::object obj = oh.get();

        BroadcastNotePos broadcast;
        obj.convert(broadcast);

        EnqueueEvent(PacketType::NoteCollectedPos, broadcast.collector, {broadcast.map_id, broadcast.x, broadcast.y, broadcast.z});
    }
    catch (...)
    {
    }
}

void NetworkClient::HandleNoteSaveData(const uint8_t *data, int len)
{
    /*

    Original msgpack code:
        msgpack::object_handle oh = msgpack::unpack((const char *)data, len);
        msgpack::object obj = oh.get();
        NoteSaveDataPacket packet;
        obj.convert(packet);

        std::vector<int32_t> eventData;
        eventData.push_back(packet.LevelIndex);
        eventData.push_back((int32_t)packet.SaveData.size());
        EnqueueEvent(PacketType::NoteSaveData, "", eventData);
    */
}

void NetworkClient::HandlePuppetUpdate(const uint8_t *data, int len)
{
    // Expected format (after player_id):
    // 8 floats (32 bytes): x, y, z, yaw, pitch, roll, anim_duration, anim_timer
    // 2 bytes: level_id (int16, big-endian)
    // 2 bytes: map_id (int16, big-endian)
    // 2 bytes: anim_id (int16, big-endian)
    // 4 bytes: model_id, flags, playback_type, playback_direction (uint8s)
    const int EXPECTED_LEN = 32 + 2 + 2 + 2 + 4; // 42 bytes
    
    if (len < EXPECTED_LEN)
    {
        return;
    }

    uint32_t player_id;
    std::memcpy(&player_id, data, 4);
    data += 4;

    // Read 8 floats (x, y, z, yaw, pitch, roll, anim_duration, anim_timer)
    float x, y, z, yaw, pitch, roll, anim_duration, anim_timer;
    
    auto read_float = [&data]() -> float {
        uint32_t bits = ((uint32_t)data[0] << 24) | 
                        ((uint32_t)data[1] << 16) | 
                        ((uint32_t)data[2] << 8) | 
                        ((uint32_t)data[3]);
        data += 4;
        float result;
        std::memcpy(&result, &bits, sizeof(float));
        return result;
    };

    x = read_float();
    y = read_float();
    z = read_float();
    yaw = read_float();
    pitch = read_float();
    roll = read_float();
    anim_duration = read_float();
    anim_timer = read_float();

    // Read int16s (big-endian)
    int16_t level_id = (int16_t)(((uint16_t)data[0] << 8) | (uint16_t)data[1]);
    data += 2;
    int16_t map_id = (int16_t)(((uint16_t)data[0] << 8) | (uint16_t)data[1]);
    data += 2;
    int16_t anim_id = (int16_t)(((uint16_t)data[0] << 8) | (uint16_t)data[1]);
    data += 2;

    // Read uint8s
    uint8_t model_id = data[0];
    uint8_t flags = data[1];
    uint8_t playback_type = data[2];
    uint8_t playback_direction = data[3];

    // Pack data for the message queue
    // The mod expects:
    // paramF1-F3: x, y, z
    // param1-3: yaw, pitch, roll (as float bits in int32)
    // paramF4-F5: anim_duration, anim_timer
    // param4: map_id (low 8 bits), level_id (next 8 bits), anim_id (high 16 bits)
    // param5: playback_type
    // param6: playback_direction

    std::vector<int32_t> payload;
    
    // Convert floats to int32 for storage in payload (reinterpret as bits)
    int32_t yaw_bits, pitch_bits, roll_bits;
    std::memcpy(&yaw_bits, &yaw, sizeof(float));
    std::memcpy(&pitch_bits, &pitch, sizeof(float));
    std::memcpy(&roll_bits, &roll, sizeof(float));
    
    payload.push_back(yaw_bits);   // param1
    payload.push_back(pitch_bits); // param2
    payload.push_back(roll_bits);  // param3
    
    // Pack map_id, level_id, anim_id into param4
    uint32_t packed = ((uint32_t)(uint16_t)anim_id << 16) | 
                      ((uint32_t)(uint8_t)level_id << 8) | 
                      ((uint32_t)(uint8_t)map_id);
    payload.push_back((int32_t)packed); // param4
    
    payload.push_back((int32_t)playback_type);     // param5
    payload.push_back((int32_t)playback_direction); // param6

    // Create NetEvent with the puppet data
    NetEvent e;
    e.type = PacketType::PuppetUpdate;
    e.playerId = (int)player_id;
    e.intData = payload;
    
    // Store x, y, z as text data (hacky but we need to pass them somehow)
    // Actually, let's use a different approach - store them in first 3 float params
    e.floatData.push_back(x);
    e.floatData.push_back(y);
    e.floatData.push_back(z);
    e.floatData.push_back(anim_duration);
    e.floatData.push_back(anim_timer);

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_eventQueue.push(e);
    }
}

void NetworkClient::HandleLevelOpened(const uint8_t *data, int len)
{
    try
    {
        msgpack::object_handle oh = msgpack::unpack((const char *)data, len);
        msgpack::object obj = oh.get();

        LevelOpenedPacket level;
        obj.convert(level);

        EnqueueEvent(PacketType::LevelOpened, "", {level.WorldId, level.JiggyCost});
    }
    catch (...)
    {
    }
}

void NetworkClient::SendJiggy(int jiggyEnumId, int collectedValue)
{
    uint8_t buffer[8];
    std::memcpy(&buffer[0], &jiggyEnumId, 4);
    std::memcpy(&buffer[4], &collectedValue, 4);
    SendRawPacket(PacketType::JiggyCollected, buffer, 8);
}

void NetworkClient::SendNote(int mapId, int levelId, bool isDynamic, int noteIndex)
{
    uint8_t buffer[13];
    std::memcpy(&buffer[0], &mapId, 4);
    std::memcpy(&buffer[4], &levelId, 4);
    buffer[8] = isDynamic ? 1 : 0;
    std::memcpy(&buffer[9], &noteIndex, 4);
    SendRawPacket(PacketType::NoteCollected, buffer, 13);
}

void NetworkClient::SendNotePos(int mapId, int x, int y, int z)
{
    uint8_t buffer[10];
    std::memcpy(&buffer[0], &mapId, 4);
    int16_t x16 = (int16_t)x;
    int16_t y16 = (int16_t)y;
    int16_t z16 = (int16_t)z;
    std::memcpy(&buffer[4], &x16, 2);
    std::memcpy(&buffer[6], &y16, 2);
    std::memcpy(&buffer[8], &z16, 2);
    SendRawPacket(PacketType::NoteCollectedPos, buffer, 10);
}

void NetworkClient::SendNoteSaveData(int levelIndex, const std::vector<uint8_t> &saveData)
{
    std::vector<uint8_t> buffer;
    buffer.resize(4 + saveData.size());
    std::memcpy(&buffer[0], &levelIndex, 4);
    if (!saveData.empty())
    {
        std::memcpy(&buffer[4], saveData.data(), saveData.size());
    }
    SendRawPacket(PacketType::NoteSaveData, buffer.data(), buffer.size());
}

void NetworkClient::SendLevelOpened(int worldId, int jiggyCost)
{
    uint8_t buffer[8];
    std::memcpy(&buffer[0], &worldId, 4);
    std::memcpy(&buffer[4], &jiggyCost, 4);
    SendRawPacket(PacketType::LevelOpened, buffer, 8);
}

void NetworkClient::SendPuppetUpdate(const PuppetUpdatePacket &pak)
{
    std::vector<uint8_t> buffer;
    buffer.reserve(128); // Reserve enough space

    auto write_float = [&buffer](float f)
    {
        uint32_t bits;
        std::memcpy(&bits, &f, sizeof(float));
        buffer.push_back((bits >> 24) & 0xFF);
        buffer.push_back((bits >> 16) & 0xFF);
        buffer.push_back((bits >> 8) & 0xFF);
        buffer.push_back(bits & 0xFF);
    };

    write_float(pak.x);
    write_float(pak.y);
    write_float(pak.z);
    write_float(pak.yaw);
    write_float(pak.pitch);
    write_float(pak.roll);
    write_float(pak.anim_duration);
    write_float(pak.anim_timer);

    buffer.push_back((pak.level_id >> 8) & 0xFF);
    buffer.push_back(pak.level_id & 0xFF);
    buffer.push_back((pak.map_id >> 8) & 0xFF);
    buffer.push_back(pak.map_id & 0xFF);

    buffer.push_back((pak.anim_id >> 8) & 0xFF);
    buffer.push_back(pak.anim_id & 0xFF);

    buffer.push_back(pak.model_id);
    buffer.push_back(pak.flags);
    buffer.push_back(pak.playback_type);
    buffer.push_back(pak.playback_direction);

    SendRawPacket(PacketType::PuppetUpdate, buffer.data(), buffer.size());
}

void NetworkClient::RequestFullSync()
{
    SendRawPacket(PacketType::FullSyncRequest, nullptr, 0);
}

void NetworkClient::SendFileProgressFlags(const std::vector<uint8_t> &flags)
{
    if (!flags.empty())
    {
        SendRawPacket(PacketType::FileProgressFlags, flags.data(), flags.size());
    }
}

void NetworkClient::SendAbilityProgress(const std::vector<uint8_t> &bytes)
{
    if (!bytes.empty())
    {
        SendRawPacket(PacketType::AbilityProgress, bytes.data(), bytes.size());
    }
}

void NetworkClient::SendHoneycombScore(const std::vector<uint8_t> &bytes)
{
    if (!bytes.empty())
    {
        SendRawPacket(PacketType::HoneycombScore, bytes.data(), bytes.size());
    }
}

void NetworkClient::SendMumboScore(const std::vector<uint8_t> &bytes)
{
    if (!bytes.empty())
    {
        SendRawPacket(PacketType::MumboScore, bytes.data(), bytes.size());
    }
}

void NetworkClient::SendHoneycombCollected(int mapId, int honeycombId, int x, int y, int z)
{
    uint8_t buffer[20];
    std::memcpy(&buffer[0], &mapId, 4);
    std::memcpy(&buffer[4], &honeycombId, 4);
    std::memcpy(&buffer[8], &x, 4);
    std::memcpy(&buffer[12], &y, 4);
    std::memcpy(&buffer[16], &z, 4);
    SendRawPacket(PacketType::HoneycombCollected, buffer, 20);
}

void NetworkClient::SendMumboTokenCollected(int mapId, int tokenId, int x, int y, int z)
{
    uint8_t buffer[20];
    std::memcpy(&buffer[0], &mapId, 4);
    std::memcpy(&buffer[4], &tokenId, 4);
    std::memcpy(&buffer[8], &x, 4);
    std::memcpy(&buffer[12], &y, 4);
    std::memcpy(&buffer[16], &z, 4);
    SendRawPacket(PacketType::MumboTokenCollected, buffer, 20);
}

void NetworkClient::UploadInitialSaveData()
{
}
