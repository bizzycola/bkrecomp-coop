#pragma once

#include <string>
#include <vector>
#include <queue>
#include <atomic>
#include <mutex>
#include <memory>
#include <functional>

// Include the packet definitions provided previously
#include "lib_packets.h"

// Platform specific socket includes
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCK_ERR(ret) ((ret) < 0)
#endif

struct NetEvent {
    PacketType type;
    int playerId;
    std::string textData;
    std::vector<int32_t> intData;
};

class NetworkClient {
private:
    SOCKET m_udpSocket;
    struct sockaddr_in m_serverAddr;
    bool m_isConnected;
    bool m_needsInit;
    
    std::string m_host;
    std::string m_user;
    std::string m_lobby;
    std::string m_pass;

    uint32_t m_lastHandshakeTime;
    uint32_t m_lastPingTime;
    uint32_t m_lastPacketSentTime;

    std::queue<NetEvent> m_eventQueue;
    std::mutex m_queueMutex;

    uint32_t GetClockMS();
    bool PerformLazyInit();
    void SendRawPacket(PacketType type, const void* data, size_t size);
    void SendPing();
    void HandlePlayerConnected(const uint8_t* data, int len);
    void HandlePlayerDisconnected(const uint8_t* data, int len);
    void HandleJiggyCollected(const uint8_t* data, int len);
    void HandleNoteCollected(const uint8_t* data, int len);
    void HandleNoteCollectedPos(const uint8_t* data, int len);
    void HandleNoteSaveData(const uint8_t* data, int len);
    void HandlePuppetUpdate(const uint8_t* data, int len);
    void HandleLevelOpened(const uint8_t* data, int len);
    void EnqueueEvent(PacketType type, const std::string& text, const std::vector<int32_t>& data, int playerId = -1);

public:
    NetworkClient();
    ~NetworkClient();
    void Configure(const std::string& host, const std::string& user, const std::string& lobby, const std::string& pass);
    void Update();
    bool HasEvents();
    NetEvent PopEvent();
    void SendJiggy(int jiggyEnumId, int collectedValue);
    void SendNote(int mapId, int levelId, bool isDynamic, int noteIndex);
    void SendNotePos(int mapId, int x, int y, int z);
    void SendNoteSaveData(int levelIndex, const std::vector<uint8_t>& saveData);
    void SendLevelOpened(int worldId, int jiggyCost);
    void SendPuppetUpdate(const PuppetUpdatePacket& packet);
    void RequestFullSync();
};
