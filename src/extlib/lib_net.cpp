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
      m_lastHandshakeTime(0), m_lastPingTime(0), m_lastPacketSentTime(0), m_reliableSeqCounter(0)
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

bool NetworkClient::IsReliableType(PacketType type)
{
    return type == PacketType::JiggyCollected ||
           type == PacketType::NoteCollected ||
           type == PacketType::NoteCollectedPos ||
           type == PacketType::NoteSaveData ||
           type == PacketType::FileProgressFlags ||
           type == PacketType::AbilityProgress ||
           type == PacketType::HoneycombScore ||
           type == PacketType::MumboScore ||
           type == PacketType::HoneycombCollected ||
           type == PacketType::MumboTokenCollected ||
           type == PacketType::LevelOpened ||
           type == PacketType::FullSyncRequest;
}

void NetworkClient::SendReliablePacket(PacketType type, const void *data, size_t size)
{
    if (m_udpSocket == INVALID_SOCKET)
    {
        return;
    }

    uint32_t seq = m_reliableSeqCounter++;
    std::vector<uint8_t> buffer(1 + 4 + size);
    buffer[0] = static_cast<uint8_t>(type);

    buffer[1] = (seq) & 0xFF;
    buffer[2] = (seq >> 8) & 0xFF;
    buffer[3] = (seq >> 16) & 0xFF;
    buffer[4] = (seq >> 24) & 0xFF;

    if (size > 0 && data != nullptr)
    {
        std::memcpy(&buffer[5], data, size);
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
        std::vector<uint8_t> buffer;

        uint32_t lobby_len = (uint32_t)m_lobby.size();
        buffer.push_back((lobby_len >> 24) & 0xFF);
        buffer.push_back((lobby_len >> 16) & 0xFF);
        buffer.push_back((lobby_len >> 8) & 0xFF);
        buffer.push_back(lobby_len & 0xFF);
        buffer.insert(buffer.end(), m_lobby.begin(), m_lobby.end());

        uint32_t pass_len = (uint32_t)m_pass.size();
        buffer.push_back((pass_len >> 24) & 0xFF);
        buffer.push_back((pass_len >> 16) & 0xFF);
        buffer.push_back((pass_len >> 8) & 0xFF);
        buffer.push_back(pass_len & 0xFF);
        buffer.insert(buffer.end(), m_pass.begin(), m_pass.end());

        uint32_t user_len = (uint32_t)m_user.size();
        buffer.push_back((user_len >> 24) & 0xFF);
        buffer.push_back((user_len >> 16) & 0xFF);
        buffer.push_back((user_len >> 8) & 0xFF);
        buffer.push_back(user_len & 0xFF);
        buffer.insert(buffer.end(), m_user.begin(), m_user.end());

        SendRawPacket(PacketType::Handshake, buffer.data(), buffer.size());
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

            if (IsReliableType(static_cast<PacketType>(type)) && payload_len >= 4)
            {
                uint32_t seq;
                std::memcpy(&seq, payload, 4);

                SendRawPacket(PacketType::ReliableAck, &seq, 4);

                payload += 4;
                payload_len -= 4;
            }

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

                break;
            case PacketType::FileProgressFlags:
                HandleFileProgressFlags(payload, payload_len);
                break;
            case PacketType::AbilityProgress:
                HandleAbilityProgress(payload, payload_len);
                break;
            case PacketType::HoneycombScore:
                HandleHoneycombScore(payload, payload_len);
                break;
            case PacketType::MumboScore:
                HandleMumboScore(payload, payload_len);
                break;
            case PacketType::HoneycombCollected:
                HandleHoneycombCollected(payload, payload_len);
                break;
            case PacketType::MumboTokenCollected:
                HandleMumboTokenCollected(payload, payload_len);
                break;
            case PacketType::PlayerInfoRequest:
                HandlePlayerInfoRequest(payload, payload_len);
                break;
            case PacketType::PlayerInfoResponse:
                HandlePlayerInfoResponse(payload, payload_len);
                break;
            case PacketType::PlayerListUpdate:
                HandlePlayerListUpdate(payload, payload_len);
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
    if (len < 8)
        return;

    uint32_t player_id = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                         ((uint32_t)data[2] << 8) | ((uint32_t)data[3]);

    uint32_t username_len = ((uint32_t)data[4] << 24) | ((uint32_t)data[5] << 16) |
                            ((uint32_t)data[6] << 8) | ((uint32_t)data[7]);

    if (len < (int)(8 + username_len))
        return;

    std::string username((const char *)(data + 8), username_len);

    EnqueueEvent(PacketType::PlayerConnected, username, {}, player_id);
}

void NetworkClient::HandlePlayerDisconnected(const uint8_t *data, int len)
{
    if (len < 8)
        return;

    uint32_t player_id = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                         ((uint32_t)data[2] << 8) | ((uint32_t)data[3]);

    uint32_t username_len = ((uint32_t)data[4] << 24) | ((uint32_t)data[5] << 16) |
                            ((uint32_t)data[6] << 8) | ((uint32_t)data[7]);

    if (len < (int)(8 + username_len))
        return;

    std::string username((const char *)(data + 8), username_len);

    EnqueueEvent(PacketType::PlayerDisconnected, username, {}, player_id);
}

void NetworkClient::HandleJiggyCollected(const uint8_t *data, int len)
{
    if (len < 12)
        return;

    uint32_t player_id = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                         ((uint32_t)data[2] << 8) | ((uint32_t)data[3]);

    int32_t jiggy_enum_id = ((int32_t)data[4] << 24) | ((int32_t)data[5] << 16) |
                            ((int32_t)data[6] << 8) | ((int32_t)data[7]);

    int32_t collected_value = ((int32_t)data[8] << 24) | ((int32_t)data[9] << 16) |
                              ((int32_t)data[10] << 8) | ((int32_t)data[11]);

    EnqueueEvent(PacketType::JiggyCollected, "", {jiggy_enum_id, collected_value}, player_id);
}

void NetworkClient::HandleNoteCollected(const uint8_t *data, int len)
{
    if (len < 20)
        return;

    uint32_t player_id = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                         ((uint32_t)data[2] << 8) | ((uint32_t)data[3]);

    int32_t map_id = ((int32_t)data[4] << 24) | ((int32_t)data[5] << 16) |
                     ((int32_t)data[6] << 8) | ((int32_t)data[7]);

    int32_t level_id = ((int32_t)data[8] << 24) | ((int32_t)data[9] << 16) |
                       ((int32_t)data[10] << 8) | ((int32_t)data[11]);

    int32_t is_dynamic = ((int32_t)data[12] << 24) | ((int32_t)data[13] << 16) |
                         ((int32_t)data[14] << 8) | ((int32_t)data[15]);

    int32_t note_index = ((int32_t)data[16] << 24) | ((int32_t)data[17] << 16) |
                         ((int32_t)data[18] << 8) | ((int32_t)data[19]);

    printf("[CLIENT] Received NoteCollected broadcast: map=%d, level=%d, is_dynamic=%d, note_index=%d\n",
           map_id, level_id, is_dynamic, note_index);

    EnqueueEvent(PacketType::NoteCollected, "", {map_id, level_id, is_dynamic, note_index}, player_id);
}

void NetworkClient::HandleNoteCollectedPos(const uint8_t *data, int len)
{
    if (len < 20)
        return;

    uint32_t player_id = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                         ((uint32_t)data[2] << 8) | ((uint32_t)data[3]);

    int32_t map_id = ((int32_t)data[4] << 24) | ((int32_t)data[5] << 16) |
                     ((int32_t)data[6] << 8) | ((int32_t)data[7]);

    int32_t x = ((int32_t)data[8] << 24) | ((int32_t)data[9] << 16) |
                ((int32_t)data[10] << 8) | ((int32_t)data[11]);

    int32_t y = ((int32_t)data[12] << 24) | ((int32_t)data[13] << 16) |
                ((int32_t)data[14] << 8) | ((int32_t)data[15]);

    int32_t z = ((int32_t)data[16] << 24) | ((int32_t)data[17] << 16) |
                ((int32_t)data[18] << 8) | ((int32_t)data[19]);

    EnqueueEvent(PacketType::NoteCollectedPos, "", {map_id, x, y, z}, player_id);
}

void NetworkClient::HandleNoteSaveData(const uint8_t *data, int len)
{
}

void NetworkClient::HandlePuppetUpdate(const uint8_t *data, int len)
{
    const int EXPECTED_LEN = 32 + 2 + 2 + 2 + 4; // 42 bytes

    if (len < EXPECTED_LEN)
    {
        return;
    }

    uint32_t player_id;
    std::memcpy(&player_id, data, 4);
    data += 4;

    float x, y, z, yaw, pitch, roll, anim_duration, anim_timer;

    auto read_float = [&data]() -> float
    {
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

    int16_t level_id = (int16_t)(((uint16_t)data[0] << 8) | (uint16_t)data[1]);
    data += 2;
    int16_t map_id = (int16_t)(((uint16_t)data[0] << 8) | (uint16_t)data[1]);
    data += 2;
    int16_t anim_id = (int16_t)(((uint16_t)data[0] << 8) | (uint16_t)data[1]);
    data += 2;

    uint8_t model_id = data[0];
    uint8_t flags = data[1];
    uint8_t playback_type = data[2];
    uint8_t playback_direction = data[3];

    std::vector<int32_t> payload;

    int32_t yaw_bits, pitch_bits, roll_bits;
    std::memcpy(&yaw_bits, &yaw, sizeof(float));
    std::memcpy(&pitch_bits, &pitch, sizeof(float));
    std::memcpy(&roll_bits, &roll, sizeof(float));

    payload.push_back(yaw_bits);
    payload.push_back(pitch_bits);
    payload.push_back(roll_bits);

    uint32_t packed = ((uint32_t)(uint16_t)anim_id << 16) |
                      ((uint32_t)(uint8_t)level_id << 8) |
                      ((uint32_t)(uint8_t)map_id);
    payload.push_back((int32_t)packed);

    payload.push_back((int32_t)playback_type);
    payload.push_back((int32_t)playback_direction);

    NetEvent e;
    e.type = PacketType::PuppetUpdate;
    e.playerId = (int)player_id;
    e.intData = payload;

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
    if (len < 12)
        return;

    uint32_t player_id = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                         ((uint32_t)data[2] << 8) | ((uint32_t)data[3]);

    int32_t world_id = ((int32_t)data[4] << 24) | ((int32_t)data[5] << 16) |
                       ((int32_t)data[6] << 8) | ((int32_t)data[7]);

    int32_t jiggy_cost = ((int32_t)data[8] << 24) | ((int32_t)data[9] << 16) |
                         ((int32_t)data[10] << 8) | ((int32_t)data[11]);

    EnqueueEvent(PacketType::LevelOpened, "", {world_id, jiggy_cost}, player_id);
}

void NetworkClient::HandleFileProgressFlags(const uint8_t *data, int len)
{
    if (len < 4)
        return;

    uint32_t player_id = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                         ((uint32_t)data[2] << 8) | ((uint32_t)data[3]);

    int byte_count = len - 4;
    std::string flags_data((const char *)(data + 4), byte_count);

    NetEvent e;
    e.type = PacketType::FileProgressFlags;
    e.playerId = (int)player_id;
    e.intData.push_back(byte_count);
    e.textData = flags_data;

    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_eventQueue.push(e);
}

void NetworkClient::HandleAbilityProgress(const uint8_t *data, int len)
{
    if (len < 4)
        return;

    uint32_t player_id = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                         ((uint32_t)data[2] << 8) | ((uint32_t)data[3]);

    int byte_count = len - 4;
    std::string ability_data((const char *)(data + 4), byte_count);

    NetEvent e;
    e.type = PacketType::AbilityProgress;
    e.playerId = (int)player_id;
    e.intData.push_back(byte_count);
    e.textData = ability_data;

    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_eventQueue.push(e);
}

void NetworkClient::HandleHoneycombScore(const uint8_t *data, int len)
{
    if (len < 4)
        return;

    uint32_t player_id = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                         ((uint32_t)data[2] << 8) | ((uint32_t)data[3]);

    int byte_count = len - 4;
    std::string score_data((const char *)(data + 4), byte_count);

    NetEvent e;
    e.type = PacketType::HoneycombScore;
    e.playerId = (int)player_id;
    e.intData.push_back(byte_count);
    e.textData = score_data;

    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_eventQueue.push(e);
}

void NetworkClient::HandleMumboScore(const uint8_t *data, int len)
{
    if (len < 4)
        return;

    uint32_t player_id = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                         ((uint32_t)data[2] << 8) | ((uint32_t)data[3]);

    int byte_count = len - 4;
    std::string score_data((const char *)(data + 4), byte_count);

    NetEvent e;
    e.type = PacketType::MumboScore;
    e.playerId = (int)player_id;
    e.intData.push_back(byte_count);
    e.textData = score_data;

    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_eventQueue.push(e);
}

void NetworkClient::HandleHoneycombCollected(const uint8_t *data, int len)
{
    if (len < 24)
        return;

    uint32_t player_id = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                         ((uint32_t)data[2] << 8) | ((uint32_t)data[3]);

    int32_t map_id = ((int32_t)data[4] << 24) | ((int32_t)data[5] << 16) |
                     ((int32_t)data[6] << 8) | ((int32_t)data[7]);

    int32_t honeycomb_id = ((int32_t)data[8] << 24) | ((int32_t)data[9] << 16) |
                           ((int32_t)data[10] << 8) | ((int32_t)data[11]);

    int32_t x = ((int32_t)data[12] << 24) | ((int32_t)data[13] << 16) |
                ((int32_t)data[14] << 8) | ((int32_t)data[15]);

    int32_t y = ((int32_t)data[16] << 24) | ((int32_t)data[17] << 16) |
                ((int32_t)data[18] << 8) | ((int32_t)data[19]);

    int32_t z = ((int32_t)data[20] << 24) | ((int32_t)data[21] << 16) |
                ((int32_t)data[22] << 8) | ((int32_t)data[23]);

    EnqueueEvent(PacketType::HoneycombCollected, "", {map_id, honeycomb_id, x, y, z}, player_id);
}

void NetworkClient::HandleMumboTokenCollected(const uint8_t *data, int len)
{
    if (len < 24)
        return;

    uint32_t player_id = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                         ((uint32_t)data[2] << 8) | ((uint32_t)data[3]);

    int32_t map_id = ((int32_t)data[4] << 24) | ((int32_t)data[5] << 16) |
                     ((int32_t)data[6] << 8) | ((int32_t)data[7]);

    int32_t token_id = ((int32_t)data[8] << 24) | ((int32_t)data[9] << 16) |
                       ((int32_t)data[10] << 8) | ((int32_t)data[11]);

    int32_t x = ((int32_t)data[12] << 24) | ((int32_t)data[13] << 16) |
                ((int32_t)data[14] << 8) | ((int32_t)data[15]);

    int32_t y = ((int32_t)data[16] << 24) | ((int32_t)data[17] << 16) |
                ((int32_t)data[18] << 8) | ((int32_t)data[19]);

    int32_t z = ((int32_t)data[20] << 24) | ((int32_t)data[21] << 16) |
                ((int32_t)data[22] << 8) | ((int32_t)data[23]);

    EnqueueEvent(PacketType::MumboTokenCollected, "", {map_id, token_id, x, y, z}, player_id);
}

void NetworkClient::SendJiggy(int jiggyEnumId, int collectedValue)
{
    uint8_t buffer[8];
    std::memcpy(&buffer[0], &jiggyEnumId, 4);
    std::memcpy(&buffer[4], &collectedValue, 4);
    SendReliablePacket(PacketType::JiggyCollected, buffer, 8);
}

void NetworkClient::SendNote(int mapId, int levelId, bool isDynamic, int noteIndex)
{
    printf("[CLIENT] SendNote: map=%d, level=%d, is_dynamic=%d, note_index=%d\n",
           mapId, levelId, isDynamic, noteIndex);

    uint8_t buffer[13];
    std::memcpy(&buffer[0], &mapId, 4);
    std::memcpy(&buffer[4], &levelId, 4);
    buffer[8] = isDynamic ? 1 : 0;
    std::memcpy(&buffer[9], &noteIndex, 4);
    SendReliablePacket(PacketType::NoteCollected, buffer, 13);
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
    SendReliablePacket(PacketType::NoteCollectedPos, buffer, 10);
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
    SendReliablePacket(PacketType::NoteSaveData, buffer.data(), buffer.size());
}

void NetworkClient::SendLevelOpened(int worldId, int jiggyCost)
{
    uint8_t buffer[8];
    std::memcpy(&buffer[0], &worldId, 4);
    std::memcpy(&buffer[4], &jiggyCost, 4);
    SendReliablePacket(PacketType::LevelOpened, buffer, 8);
}

void NetworkClient::SendPuppetUpdate(const PuppetUpdatePacket &pak)
{
    std::vector<uint8_t> buffer;
    buffer.reserve(128);

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
    SendReliablePacket(PacketType::FullSyncRequest, nullptr, 0);
}

void NetworkClient::SendFileProgressFlags(const std::vector<uint8_t> &flags)
{
    if (!flags.empty())
    {
        SendReliablePacket(PacketType::FileProgressFlags, flags.data(), flags.size());
    }
}

void NetworkClient::SendAbilityProgress(const std::vector<uint8_t> &bytes)
{
    if (!bytes.empty())
    {
        SendReliablePacket(PacketType::AbilityProgress, bytes.data(), bytes.size());
    }
}

void NetworkClient::SendHoneycombScore(const std::vector<uint8_t> &bytes)
{
    if (!bytes.empty())
    {
        SendReliablePacket(PacketType::HoneycombScore, bytes.data(), bytes.size());
    }
}

void NetworkClient::SendMumboScore(const std::vector<uint8_t> &bytes)
{
    if (!bytes.empty())
    {
        SendReliablePacket(PacketType::MumboScore, bytes.data(), bytes.size());
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
    SendReliablePacket(PacketType::HoneycombCollected, buffer, 20);
}

void NetworkClient::SendMumboTokenCollected(int mapId, int tokenId, int x, int y, int z)
{
    uint8_t buffer[20];
    std::memcpy(&buffer[0], &mapId, 4);
    std::memcpy(&buffer[4], &tokenId, 4);
    std::memcpy(&buffer[8], &x, 4);
    std::memcpy(&buffer[12], &y, 4);
    std::memcpy(&buffer[16], &z, 4);
    SendReliablePacket(PacketType::MumboTokenCollected, buffer, 20);
}

void NetworkClient::HandlePlayerInfoRequest(const uint8_t *data, int len)
{
    if (len < 8)
        return;

    uint32_t target_player_id = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                                ((uint32_t)data[2] << 8) | ((uint32_t)data[3]);

    uint32_t requester_player_id = ((uint32_t)data[4] << 24) | ((uint32_t)data[5] << 16) |
                                   ((uint32_t)data[6] << 8) | ((uint32_t)data[7]);

    EnqueueEvent(PacketType::PlayerInfoRequest, "", {(int32_t)target_player_id, (int32_t)requester_player_id}, 0);
}

void NetworkClient::HandlePlayerInfoResponse(const uint8_t *data, int len)
{
    if (len < 24)
        return;

    uint32_t target_player_id = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                                ((uint32_t)data[2] << 8) | ((uint32_t)data[3]);

    int16_t map_id = ((int16_t)data[4] << 8) | (int16_t)data[5];
    int16_t level_id = ((int16_t)data[6] << 8) | (int16_t)data[7];

    float x, y, z, yaw;
    std::memcpy(&x, &data[8], 4);
    std::memcpy(&y, &data[12], 4);
    std::memcpy(&z, &data[16], 4);
    std::memcpy(&yaw, &data[20], 4);

    uint32_t x_int = ((uint32_t)data[8] << 24) | ((uint32_t)data[9] << 16) |
                     ((uint32_t)data[10] << 8) | (uint32_t)data[11];
    uint32_t y_int = ((uint32_t)data[12] << 24) | ((uint32_t)data[13] << 16) |
                     ((uint32_t)data[14] << 8) | (uint32_t)data[15];
    uint32_t z_int = ((uint32_t)data[16] << 24) | ((uint32_t)data[17] << 16) |
                     ((uint32_t)data[18] << 8) | (uint32_t)data[19];
    uint32_t yaw_int = ((uint32_t)data[20] << 24) | ((uint32_t)data[21] << 16) |
                       ((uint32_t)data[22] << 8) | (uint32_t)data[23];

    std::memcpy(&x, &x_int, 4);
    std::memcpy(&y, &y_int, 4);
    std::memcpy(&z, &z_int, 4);
    std::memcpy(&yaw, &yaw_int, 4);

    std::vector<int32_t> params;
    params.push_back((int32_t)target_player_id);
    params.push_back((int32_t)map_id);
    params.push_back((int32_t)level_id);

    int32_t x_as_int, y_as_int, z_as_int, yaw_as_int;
    std::memcpy(&x_as_int, &x, 4);
    std::memcpy(&y_as_int, &y, 4);
    std::memcpy(&z_as_int, &z, 4);
    std::memcpy(&yaw_as_int, &yaw, 4);

    params.push_back(x_as_int);
    params.push_back(y_as_int);
    params.push_back(z_as_int);
    params.push_back(yaw_as_int);

    EnqueueEvent(PacketType::PlayerInfoResponse, "", params, 0);
}

void NetworkClient::HandlePlayerListUpdate(const uint8_t *data, int len)
{
    if (len < 4)
        return;

    uint32_t player_count = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                            ((uint32_t)data[2] << 8) | ((uint32_t)data[3]);

    int offset = 4;

    for (uint32_t i = 0; i < player_count && offset < len; i++)
    {
        if (offset + 8 > len)
            break;

        uint32_t player_id = ((uint32_t)data[offset] << 24) | ((uint32_t)data[offset + 1] << 16) |
                             ((uint32_t)data[offset + 2] << 8) | ((uint32_t)data[offset + 3]);
        offset += 4;

        uint32_t username_len = ((uint32_t)data[offset] << 24) | ((uint32_t)data[offset + 1] << 16) |
                                ((uint32_t)data[offset + 2] << 8) | ((uint32_t)data[offset + 3]);
        offset += 4;

        if (offset + username_len > (uint32_t)len)
            break;

        std::string username((const char *)(data + offset), username_len);
        offset += username_len;

        EnqueueEvent(PacketType::PlayerListUpdate, username, {(int32_t)player_id}, player_id);
    }
}

void NetworkClient::SendPlayerInfoRequest(uint32_t targetPlayerId, uint32_t requesterPlayerId)
{
    uint8_t buffer[8];
    buffer[0] = (targetPlayerId >> 24) & 0xFF;
    buffer[1] = (targetPlayerId >> 16) & 0xFF;
    buffer[2] = (targetPlayerId >> 8) & 0xFF;
    buffer[3] = targetPlayerId & 0xFF;
    buffer[4] = (requesterPlayerId >> 24) & 0xFF;
    buffer[5] = (requesterPlayerId >> 16) & 0xFF;
    buffer[6] = (requesterPlayerId >> 8) & 0xFF;
    buffer[7] = requesterPlayerId & 0xFF;

    SendRawPacket(PacketType::PlayerInfoRequest, buffer, 8);
}

void NetworkClient::SendPlayerInfoResponse(uint32_t targetPlayerId, int16_t mapId, int16_t levelId,
                                           float x, float y, float z, float yaw)
{
    uint8_t buffer[24];

    buffer[0] = (targetPlayerId >> 24) & 0xFF;
    buffer[1] = (targetPlayerId >> 16) & 0xFF;
    buffer[2] = (targetPlayerId >> 8) & 0xFF;
    buffer[3] = targetPlayerId & 0xFF;

    buffer[4] = (mapId >> 8) & 0xFF;
    buffer[5] = mapId & 0xFF;

    buffer[6] = (levelId >> 8) & 0xFF;
    buffer[7] = levelId & 0xFF;

    uint32_t x_int, y_int, z_int, yaw_int;
    std::memcpy(&x_int, &x, 4);
    std::memcpy(&y_int, &y, 4);
    std::memcpy(&z_int, &z, 4);
    std::memcpy(&yaw_int, &yaw, 4);

    buffer[8] = (x_int >> 24) & 0xFF;
    buffer[9] = (x_int >> 16) & 0xFF;
    buffer[10] = (x_int >> 8) & 0xFF;
    buffer[11] = x_int & 0xFF;

    buffer[12] = (y_int >> 24) & 0xFF;
    buffer[13] = (y_int >> 16) & 0xFF;
    buffer[14] = (y_int >> 8) & 0xFF;
    buffer[15] = y_int & 0xFF;

    buffer[16] = (z_int >> 24) & 0xFF;
    buffer[17] = (z_int >> 16) & 0xFF;
    buffer[18] = (z_int >> 8) & 0xFF;
    buffer[19] = z_int & 0xFF;

    buffer[20] = (yaw_int >> 24) & 0xFF;
    buffer[21] = (yaw_int >> 16) & 0xFF;
    buffer[22] = (yaw_int >> 8) & 0xFF;
    buffer[23] = yaw_int & 0xFF;

    SendRawPacket(PacketType::PlayerInfoResponse, buffer, 24);
}

void NetworkClient::UploadInitialSaveData()
{
}
