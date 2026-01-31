#include "lib_net.h"
#include <iostream>
#include <chrono>
#include <sstream>

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
                std::cout << "[Net] Connected!" << std::endl;
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
    try
    {
        msgpack::object_handle oh = msgpack::unpack((const char *)data, len);
        msgpack::object obj = oh.get();

        NoteSaveDataPacket packet;
        obj.convert(packet);

        std::vector<int32_t> eventData;
        eventData.push_back(packet.LevelIndex);
        eventData.push_back((int32_t)packet.SaveData.size());

        EnqueueEvent(PacketType::NoteSaveData, "", eventData);
    }
    catch (...)
    {
    }
}

void NetworkClient::HandlePuppetUpdate(const uint8_t *data, int len)
{
    if (len < 4)
    {
        return;
    }

    try
    {
        uint32_t player_id;
        std::memcpy(&player_id, data, 4);
        msgpack::object_handle oh = msgpack::unpack((const char *)(data + 4), len - 4);
        msgpack::object obj = oh.get();

        PuppetUpdatePacket p;
        obj.convert(p);

        std::vector<int32_t> payload;
        payload.push_back((int32_t)p.x);
        payload.push_back((int32_t)p.y);
        payload.push_back((int32_t)p.z);

        EnqueueEvent(PacketType::PuppetUpdate, "", payload, player_id);
    }
    catch (...)
    {
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
    JiggyPacket pak;
    pak.JiggyEnumId = jiggyEnumId;
    pak.CollectedValue = collectedValue;
    msgpack::sbuffer sbuf;
    msgpack::pack(sbuf, pak);
    SendRawPacket(PacketType::JiggyCollected, sbuf.data(), sbuf.size());
}

void NetworkClient::SendNote(int mapId, int levelId, bool isDynamic, int noteIndex)
{
    NotePacket pak;
    pak.MapId = mapId;
    pak.LevelId = levelId;
    pak.IsDynamic = isDynamic;
    pak.NoteIndex = noteIndex;
    msgpack::sbuffer sbuf;
    msgpack::pack(sbuf, pak);
    SendRawPacket(PacketType::NoteCollected, sbuf.data(), sbuf.size());
}

void NetworkClient::SendNotePos(int mapId, int x, int y, int z)
{
    NotePacketPos pak;
    pak.MapId = mapId;
    pak.X = (int16_t)x;
    pak.Y = (int16_t)y;
    pak.Z = (int16_t)z;
    msgpack::sbuffer sbuf;
    msgpack::pack(sbuf, pak);
    SendRawPacket(PacketType::NoteCollectedPos, sbuf.data(), sbuf.size());
}

void NetworkClient::SendNoteSaveData(int levelIndex, const std::vector<uint8_t> &saveData)
{
    NoteSaveDataPacket pak;
    pak.LevelIndex = levelIndex;
    pak.SaveData = saveData;
    msgpack::sbuffer sbuf;
    msgpack::pack(sbuf, pak);
    SendRawPacket(PacketType::NoteSaveData, sbuf.data(), sbuf.size());
}

void NetworkClient::SendLevelOpened(int worldId, int jiggyCost)
{
    LevelOpenedPacket pak;
    pak.WorldId = worldId;
    pak.JiggyCost = jiggyCost;
    msgpack::sbuffer sbuf;
    msgpack::pack(sbuf, pak);
    SendRawPacket(PacketType::LevelOpened, sbuf.data(), sbuf.size());
}

void NetworkClient::SendPuppetUpdate(const PuppetUpdatePacket &pak)
{
    msgpack::sbuffer sbuf;
    msgpack::pack(sbuf, pak);
    SendRawPacket(PacketType::PuppetUpdate, sbuf.data(), sbuf.size());
}

void NetworkClient::RequestFullSync()
{
    SendRawPacket(PacketType::FullSyncRequest, nullptr, 0);
}
