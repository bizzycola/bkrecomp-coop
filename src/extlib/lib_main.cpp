// =========================================================================== //
// Backend code library.
// This file includes networking and debug logging code that
// has to be run using host resources.
//
// This library is compiled to a .dll/.so and loaded by our recomp mod.
// =========================================================================== //

#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <chrono>
#include <atomic>
#include "debug_log.h"
#include "lib_packets.h"
#include "lib_recomp.hpp"
#include "lib_net.h"
#include "lib_message_queue.h"

extern "C"
{
    DLLEXPORT uint32_t recomp_api_version = 1;
}

static NetworkClient *g_networkClient = nullptr;

RECOMP_DLL_FUNC(native_lib_test)
{
    RECOMP_RETURN(int, 0);
}

RECOMP_DLL_FUNC(native_poll_message)
{
    PTR(GameMessage)
    buffer_ptr = RECOMP_ARG(PTR(GameMessage), 0);

    if (!buffer_ptr)
    {
        RECOMP_RETURN(int, 0);
    }

    GameMessage msg;
    if (g_messageQueue.Pop(msg))
    {
        MEM_B(0, buffer_ptr) = msg.type;
        MEM_W(4, buffer_ptr) = msg.playerId;
        MEM_W(8, buffer_ptr) = msg.param1;
        MEM_W(12, buffer_ptr) = msg.param2;
        MEM_W(16, buffer_ptr) = msg.param3;
        MEM_W(20, buffer_ptr) = msg.param4;

        uint32_t f1_bits, f2_bits, f3_bits;
        memcpy(&f1_bits, &msg.paramF1, sizeof(float));
        memcpy(&f2_bits, &msg.paramF2, sizeof(float));
        memcpy(&f3_bits, &msg.paramF3, sizeof(float));
        MEM_W(24, buffer_ptr) = f1_bits;
        MEM_W(28, buffer_ptr) = f2_bits;
        MEM_W(32, buffer_ptr) = f3_bits;

        MEM_H(36, buffer_ptr) = msg.dataSize;

        for (size_t i = 0; i < MAX_MESSAGE_DATA_SIZE; i++)
        {
            MEM_B(38 + i, buffer_ptr) = msg.data[i];
        }

        RECOMP_RETURN(int, 1);
    }

    RECOMP_RETURN(int, 0);
}

RECOMP_DLL_FUNC(native_connect_to_server)
{
    std::string host = RECOMP_ARG_STR(0);
    std::string username = RECOMP_ARG_STR(1);
    std::string lobby = RECOMP_ARG_STR(2);
    std::string password = RECOMP_ARG_STR(3);

    if (g_networkClient == nullptr)
    {
        g_networkClient = new NetworkClient();
    }

    GameMessage connectingMsg = CreateConnectionStatusMsg("Connecting to server...");
    g_messageQueue.Push(connectingMsg);

    g_networkClient->Configure(host, username, lobby, password);

    RECOMP_RETURN(int, 1);
}

RECOMP_DLL_FUNC(native_update_network)
{
    if (g_networkClient != nullptr)
    {
        g_networkClient->Update();
    }

    RECOMP_RETURN(int, 0);
}

RECOMP_DLL_FUNC(native_disconnect_from_server)
{
    debug_log("[lib_main] native_disconnect_from_server called");

    if (g_networkClient != nullptr)
    {
        debug_log("[lib_main] Deleting NetworkClient");
        delete g_networkClient;
        g_networkClient = nullptr;

        GameMessage disconnectedMsg = CreateConnectionStatusMsg("Disconnected from server");
        g_messageQueue.Push(disconnectedMsg);
        debug_log("[lib_main] Pushed 'Disconnected' message to queue");
    }
    else
    {
        debug_log("[lib_main] NetworkClient was already null");
    }

    RECOMP_RETURN(int, 1);
}

void EnqueueExampleMessages()
{
    GameMessage playerConnected = CreatePlayerConnectedMsg(42, "TestPlayer");
    g_messageQueue.Push(playerConnected);

    GameMessage jiggyMsg = CreateJiggyCollectedMsg(42, 1, 5);
    g_messageQueue.Push(jiggyMsg);

    GameMessage puppetMsg = CreatePuppetUpdateMsg(42, 100.0f, 200.0f, 300.0f,
                                                  0.0f, 0.0f, 0.0f, 1, 1);
    g_messageQueue.Push(puppetMsg);
}
