
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <chrono>
#include <atomic>
#include "debug_log.h"
#include "lib_packets.hpp"
#include "lib_recomp.hpp"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define CLOSE_SOCKET closesocket
    #define VALID_SOCK(s) ((s) != INVALID_SOCKET)
    #define SOCK_ERR(ret) ((ret) == SOCKET_ERROR)
    #define GET_LAST_ERROR() WSAGetLastError()
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define CLOSE_SOCKET close
    #define VALID_SOCK(s) ((s) >= 0)
    #define SOCK_ERR(ret) ((ret) < 0)
    #define GET_LAST_ERROR() errno
#endif

extern "C" {
    DLLEXPORT uint32_t recomp_api_version = 1;
}


std::atomic<bool> net_busy(false);
SOCKET udp_socket = INVALID_SOCKET;
struct sockaddr_in server_addr;

// config
std::string g_host = "127.0.0.1";
std::string g_user = "Player";
std::string g_lobby = "Default";
std::string g_pass = "";

// state
bool is_connected = false;
bool needs_init = false;
uint32_t last_handshake_time = 0;
uint32_t last_ping_time = 0;
uint32_t last_packet_sent_time = 0;

const uint32_t HANDSHAKE_INTERVAL_MS = 1000;
const uint32_t PING_INTERVAL_MS = 10000;

uint32_t GetClockMS() {
    using namespace std::chrono;
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

#define MSG_TEXT_SIZE 128
#define MSG_DATA_SIZE 32
#define MSG_QUEUE_CAPACITY 16

struct PendingMessage {
    char text[MSG_TEXT_SIZE];
    int text_length;
    
    uint8_t data[MSG_DATA_SIZE];
    int data_length;
    
    int player_id;
    uint8_t msg_type;
    bool active;
};

enum MessageType : uint8_t {
    MSGTYPE_NONE = 0,
    
    MSGTYPE_PLAYER_CONNECTED = 1,
    MSGTYPE_PLAYER_DISCONNECTED = 2,
    
    MSGTYPE_CHAT = 3,
    MSGTYPE_SERVER_INFO = 4,
    
    MSGTYPE_JIGGY_COLLECTED = 10,
    MSGTYPE_NOTE_COLLECTED = 11,
    MSGTYPE_HONEYCOMB_COLLECTED = 12,
    MSGTYPE_MUMBO_TOKEN_COLLECTED = 13,
    MSGTYPE_JINJO_COLLECTED = 14,
    
    MSGTYPE_PLAYER_POSITION = 20,
    MSGTYPE_PLAYER_TRANSFORM = 21,
};

static PendingMessage msg_queue[MSG_QUEUE_CAPACITY];
static int msg_queue_head = 0;
static int msg_queue_tail = 0;
static int msg_queue_count = 0;

static PendingMessage current_msg = { };

static void QueueTextMessage(MessageType type, const char* text, int player_id = -1) {
    if (msg_queue_count >= MSG_QUEUE_CAPACITY) {
        msg_queue_tail = (msg_queue_tail + 1) % MSG_QUEUE_CAPACITY;
        msg_queue_count--;
        debug_log("[MsgQueue] Overflow - dropped oldest message");
    }

    PendingMessage& msg = msg_queue[msg_queue_head];
    msg.msg_type = type;
    msg.player_id = player_id;
    msg.active = true;
    msg.data_length = 0;

    int i = 0;
    while (text[i] != '\0' && i < MSG_TEXT_SIZE - 1) {
        msg.text[i] = text[i];
        i++;
    }
    msg.text[i] = '\0';
    msg.text_length = i;

    msg_queue_head = (msg_queue_head + 1) % MSG_QUEUE_CAPACITY;
    msg_queue_count++;

    debug_log("[MsgQueue] Text queued: type=" + std::to_string(type) + " text=" + std::string(text));
}

static void QueueDataMessage(MessageType type, const void* data, int data_len, int player_id = -1) {
    if (msg_queue_count >= MSG_QUEUE_CAPACITY) {
        msg_queue_tail = (msg_queue_tail + 1) % MSG_QUEUE_CAPACITY;
        msg_queue_count--;
        debug_log("[MsgQueue] Overflow - dropped oldest message");
    }

    PendingMessage& msg = msg_queue[msg_queue_head];
    msg.msg_type = type;
    msg.player_id = player_id;
    msg.active = true;
    msg.text_length = 0;
    msg.text[0] = '\0';

    int copy_len = (data_len > MSG_DATA_SIZE) ? MSG_DATA_SIZE : data_len;
    std::memcpy(msg.data, data, copy_len);
    msg.data_length = copy_len;

    msg_queue_head = (msg_queue_head + 1) % MSG_QUEUE_CAPACITY;
    msg_queue_count++;

    debug_log("[MsgQueue] Data queued: type=" + std::to_string(type) + " len=" + std::to_string(data_len));
}

static void QueueFullMessage(MessageType type, const char* text, const void* data, int data_len, int player_id = -1) {
    if (msg_queue_count >= MSG_QUEUE_CAPACITY) {
        msg_queue_tail = (msg_queue_tail + 1) % MSG_QUEUE_CAPACITY;
        msg_queue_count--;
    }

    PendingMessage& msg = msg_queue[msg_queue_head];
    msg.msg_type = type;
    msg.player_id = player_id;
    msg.active = true;

    int i = 0;
    while (text[i] != '\0' && i < MSG_TEXT_SIZE - 1) {
        msg.text[i] = text[i];
        i++;
    }
    msg.text[i] = '\0';
    msg.text_length = i;

    int copy_len = (data_len > MSG_DATA_SIZE) ? MSG_DATA_SIZE : data_len;
    std::memcpy(msg.data, data, copy_len);
    msg.data_length = copy_len;

    msg_queue_head = (msg_queue_head + 1) % MSG_QUEUE_CAPACITY;
    msg_queue_count++;
}

RECOMP_DLL_FUNC(net_msg_poll) {
    if (msg_queue_count == 0) {
        current_msg.active = false;
        RECOMP_RETURN(int, 0);
        return;
    }

    current_msg = msg_queue[msg_queue_tail];
    msg_queue_tail = (msg_queue_tail + 1) % MSG_QUEUE_CAPACITY;
    msg_queue_count--;

    uint32_t result = ((uint32_t)current_msg.msg_type << 24) | (current_msg.player_id & 0x00FFFFFF);
    RECOMP_RETURN(int, (int)result);
    return;
}

RECOMP_DLL_FUNC(net_msg_get_text_len) {
    if (!current_msg.active) {
        RECOMP_RETURN(int, 0);
        return;
    }
    RECOMP_RETURN(int, current_msg.text_length);
    return;
}

RECOMP_DLL_FUNC(net_msg_get_text_chunk) {
    int offset = RECOMP_ARG(int, 0);

    if (!current_msg.active || offset < 0 || offset >= MSG_TEXT_SIZE) {
        RECOMP_RETURN(int, 0);
        return;
    }

    uint32_t packed = 0;
    for (int i = 0; i < 4; i++) {
        int idx = offset + i;
        char c = (idx < current_msg.text_length) ? current_msg.text[idx] : 0;
        packed |= ((uint32_t)(uint8_t)c) << (i * 8);
    }

    RECOMP_RETURN(int, (int)packed);
    return;
}

RECOMP_DLL_FUNC(net_msg_get_data_len) {
    if (!current_msg.active) {
        RECOMP_RETURN(int, 0);
        return;
    }
    RECOMP_RETURN(int, current_msg.data_length);
    return;
}

RECOMP_DLL_FUNC(net_msg_get_data_chunk) {
    int offset = RECOMP_ARG(int, 0);

    if (!current_msg.active || offset < 0 || offset >= MSG_DATA_SIZE) {
        RECOMP_RETURN(int, 0);
        return;
    }

    uint32_t packed = 0;
    for (int i = 0; i < 4; i++) {
        int idx = offset + i;
        uint8_t b = (idx < current_msg.data_length) ? current_msg.data[idx] : 0;
        packed |= ((uint32_t)b) << (i * 8);
    }

    RECOMP_RETURN(int, (int)packed);
    return;
}

RECOMP_DLL_FUNC(net_msg_get_data_int) {
    int index = RECOMP_ARG(int, 0);
    int byte_offset = index * 4;

    if (!current_msg.active || byte_offset < 0 || byte_offset + 4 > current_msg.data_length) {
        RECOMP_RETURN(int, 0);
        return;
    }

    int32_t value;
    std::memcpy(&value, &current_msg.data[byte_offset], sizeof(int32_t));
    RECOMP_RETURN(int, value);
    return;
}

RECOMP_DLL_FUNC(net_msg_consume) {
    current_msg.active = false;
    current_msg.text_length = 0;
    current_msg.data_length = 0;
    RECOMP_RETURN(int, 1);
    return;
}

RECOMP_DLL_FUNC(net_init) {
    net_busy = true;
    
    #ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    #endif

    if (VALID_SOCK(udp_socket)) {
        CLOSE_SOCKET(udp_socket);
        udp_socket = INVALID_SOCKET;
    }
    is_connected = false;

    msg_queue_head = 0;
    msg_queue_tail = 0;
    msg_queue_count = 0;
    current_msg.active = false;
    
    last_handshake_time = 0;
    last_ping_time = 0;
    last_packet_sent_time = 0;

    try {
        g_host = RECOMP_ARG_STR(0);
        g_user = RECOMP_ARG_STR(1);
        g_lobby = RECOMP_ARG_STR(2);
        g_pass = RECOMP_ARG_STR(3);
    } catch (...) {}

    debug_log("[Net] Config Stored. Target: " + g_host);
    needs_init = true; 
    
    net_busy = false;
    RECOMP_RETURN(int, 1);
    return;
}

void SendRawPacket(PacketType type, const void* data, size_t size) {
    if (!VALID_SOCK(udp_socket)) return;

    std::vector<uint8_t> buffer(1 + size);
    buffer[0] = static_cast<uint8_t>(type);
    
    if (size > 0 && data != nullptr) {
        std::memcpy(&buffer[1], data, size);
    }

    int sent = sendto(udp_socket, (const char*)buffer.data(), (int)buffer.size(), 0, 
                      (struct sockaddr*)&server_addr, sizeof(server_addr));
    
    if (!SOCK_ERR(sent)) {
        last_packet_sent_time = GetClockMS();
    }
}

void SendPing() {
    SendRawPacket(PacketType::Ping, nullptr, 0);
    debug_log("[Net] Ping sent");
}

bool PerformLazyInit() {
    debug_log("[Net] Setting up UDP Socket...");

    udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (!VALID_SOCK(udp_socket)) {
        debug_log("[Net] Socket Failed!");
        return false;
    }

    #ifdef _WIN32
        u_long mode = 1;
        ioctlsocket(udp_socket, FIONBIO, &mode);
    #else
        int flags = fcntl(udp_socket, F_GETFL, 0);
        fcntl(udp_socket, F_SETFL, flags | O_NONBLOCK);
    #endif

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8756);
    inet_pton(AF_INET, g_host.c_str(), &server_addr.sin_addr);

    debug_log("[Net] Socket Ready.");
    return true;
}

static void HandlePlayerConnected(const uint8_t* data, int len) {
    try {
        msgpack::object_handle oh = msgpack::unpack((const char*)data, len);
        msgpack::object obj = oh.get();

        if (obj.type == msgpack::type::ARRAY && obj.via.array.size >= 2) {
            std::string username = obj.via.array.ptr[0].as<std::string>();
            int player_id = obj.via.array.ptr[1].as<int>();

            debug_log("[Net] Player Connected: " + username + " (ID: " + std::to_string(player_id) + ")");
            QueueTextMessage(MSGTYPE_PLAYER_CONNECTED, username.c_str(), player_id);
        }
    } catch (const std::exception& e) {
        debug_log("[Net] Failed to parse PlayerConnected: " + std::string(e.what()));
    }
}

static void HandlePlayerDisconnected(const uint8_t* data, int len) {
    try {
        msgpack::object_handle oh = msgpack::unpack((const char*)data, len);
        msgpack::object obj = oh.get();

        if (obj.type == msgpack::type::ARRAY && obj.via.array.size >= 2) {
            std::string username = obj.via.array.ptr[0].as<std::string>();
            int player_id = obj.via.array.ptr[1].as<int>();

            debug_log("[Net] Player Disconnected: " + username + " (ID: " + std::to_string(player_id) + ")");
            QueueTextMessage(MSGTYPE_PLAYER_DISCONNECTED, username.c_str(), player_id);
        }
    } catch (const std::exception& e) {
        debug_log("[Net] Failed to parse PlayerDisconnected: " + std::string(e.what()));
    }
}

static void HandleJiggyCollected(const uint8_t* data, int len) {
    try {
        msgpack::object_handle oh = msgpack::unpack((const char*)data, len);
        msgpack::object obj = oh.get();

        if (obj.type == msgpack::type::ARRAY && obj.via.array.size >= 2) {
            int level_id = obj.via.array.ptr[0].as<int>();
            int jiggy_id = obj.via.array.ptr[1].as<int>();
            
            int32_t payload[2] = { level_id, jiggy_id };
            
            if (obj.via.array.size >= 3) {
                std::string collector = obj.via.array.ptr[2].as<std::string>();
                QueueFullMessage(MSGTYPE_JIGGY_COLLECTED, collector.c_str(), payload, sizeof(payload));
            } else {
                QueueDataMessage(MSGTYPE_JIGGY_COLLECTED, payload, sizeof(payload));
            }

            debug_log("[Net] Jiggy Collected: Level=" + std::to_string(level_id) + " Jiggy=" + std::to_string(jiggy_id));
        }
    } catch (const std::exception& e) {
        debug_log("[Net] Failed to parse JiggyCollected: " + std::string(e.what()));
    }
}

static void HandlePong(const uint8_t* data, int len) {
    
}

RECOMP_DLL_FUNC(net_update) {
    if (net_busy) { RECOMP_RETURN(int, 4); return; }

    if (needs_init) {
        if (!PerformLazyInit()) {
            needs_init = false;
            RECOMP_RETURN(int, 0); return;
        }
        needs_init = false;
    }

    if (!VALID_SOCK(udp_socket)) { RECOMP_RETURN(int, 0); return; }

    uint32_t now = GetClockMS();

    if (!is_connected && (now - last_handshake_time > HANDSHAKE_INTERVAL_MS)) {
        debug_log("[Net] Sending Handshake...");
        
        LoginPacket login;
        login.Username = g_user;
        login.LobbyName = g_lobby;
        login.Password = g_pass;
        msgpack::sbuffer sbuf;
        msgpack::pack(sbuf, login);
        
        SendRawPacket(PacketType::Handshake, sbuf.data(), sbuf.size());
        last_handshake_time = now;
    }

    if (is_connected) {
        uint32_t time_since_last_send = now - last_packet_sent_time;
        
        if (time_since_last_send > PING_INTERVAL_MS) {
            SendPing();
            last_ping_time = now;
        }
    }

    struct sockaddr_in from;
    int fromLen = sizeof(from);
    uint8_t buf[2048]; 

    while (true) {
        int len = recvfrom(udp_socket, (char*)buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromLen);
        
        if (len > 0) {
            uint8_t type = buf[0];
            uint8_t* payload = &buf[1];
            int payload_len = len - 1;
            
            if (!is_connected) {
                is_connected = true;
                last_packet_sent_time = now;
                debug_log("[Net] Connected! Server Responded.");
            }

            switch (type) {
                case static_cast<uint8_t>(PacketType::PlayerConnected):
                    HandlePlayerConnected(payload, payload_len);
                    break;
                    
                case static_cast<uint8_t>(PacketType::PlayerDisconnected):
                    HandlePlayerDisconnected(payload, payload_len);
                    break;
                    
                case static_cast<uint8_t>(PacketType::JiggyCollected):
                    HandleJiggyCollected(payload, payload_len);
                    break;
                    
                case static_cast<uint8_t>(PacketType::Pong):
                    HandlePong(payload, payload_len);
                    break;
                    
                default:
                    debug_log("[Net] Unknown packet type: " + std::to_string(type));
                    break;
            }
            
        } else {
            break;
        }
    }

    if (is_connected) {
        RECOMP_RETURN(int, 2);
    } else {
        RECOMP_RETURN(int, 1);
    }
    return;
}

RECOMP_DLL_FUNC(net_send_jiggy) {
    int levelId = RECOMP_ARG(int, 0);
    int jiggyId = RECOMP_ARG(int, 1);

    JiggyPacket pak;
    pak.LevelId = levelId;
    pak.JiggyId = jiggyId;
    
    msgpack::sbuffer sbuf;
    msgpack::pack(sbuf, pak);
        
    SendRawPacket(PacketType::JiggyCollected, sbuf.data(), sbuf.size());
    RECOMP_RETURN(int, 1);
    return;
}

RECOMP_DLL_FUNC(native_lib_test) {
    RECOMP_RETURN(int, 0);
}

RECOMP_DLL_FUNC(net_test_udp) {
    debug_log("[Test] Running Raw UDP Test...");
    
    if (!VALID_SOCK(udp_socket)) {
        debug_log("[Test] Socket invalid.");
        RECOMP_RETURN(int, 0);
    }
    
    const char* msg = "HELLO_SERVER";
    int sent = sendto(udp_socket, msg, 12, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
    
    if (SOCK_ERR(sent)) {
        debug_log("[Test] Failed. Error: " + std::to_string(GET_LAST_ERROR()));
        RECOMP_RETURN(int, 0);
    }
    
    debug_log("[Test] Success! Sent 12 bytes.");
    RECOMP_RETURN(int, 1);
}

RECOMP_DLL_FUNC(net_send_pos) {
    RECOMP_RETURN(int, 0);
}