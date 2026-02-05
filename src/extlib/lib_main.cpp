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
#if defined(_WIN32)
#include <windows.h>
#endif
#include "debug_log.h"
#include "lib_packets.h"
#include "lib_recomp.hpp"
#include "lib_net.h"
#include "lib_message_queue.h"
#include "console_input.h"
#include "util/util.h"

void coop_dll_log(const char *msg)
{
    if (!msg)
        return;
#if defined(_WIN32)
    OutputDebugStringA(msg);
    OutputDebugStringA("\n");
#endif

    FILE *f = fopen("bkrecomp_coop_extlib.log", "a");
    if (f)
    {
        fprintf(f, "%s\n", msg);
        fclose(f);
    }
}

#if defined(_WIN32)
// tl;dr used to log exceptions when the lib crashes
// only enable in the next function if needed
static LONG WINAPI coop_vectored_exception_handler(struct _EXCEPTION_POINTERS *ExceptionInfo)
{
    if (!ExceptionInfo || !ExceptionInfo->ExceptionRecord)
        return EXCEPTION_CONTINUE_SEARCH;

    const DWORD code = ExceptionInfo->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_ILLEGAL_INSTRUCTION || code == EXCEPTION_STACK_OVERFLOW)
    {
        void *ip = (void *)ExceptionInfo->ExceptionRecord->ExceptionAddress;
        void *addr = (ExceptionInfo->ExceptionRecord->NumberParameters >= 2) ? (void *)ExceptionInfo->ExceptionRecord->ExceptionInformation[1] : nullptr;
        unsigned long long av_kind = 0;
        if (code == EXCEPTION_ACCESS_VIOLATION && ExceptionInfo->ExceptionRecord->NumberParameters >= 1)
        {
            av_kind = (unsigned long long)ExceptionInfo->ExceptionRecord->ExceptionInformation[0];
        }

        auto describe_ptr = [](void *p, char *out, size_t out_sz)
        {
            if (!out || out_sz == 0)
                return;
            out[0] = '\0';
            if (!p)
            {
                snprintf(out, out_sz, "(null)");
                return;
            }

            HMODULE mod = nullptr;
            char path[MAX_PATH];
            path[0] = '\0';
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   (LPCSTR)p, &mod) &&
                mod)
            {
                GetModuleFileNameA(mod, path, MAX_PATH);
                uintptr_t base = (uintptr_t)mod;
                uintptr_t off = (uintptr_t)p - base;
                const char *file = path;
                for (const char *c = path; *c; ++c)
                    if (*c == '\\' || *c == '/')
                        file = c + 1;
                snprintf(out, out_sz, "%s+0x%llX", file, (unsigned long long)off);
            }
            else
            {
                snprintf(out, out_sz, "(no module)");
            }
        };

        char ip_mod[128];
        char addr_mod[128];
        describe_ptr(ip, ip_mod, sizeof(ip_mod));
        describe_ptr(addr, addr_mod, sizeof(addr_mod));

        const char *av_kind_str = "";
        if (code == EXCEPTION_ACCESS_VIOLATION)
        {
            if (av_kind == 0)
                av_kind_str = "read";
            else if (av_kind == 1)
                av_kind_str = "write";
            else if (av_kind == 8)
                av_kind_str = "execute";
            else
                av_kind_str = "unknown";
        }

        char addr_vq[220];
        addr_vq[0] = '\0';
        if (addr)
        {
            MEMORY_BASIC_INFORMATION mbi;
            SIZE_T q = VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi));
            if (q == sizeof(mbi))
            {
                snprintf(addr_vq, sizeof(addr_vq),
                         " vq{base=%p allocBase=%p size=%llu state=0x%lX prot=0x%lX type=0x%lX}",
                         mbi.BaseAddress,
                         mbi.AllocationBase,
                         (unsigned long long)mbi.RegionSize,
                         (unsigned long)mbi.State,
                         (unsigned long)mbi.Protect,
                         (unsigned long)mbi.Type);
            }
            else
            {
                snprintf(addr_vq, sizeof(addr_vq), " vq{fail}");
            }
        }

        char buf[640];
        if (code == EXCEPTION_ACCESS_VIOLATION)
        {
            snprintf(buf, sizeof(buf), "[COOP][DLL] VEH: AV %s code=0x%08lX ip=%p (%s) addr=%p (%s)%s",
                     av_kind_str,
                     (unsigned long)code,
                     ip,
                     ip_mod,
                     addr,
                     addr_mod,
                     addr_vq);
        }
        else
        {
            snprintf(buf, sizeof(buf), "[COOP][DLL] VEH: exception code=0x%08lX ip=%p (%s) addr=%p (%s)%s",
                     (unsigned long)code,
                     ip,
                     ip_mod,
                     addr,
                     addr_mod,
                     addr_vq);
        }
        coop_dll_log(buf);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static void coop_install_vectored_handler_once()
{
    // static std::atomic<int> installed{0};
    // int expected = 0;
    //
    // if (!installed.compare_exchange_strong(expected, 1))
    //     return;
    //
    // PVOID h = AddVectoredExceptionHandler(1, coop_vectored_exception_handler);
    // char buf[128];
    // snprintf(buf, sizeof(buf), "[COOP][DLL] VEH: installed handler=%p", h);
    //
    // coop_dll_log(buf);
}
#endif

#if defined(_WIN32)
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    (void)hModule;
    (void)lpReserved;
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        OutputDebugStringA("[COOP][DLL] DllMain: PROCESS_ATTACH\n");
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH)
    {
        OutputDebugStringA("[COOP][DLL] DllMain: PROCESS_DETACH\n");
    }
    return TRUE;
}
#endif

extern "C"
{
    DLLEXPORT uint32_t recomp_api_version = 1;
}

static NetworkClient *g_networkClient = nullptr;
static int g_connect_state = 0;

// Forward declaration for util
uint8_t PacketTypeToMessageType(PacketType packetType);

RECOMP_DLL_FUNC(native_lib_test)
{
#if defined(_WIN32)
    coop_install_vectored_handler_once();
#endif
    coop_dll_log("[COOP][DLL] native_lib_test: called");
    RECOMP_RETURN(int, 0);
}

// polls for network messages
RECOMP_DLL_FUNC(net_msg_poll)
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
        util::SerializeGameMessageToMemory(rdram, msg, buffer_ptr);
        RECOMP_RETURN(int, 1);
    }

    RECOMP_RETURN(int, 0);
}

RECOMP_DLL_FUNC(native_connect_to_server)
{
#if defined(_WIN32)
    coop_install_vectored_handler_once();
#endif
    coop_dll_log("[COOP][DLL] native_connect_to_server: enter");

    {
        char buf[256];
        snprintf(buf, sizeof(buf), "[COOP][DLL] native_connect_to_server: args rdram=%p ctx=%p", (void *)rdram, (void *)ctx);
        coop_dll_log(buf);

#if defined(_WIN32)
        if (ctx)
        {
            MEMORY_BASIC_INFORMATION mbi;
            SIZE_T q = VirtualQuery((LPCVOID)ctx, &mbi, sizeof(mbi));
            if (q != 0 && mbi.State == MEM_COMMIT && (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) == 0)
            {
                const uint32_t *words = (const uint32_t *)ctx;
                snprintf(buf, sizeof(buf), "[COOP][DLL] native_connect_to_server: ctx words[0..7]=%08X %08X %08X %08X %08X %08X %08X %08X",
                         (unsigned)words[0], (unsigned)words[1], (unsigned)words[2], (unsigned)words[3],
                         (unsigned)words[4], (unsigned)words[5], (unsigned)words[6], (unsigned)words[7]);
                coop_dll_log(buf);
            }
            else
            {
                snprintf(buf, sizeof(buf), "[COOP][DLL] native_connect_to_server: ctx not readable q=%llu state=0x%lX prot=0x%lX",
                         (unsigned long long)q, (unsigned long)mbi.State, (unsigned long)mbi.Protect);
                coop_dll_log(buf);
            }
        }
#endif
    }

#if defined(_WIN32)
    {
        char env_buf[8];
        DWORD n = GetEnvironmentVariableA("COOP_CONNECT_NOOP", env_buf, (DWORD)sizeof(env_buf));
        if (n > 0 && env_buf[0] == '1')
        {
            coop_dll_log("[COOP][DLL] native_connect_to_server: COOP_CONNECT_NOOP=1 => returning success without doing anything");
            RECOMP_RETURN(int, 1);
        }
    }
#endif

    if (g_connect_state != 0)
    {
        coop_dll_log("[COOP][DLL] native_connect_to_server: already connecting/connected (ignored)");
        RECOMP_RETURN(int, 1);
    }
    g_connect_state = 1;

    std::string host = RECOMP_ARG_STR(0);
    std::string username = RECOMP_ARG_STR(1);
    std::string lobby = RECOMP_ARG_STR(2);
    std::string password = RECOMP_ARG_STR(3);

    if (host.empty() || username.empty())
    {
        coop_dll_log("[COOP][DLL] native_connect_to_server: empty host/user");
        GameMessage err = CreateConnectionErrorMsg("Invalid connect args (empty host/user)");
        g_messageQueue.Push(err);
        g_connect_state = 0;
        RECOMP_RETURN(int, 0);
    }

    if (g_networkClient == nullptr)
    {
        coop_dll_log("[COOP][DLL] native_connect_to_server: creating NetworkClient");
        g_networkClient = new NetworkClient();
    }

    GameMessage connectingMsg = CreateConnectionStatusMsg("Connecting to server...");
    g_messageQueue.Push(connectingMsg);

    coop_dll_log("[COOP][DLL] native_connect_to_server: calling Configure");
    g_networkClient->Configure(host, username, lobby, password);

    coop_dll_log("[COOP][DLL] native_connect_to_server: exit ok");

    RECOMP_RETURN(int, 1);
}

uint8_t PacketTypeToMessageType(PacketType packetType)
{
    switch (packetType)
    {
    case PacketType::PlayerConnected:
        return (uint8_t)MessageType::PLAYER_CONNECTED;

    case PacketType::PlayerDisconnected:
        return (uint8_t)MessageType::PLAYER_DISCONNECTED;

    case PacketType::JiggyCollected:
        return (uint8_t)MessageType::JIGGY_COLLECTED;

    case PacketType::NoteCollected:
        return (uint8_t)MessageType::NOTE_COLLECTED;

    case PacketType::PuppetUpdate:
        return (uint8_t)MessageType::PUPPET_UPDATE;

    case PacketType::LevelOpened:
        return (uint8_t)MessageType::LEVEL_OPENED;

    case PacketType::NoteSaveData:
        return (uint8_t)MessageType::NOTE_SAVE_DATA;

    case PacketType::InitialSaveDataRequest:
        return (uint8_t)MessageType::INITIAL_SAVE_DATA_REQUEST;

    case PacketType::FileProgressFlags:
        return (uint8_t)MessageType::FILE_PROGRESS_FLAGS;

    case PacketType::AbilityProgress:
        return (uint8_t)MessageType::ABILITY_PROGRESS;

    case PacketType::HoneycombScore:
        return (uint8_t)MessageType::HONEYCOMB_SCORE;

    case PacketType::MumboScore:
        return (uint8_t)MessageType::MUMBO_SCORE;

    case PacketType::HoneycombCollected:
        return (uint8_t)MessageType::HONEYCOMB_COLLECTED;

    case PacketType::MumboTokenCollected:
        return (uint8_t)MessageType::MUMBO_TOKEN_COLLECTED;

    case PacketType::PlayerInfoRequest:
        return (uint8_t)MessageType::PLAYER_INFO_REQUEST;

    case PacketType::PlayerInfoResponse:
        return (uint8_t)MessageType::PLAYER_INFO_RESPONSE;

    case PacketType::PlayerListUpdate:
        return (uint8_t)MessageType::PLAYER_LIST_UPDATE;

    default:
        return 0;
    }
}

static void push_honeycomb_collected(const HoneycombCollectedPacket &p)
{
    GameMessage msg;
    msg.type = (uint8_t)MessageType::HONEYCOMB_COLLECTED;
    msg.playerId = 0;
    msg.param1 = p.MapId;
    msg.param2 = p.HoneycombId;
    msg.param3 = p.X;
    msg.param4 = p.Y;
    msg.param5 = p.Z;
    msg.dataSize = 0;
    g_messageQueue.Push(msg);
}

static void push_mumbo_token_collected(const MumboTokenCollectedPacket &p)
{
    GameMessage msg;
    msg.type = (uint8_t)MessageType::MUMBO_TOKEN_COLLECTED;
    msg.playerId = 0;
    msg.param1 = p.MapId;
    msg.param2 = p.TokenId;
    msg.param3 = p.X;
    msg.param4 = p.Y;
    msg.param5 = p.Z;
    msg.dataSize = 0;
    g_messageQueue.Push(msg);
}

// handle network updates (packet receive)
RECOMP_DLL_FUNC(native_update_network)
{
    if (g_networkClient != nullptr)
    {
        g_networkClient->Update();

        while (g_networkClient->HasEvents())
        {
            NetEvent evt = g_networkClient->PopEvent();
            GameMessage msg;
            util::ConvertNetEventToGameMessage(evt, msg);
            g_messageQueue.Push(msg);
        }
    }

    RECOMP_RETURN(int, 0);
}

// closes connection and disconnects
RECOMP_DLL_FUNC(native_disconnect_from_server)
{
    if (g_networkClient != nullptr)
    {
        delete g_networkClient;
        g_networkClient = nullptr;

        GameMessage disconnectedMsg = CreateConnectionStatusMsg("Disconnected from server");
        g_messageQueue.Push(disconnectedMsg);
    }

    RECOMP_RETURN(int, 1);
}

// send a collected jiggy
RECOMP_DLL_FUNC(native_sync_jiggy)
{
    int jiggyEnumId = RECOMP_ARG(int, 0);
    int collectedValue = RECOMP_ARG(int, 1);

    if (g_networkClient != nullptr)
    {
        g_networkClient->SendJiggy(jiggyEnumId, collectedValue);
    }

    RECOMP_RETURN(int, 1);
}

extern "C" DLLEXPORT int native_get_note_save_data(int levelIndex, unsigned char *outBuf, int outBufSize)
{
    (void)levelIndex;
    if (outBuf == nullptr || outBufSize < 32)
    {
        return 0;
    }

    return 0;
}

// send collected note
RECOMP_DLL_FUNC(native_sync_note)
{
    int mapId = RECOMP_ARG(int, 0);
    int levelId = RECOMP_ARG(int, 1);
    bool isDynamic = RECOMP_ARG(bool, 2);
    int noteIndex = RECOMP_ARG(int, 3);

    if (g_networkClient != nullptr)
    {
        g_networkClient->SendNote(mapId, levelId, isDynamic, noteIndex);
    }

    RECOMP_RETURN(int, 1);
}

// send when a level was opened
RECOMP_DLL_FUNC(native_send_level_opened)
{
    coop_dll_log("[COOP][DLL] native_send_level_opened: enter");
    int worldId = RECOMP_ARG(int, 0);
    int jiggyCost = RECOMP_ARG(int, 1);

    if (g_networkClient != nullptr)
    {
        g_networkClient->SendLevelOpened(worldId, jiggyCost);
    }

    coop_dll_log("[COOP][DLL] native_send_level_opened: exit");
    RECOMP_RETURN(int, 1);
}

// uploads initial save data
// this is called for the first player to join a lobby
// to send an initial state which can be synced to other players
RECOMP_DLL_FUNC(native_upload_initial_save_data)
{
    coop_dll_log("[COOP][DLL] native_upload_initial_save_data: enter");
    if (g_networkClient != nullptr)
    {
        g_networkClient->UploadInitialSaveData();
    }

    coop_dll_log("[COOP][DLL] native_upload_initial_save_data: exit");
    RECOMP_RETURN(int, 1);
}

// send file flags
RECOMP_DLL_FUNC(native_send_file_progress_flags)
{
    if (g_networkClient == nullptr)
    {
        RECOMP_RETURN(int, 0);
    }

    PTR(uint8_t)
    bufPtr = RECOMP_ARG(PTR(uint8_t), 0);
    int size = RECOMP_ARG(int, 1);

    if (!bufPtr || size <= 0)
    {
        RECOMP_RETURN(int, 0);
    }

    std::vector<uint8_t> flags = util::ReadByteBufferFromMemory(rdram, bufPtr, size);
    g_networkClient->SendFileProgressFlags(flags);

    RECOMP_RETURN(int, 1);
}

// send unlocked move state
RECOMP_DLL_FUNC(native_send_ability_progress)
{
    if (g_networkClient == nullptr)
    {
        RECOMP_RETURN(int, 0);
    }

    PTR(uint8_t)
    bufPtr = RECOMP_ARG(PTR(uint8_t), 0);
    int size = RECOMP_ARG(int, 1);

    if (!bufPtr || size <= 0)
    {
        RECOMP_RETURN(int, 0);
    }

    std::vector<uint8_t> bytes = util::ReadByteBufferFromMemory(rdram, bufPtr, size);
    g_networkClient->SendAbilityProgress(bytes);
    RECOMP_RETURN(int, 1);
}

// send honeycomb score total
RECOMP_DLL_FUNC(native_send_honeycomb_score)
{
    if (g_networkClient == nullptr)
    {
        RECOMP_RETURN(int, 0);
    }

    PTR(uint8_t)
    bufPtr = RECOMP_ARG(PTR(uint8_t), 0);
    int size = RECOMP_ARG(int, 1);

    if (!bufPtr || size <= 0)
    {
        RECOMP_RETURN(int, 0);
    }

    std::vector<uint8_t> bytes = util::ReadByteBufferFromMemory(rdram, bufPtr, size);
    g_networkClient->SendHoneycombScore(bytes);
    RECOMP_RETURN(int, 1);
}

// send mumbo score total
RECOMP_DLL_FUNC(native_send_mumbo_score)
{
    if (g_networkClient == nullptr)
    {
        RECOMP_RETURN(int, 0);
    }

    PTR(uint8_t)
    bufPtr = RECOMP_ARG(PTR(uint8_t), 0);
    int size = RECOMP_ARG(int, 1);

    if (!bufPtr || size <= 0)
    {
        RECOMP_RETURN(int, 0);
    }

    std::vector<uint8_t> bytes = util::ReadByteBufferFromMemory(rdram, bufPtr, size);
    g_networkClient->SendMumboScore(bytes);
    RECOMP_RETURN(int, 1);
}

// send a collected honeycomb
RECOMP_DLL_FUNC(native_send_honeycomb_collected)
{
    int mapId = RECOMP_ARG(int, 0);
    int honeycombId = RECOMP_ARG(int, 1);
    int xyPacked = RECOMP_ARG(int, 2);
    int z = RECOMP_ARG(int, 3);
    int x = (int16_t)(xyPacked & 0xFFFF);
    int y = (int16_t)((xyPacked >> 16) & 0xFFFF);

    if (g_networkClient != nullptr)
    {
        g_networkClient->SendHoneycombCollected(mapId, honeycombId, x, y, z);
    }

    RECOMP_RETURN(int, 1);
}

// send a collected mumbo token
RECOMP_DLL_FUNC(native_send_mumbo_token_collected)
{
    int mapId = RECOMP_ARG(int, 0);
    int tokenId = RECOMP_ARG(int, 1);
    int xyPacked = RECOMP_ARG(int, 2);
    int z = RECOMP_ARG(int, 3);
    int x = (int16_t)(xyPacked & 0xFFFF);
    int y = (int16_t)((xyPacked >> 16) & 0xFFFF);

    if (g_networkClient != nullptr)
    {
        g_networkClient->SendMumboTokenCollected(mapId, tokenId, x, y, z);
    }

    RECOMP_RETURN(int, 1);
}

// send a teleport request to another player
RECOMP_DLL_FUNC(native_send_player_info_request)
{
    uint32_t targetPlayerId = RECOMP_ARG(uint32_t, 0);
    uint32_t requesterPlayerId = RECOMP_ARG(uint32_t, 1);

    if (g_networkClient != nullptr)
    {
        g_networkClient->SendPlayerInfoRequest(targetPlayerId, requesterPlayerId);
    }

    RECOMP_RETURN(int, 1);
}

// send current map and pos info for teleport requests
RECOMP_DLL_FUNC(native_send_player_info_response)
{
    uint32_t targetPlayerId = RECOMP_ARG(uint32_t, 0);
    int16_t mapId = RECOMP_ARG(int16_t, 1);
    int16_t levelId = RECOMP_ARG(int16_t, 2);
    PTR(float)
    posPtr = RECOMP_ARG(PTR(float), 3);

    if (g_networkClient != nullptr && posPtr)
    {
        float position[4];
        util::ReadFloatsFromMemory(rdram, posPtr, position, 4);
        g_networkClient->SendPlayerInfoResponse(targetPlayerId, mapId, levelId, 
                                                 position[0], position[1], position[2], position[3]);
    }

    RECOMP_RETURN(int, 1);
}

// used to broadcast puppet (pos/rot) updates
RECOMP_DLL_FUNC(net_send_puppet_update)
{
    void *puppet_data = RECOMP_ARG(void *, 0);

    if (g_networkClient != nullptr && puppet_data != nullptr)
    {
        PuppetUpdatePacket pak;
        util::DeserializePuppetData(puppet_data, pak);
        g_networkClient->SendPuppetUpdate(pak);
    }

    RECOMP_RETURN(int, 1);
}

// gets client clock (used for sync stuff)
RECOMP_DLL_FUNC(GetClockMS)
{
    uint32_t time = 0;
    if (g_networkClient != nullptr)
    {
        time = g_networkClient->GetClockMS();
    }

    RECOMP_RETURN(uint32_t, time);
}

// gets typed characters for console ui
RECOMP_DLL_FUNC(native_poll_console_input)
{
    ConsoleInput_Poll();
    RECOMP_RETURN(int, 0);
}
