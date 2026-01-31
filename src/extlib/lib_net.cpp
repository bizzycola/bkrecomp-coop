#include "lib_net.h"
#include "debug_log.h"
#include <iostream>
#include <chrono>
#include <sstream>

const uint32_t HANDSHAKE_INTERVAL_MS = 1000;
const uint32_t PING_INTERVAL_MS = 10000;

NetworkClient::NetworkClient()
    : m_udpSocket(INVALID_SOCKET), m_isConnected(false), m_needsInit(false),
      m_lastHandshakeTime(0), m_lastPingTime(0), m_lastPacketSentTime(0)
{
    debug_log("[NetworkClient] Constructor called");
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0)
    {
        debug_log("[NetworkClient] WSAStartup failed with error: " + std::to_string(result));
    }
    else
    {
        debug_log("[NetworkClient] WSAStartup succeeded");
    }
#endif
}

NetworkClient::~NetworkClient()
{
    debug_log("[NetworkClient] Destructor called");
    if (m_udpSocket != INVALID_SOCKET)
    {
        debug_log("[NetworkClient] Closing socket");
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
    std::ostringstream oss;
    oss << "[NetworkClient] Configure - Host: " << host
        << ", User: " << user
        << ", Lobby: " << lobby
        << ", HasPassword: " << (!pass.empty());
    debug_log(oss.str());

    m_host = host;
    m_user = user;
    m_lobby = lobby;
    m_pass = pass;
    m_needsInit = true;

    // Reset state
    m_isConnected = false;
    m_lastHandshakeTime = 0;

    debug_log("[NetworkClient] Configuration complete, marked for initialization");
}

uint32_t NetworkClient::GetClockMS()
{
    using namespace std::chrono;
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

bool NetworkClient::PerformLazyInit()
{
    debug_log("[NetworkClient] PerformLazyInit called");

    if (m_udpSocket != INVALID_SOCKET)
    {
        debug_log("[NetworkClient] Closing existing socket");
#ifdef _WIN32
        closesocket(m_udpSocket);
#else
        close(m_udpSocket);
#endif
    }

    m_udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_udpSocket == INVALID_SOCKET)
    {
#ifdef _WIN32
        int error = WSAGetLastError();
        debug_log("[NetworkClient] Failed to create socket, WSA error: " + std::to_string(error));
#else
        debug_log("[NetworkClient] Failed to create socket");
#endif
        return false;
    }
    debug_log("[NetworkClient] Socket created successfully");

#ifdef _WIN32
    u_long mode = 1;
    if (ioctlsocket(m_udpSocket, FIONBIO, &mode) != 0)
    {
        debug_log("[NetworkClient] Failed to set non-blocking mode");
    }
    else
    {
        debug_log("[NetworkClient] Set non-blocking mode");
    }
#else
    int flags = fcntl(m_udpSocket, F_GETFL, 0);
    fcntl(m_udpSocket, F_SETFL, flags | O_NONBLOCK);
    debug_log("[NetworkClient] Set non-blocking mode");
#endif

    memset(&m_serverAddr, 0, sizeof(m_serverAddr));
    m_serverAddr.sin_family = AF_INET;
    m_serverAddr.sin_port = htons(8756);

    if (inet_pton(AF_INET, m_host.c_str(), &m_serverAddr.sin_addr) <= 0)
    {
        debug_log("[NetworkClient] inet_pton failed for host: " + m_host);
        return false;
    }

    std::ostringstream oss;
    oss << "[NetworkClient] Server address configured: " << m_host << ":8756";
    debug_log(oss.str());

    return true;
}

void NetworkClient::SendRawPacket(PacketType type, const void *data, size_t size)
{
    if (m_udpSocket == INVALID_SOCKET)
    {
        debug_log("[NetworkClient] SendRawPacket called but socket is invalid");
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
        std::ostringstream oss;
        oss << "[NetworkClient] Sent packet type " << (int)type << ", size: " << buffer.size() << " bytes";
        debug_log(oss.str());
    }
    else
    {
#ifdef _WIN32
        int error = WSAGetLastError();
        std::ostringstream oss;
        oss << "[NetworkClient] sendto failed, WSA error: " << error;
        debug_log(oss.str());
#else
        debug_log("[NetworkClient] sendto failed");
#endif
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
        debug_log("[NetworkClient] Update: Performing lazy initialization");
        if (PerformLazyInit())
        {
            m_needsInit = false;
            debug_log("[NetworkClient] Initialization successful");
        }
        else
        {
            debug_log("[NetworkClient] Initialization failed");
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
        debug_log("[NetworkClient] Sending handshake packet");

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
            debug_log("[NetworkClient] Sending ping");
            SendPing();
            m_lastPingTime = now;
        }
    }

    struct sockaddr_in from;
    int fromLen = sizeof(from);
    uint8_t buf[2048];

    int packetsReceived = 0;

    while (true)
    {
        int len = recvfrom(m_udpSocket, (char *)buf, sizeof(buf), 0, (struct sockaddr *)&from, &fromLen);

        if (len > 0)
        {
            packetsReceived++;
            uint8_t type = buf[0];
            uint8_t *payload = &buf[1];
            int payload_len = len - 1;

            if (!m_isConnected)
            {
                m_isConnected = true;
                m_lastPacketSentTime = now;
                debug_log("[NetworkClient] Connected to server!");
                std::cout << "[Net] Connected!" << std::endl;
                RequestFullSync();
            }

            std::ostringstream oss;
            oss << "[NetworkClient] Received packet type " << (int)type << ", size: " << len << " bytes";
            debug_log(oss.str());

            switch (static_cast<PacketType>(type))
            {
            case PacketType::PlayerConnected:
                debug_log("[NetworkClient] Handling PlayerConnected");
                HandlePlayerConnected(payload, payload_len);
                break;
            case PacketType::PlayerDisconnected:
                debug_log("[NetworkClient] Handling PlayerDisconnected");
                HandlePlayerDisconnected(payload, payload_len);
                break;
            case PacketType::JiggyCollected:
                debug_log("[NetworkClient] Handling JiggyCollected");
                HandleJiggyCollected(payload, payload_len);
                break;
            case PacketType::NoteCollected:
                debug_log("[NetworkClient] Handling NoteCollected");
                HandleNoteCollected(payload, payload_len);
                break;
            case PacketType::NoteCollectedPos:
                debug_log("[NetworkClient] Handling NoteCollectedPos");
                HandleNoteCollectedPos(payload, payload_len);
                break;
            case PacketType::NoteSaveData:
                debug_log("[NetworkClient] Handling NoteSaveData");
                HandleNoteSaveData(payload, payload_len);
                break;
            case PacketType::PuppetUpdate:
                // Don't log puppet updates - too frequent
                HandlePuppetUpdate(payload, payload_len);
                break;
            case PacketType::LevelOpened:
                debug_log("[NetworkClient] Handling LevelOpened");
                HandleLevelOpened(payload, payload_len);
                break;
            case PacketType::Pong:
                debug_log("[NetworkClient] Received Pong");
                break;
            default:
                std::ostringstream oss2;
                oss2 << "[NetworkClient] Unknown packet type: " << (int)type;
                debug_log(oss2.str());
                break;
            }
        }
        else
        {
            break;
        }
    }

    if (packetsReceived > 0)
    {
        std::ostringstream oss;
        oss << "[NetworkClient] Processed " << packetsReceived << " packets this update";
        debug_log(oss.str());
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

        std::ostringstream oss;
        oss << "[NetworkClient] Player connected: " << broadcast.username << " (ID: " << broadcast.player_id << ")";
        debug_log(oss.str());

        EnqueueEvent(PacketType::PlayerConnected, broadcast.username, {}, broadcast.player_id);
    }
    catch (const std::exception &e)
    {
        debug_log(std::string("[NetworkClient] Exception in HandlePlayerConnected: ") + e.what());
    }
    catch (...)
    {
        debug_log("[NetworkClient] Unknown exception in HandlePlayerConnected");
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

        std::ostringstream oss;
        oss << "[NetworkClient] Player disconnected: " << broadcast.username << " (ID: " << broadcast.player_id << ")";
        debug_log(oss.str());

        EnqueueEvent(PacketType::PlayerDisconnected, broadcast.username, {}, broadcast.player_id);
    }
    catch (const std::exception &e)
    {
        debug_log(std::string("[NetworkClient] Exception in HandlePlayerDisconnected: ") + e.what());
    }
    catch (...)
    {
        debug_log("[NetworkClient] Unknown exception in HandlePlayerDisconnected");
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

        std::ostringstream oss;
        oss << "[NetworkClient] Jiggy collected by " << broadcast.collector
            << " (Level: " << broadcast.level_id << ", Jiggy: " << broadcast.jiggy_id << ")";
        debug_log(oss.str());

        EnqueueEvent(PacketType::JiggyCollected, broadcast.collector, {broadcast.level_id, broadcast.jiggy_id});
    }
    catch (const std::exception &e)
    {
        debug_log(std::string("[NetworkClient] Exception in HandleJiggyCollected: ") + e.what());
    }
    catch (...)
    {
        debug_log("[NetworkClient] Unknown exception in HandleJiggyCollected");
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

        std::ostringstream oss;
        oss << "[NetworkClient] Note collected by " << broadcast.collector
            << " (Map: " << broadcast.map_id << ", Level: " << broadcast.level_id << ")";
        debug_log(oss.str());

        EnqueueEvent(PacketType::NoteCollected, broadcast.collector, {broadcast.map_id, broadcast.level_id, broadcast.is_dynamic ? 1 : 0, broadcast.note_index});
    }
    catch (const std::exception &e)
    {
        debug_log(std::string("[NetworkClient] Exception in HandleNoteCollected: ") + e.what());
    }
    catch (...)
    {
        debug_log("[NetworkClient] Unknown exception in HandleNoteCollected");
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
    catch (const std::exception &e)
    {
        debug_log(std::string("[NetworkClient] Exception in HandleNoteCollectedPos: ") + e.what());
    }
    catch (...)
    {
        debug_log("[NetworkClient] Unknown exception in HandleNoteCollectedPos");
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

        std::ostringstream oss;
        oss << "[NetworkClient] Note save data received (Level: " << packet.LevelIndex
            << ", Size: " << packet.SaveData.size() << " bytes)";
        debug_log(oss.str());

        std::vector<int32_t> eventData;
        eventData.push_back(packet.LevelIndex);
        eventData.push_back((int32_t)packet.SaveData.size());

        EnqueueEvent(PacketType::NoteSaveData, "", eventData);
    }
    catch (const std::exception &e)
    {
        debug_log(std::string("[NetworkClient] Exception in HandleNoteSaveData: ") + e.what());
    }
    catch (...)
    {
        debug_log("[NetworkClient] Unknown exception in HandleNoteSaveData");
    }
}

void NetworkClient::HandlePuppetUpdate(const uint8_t *data, int len)
{
    if (len < 4)
    {
        debug_log("[NetworkClient] HandlePuppetUpdate: Packet too small");
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
    catch (const std::exception &e)
    {
        debug_log(std::string("[NetworkClient] Exception in HandlePuppetUpdate: ") + e.what());
    }
    catch (...)
    {
        debug_log("[NetworkClient] Unknown exception in HandlePuppetUpdate");
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

        std::ostringstream oss;
        oss << "[NetworkClient] Level opened (World: " << level.WorldId
            << ", Cost: " << level.JiggyCost << ")";
        debug_log(oss.str());

        EnqueueEvent(PacketType::LevelOpened, "", {level.WorldId, level.JiggyCost});
    }
    catch (const std::exception &e)
    {
        debug_log(std::string("[NetworkClient] Exception in HandleLevelOpened: ") + e.what());
    }
    catch (...)
    {
        debug_log("[NetworkClient] Unknown exception in HandleLevelOpened");
    }
}

void NetworkClient::SendJiggy(int levelId, int jiggyId)
{
    std::ostringstream oss;
    oss << "[NetworkClient] Sending Jiggy (Level: " << levelId << ", Jiggy: " << jiggyId << ")";
    debug_log(oss.str());

    JiggyPacket pak;
    pak.LevelId = levelId;
    pak.JiggyId = jiggyId;
    msgpack::sbuffer sbuf;
    msgpack::pack(sbuf, pak);
    SendRawPacket(PacketType::JiggyCollected, sbuf.data(), sbuf.size());
}

void NetworkClient::SendNote(int mapId, int levelId, bool isDynamic, int noteIndex)
{
    std::ostringstream oss;
    oss << "[NetworkClient] Sending Note (Map: " << mapId << ", Level: " << levelId << ")";
    debug_log(oss.str());

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
    std::ostringstream oss;
    oss << "[NetworkClient] Sending Note Save Data (Level: " << levelIndex
        << ", Size: " << saveData.size() << " bytes)";
    debug_log(oss.str());

    NoteSaveDataPacket pak;
    pak.LevelIndex = levelIndex;
    pak.SaveData = saveData;
    msgpack::sbuffer sbuf;
    msgpack::pack(sbuf, pak);
    SendRawPacket(PacketType::NoteSaveData, sbuf.data(), sbuf.size());
}

void NetworkClient::SendLevelOpened(int worldId, int jiggyCost)
{
    std::ostringstream oss;
    oss << "[NetworkClient] Sending Level Opened (World: " << worldId
        << ", Cost: " << jiggyCost << ")";
    debug_log(oss.str());

    LevelOpenedPacket pak;
    pak.WorldId = worldId;
    pak.JiggyCost = jiggyCost;
    msgpack::sbuffer sbuf;
    msgpack::pack(sbuf, pak);
    SendRawPacket(PacketType::LevelOpened, sbuf.data(), sbuf.size());
}

void NetworkClient::SendPuppetUpdate(const PuppetUpdatePacket &pak)
{
    // Don't log puppet updates - too frequent
    msgpack::sbuffer sbuf;
    msgpack::pack(sbuf, pak);
    SendRawPacket(PacketType::PuppetUpdate, sbuf.data(), sbuf.size());
}

void NetworkClient::RequestFullSync()
{
    debug_log("[NetworkClient] Requesting full sync");
    SendRawPacket(PacketType::FullSyncRequest, nullptr, 0);
}
