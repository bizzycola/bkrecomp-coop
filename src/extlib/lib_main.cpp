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
    // Disabled VEH for now - may be causing issues
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

static inline float swap_float(const uint8_t *ptr)
{
    uint32_t val = ((uint32_t)ptr[0] << 24) | ((uint32_t)ptr[1] << 16) |
                   ((uint32_t)ptr[2] << 8) | ((uint32_t)ptr[3]);
    float result;
    memcpy(&result, &val, sizeof(float));
    return result;
}

static inline int16_t swap_int16(const uint8_t *ptr)
{
    return (int16_t)(((uint16_t)ptr[0] << 8) | ((uint16_t)ptr[1]));
}

static inline uint16_t swap_uint16(const uint8_t *ptr)
{
    return ((uint16_t)ptr[0] << 8) | ((uint16_t)ptr[1]);
}

extern "C"
{
    DLLEXPORT uint32_t recomp_api_version = 1;
}

static NetworkClient *g_networkClient = nullptr;
static int g_connect_state = 0;

RECOMP_DLL_FUNC(native_lib_test)
{
#if defined(_WIN32)
    coop_install_vectored_handler_once();
#endif
    coop_dll_log("[COOP][DLL] native_lib_test: called");
    RECOMP_RETURN(int, 0);
}

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
        MEM_B(0, buffer_ptr) = msg.type;
        MEM_W(4, buffer_ptr) = msg.playerId;
        MEM_W(8, buffer_ptr) = msg.param1;
        MEM_W(12, buffer_ptr) = msg.param2;
        MEM_W(16, buffer_ptr) = msg.param3;
        MEM_W(20, buffer_ptr) = msg.param4;
        MEM_W(24, buffer_ptr) = msg.param5;
        MEM_W(28, buffer_ptr) = msg.param6;

        uint32_t f1_bits, f2_bits, f3_bits, f4_bits, f5_bits;
        memcpy(&f1_bits, &msg.paramF1, sizeof(float));
        memcpy(&f2_bits, &msg.paramF2, sizeof(float));
        memcpy(&f3_bits, &msg.paramF3, sizeof(float));
        memcpy(&f4_bits, &msg.paramF4, sizeof(float));
        memcpy(&f5_bits, &msg.paramF5, sizeof(float));
        MEM_W(32, buffer_ptr) = f1_bits;
        MEM_W(36, buffer_ptr) = f2_bits;
        MEM_W(40, buffer_ptr) = f3_bits;
        MEM_W(44, buffer_ptr) = f4_bits;
        MEM_W(48, buffer_ptr) = f5_bits;

        MEM_H(52, buffer_ptr) = msg.dataSize;

        for (size_t i = 0; i < MAX_MESSAGE_DATA_SIZE; i++)
        {
            MEM_B(54 + i, buffer_ptr) = msg.data[i];
        }

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

static uint8_t PacketTypeToMessageType(PacketType packetType)
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

RECOMP_DLL_FUNC(native_update_network)
{
    if (g_networkClient != nullptr)
    {
        g_networkClient->Update();

        while (g_networkClient->HasEvents())
        {
            NetEvent evt = g_networkClient->PopEvent();

            GameMessage msg;
            memset(&msg, 0, sizeof(GameMessage));
            msg.type = PacketTypeToMessageType(evt.type);
            msg.playerId = evt.playerId;

            if (!evt.textData.empty())
            {
                size_t copySize = std::min(evt.textData.size(), MAX_MESSAGE_DATA_SIZE - 1);
                memcpy(msg.data, evt.textData.c_str(), copySize);
                msg.data[copySize] = '\0';
                msg.dataSize = (uint16_t)(copySize + 1);
            }

            if (evt.intData.size() > 0)
                msg.param1 = evt.intData[0];
            if (evt.intData.size() > 1)
                msg.param2 = evt.intData[1];
            if (evt.intData.size() > 2)
                msg.param3 = evt.intData[2];
            if (evt.intData.size() > 3)
                msg.param4 = evt.intData[3];
            if (evt.intData.size() > 4)
                msg.param5 = evt.intData[4];
            if (evt.intData.size() > 5)
                msg.param6 = evt.intData[5];

            if (evt.floatData.size() > 0)
                msg.paramF1 = evt.floatData[0];
            if (evt.floatData.size() > 1)
                msg.paramF2 = evt.floatData[1];
            if (evt.floatData.size() > 2)
                msg.paramF3 = evt.floatData[2];
            if (evt.floatData.size() > 3)
                msg.paramF4 = evt.floatData[3];
            if (evt.floatData.size() > 4)
                msg.paramF5 = evt.floatData[4];

            g_messageQueue.Push(msg);
        }
    }

    RECOMP_RETURN(int, 0);
}

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

    std::vector<uint8_t> flags;
    flags.resize((size_t)size);
    for (int i = 0; i < size; i++)
    {
        flags[(size_t)i] = MEM_B(i, bufPtr);
    }

    g_networkClient->SendFileProgressFlags(flags);

    RECOMP_RETURN(int, 1);
}

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

    std::vector<uint8_t> bytes;
    bytes.resize((size_t)size);
    for (int i = 0; i < size; i++)
    {
        bytes[(size_t)i] = MEM_B(i, bufPtr);
    }

    g_networkClient->SendAbilityProgress(bytes);
    RECOMP_RETURN(int, 1);
}

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

    std::vector<uint8_t> bytes;
    bytes.resize((size_t)size);
    for (int i = 0; i < size; i++)
    {
        bytes[(size_t)i] = MEM_B(i, bufPtr);
    }

    g_networkClient->SendHoneycombScore(bytes);
    RECOMP_RETURN(int, 1);
}

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

    std::vector<uint8_t> bytes;
    bytes.resize((size_t)size);
    for (int i = 0; i < size; i++)
    {
        bytes[(size_t)i] = MEM_B(i, bufPtr);
    }

    g_networkClient->SendMumboScore(bytes);
    RECOMP_RETURN(int, 1);
}

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

RECOMP_DLL_FUNC(native_send_player_info_response)
{
    uint32_t targetPlayerId = RECOMP_ARG(uint32_t, 0);
    int16_t mapId = RECOMP_ARG(int16_t, 1);
    int16_t levelId = RECOMP_ARG(int16_t, 2);
    PTR(float)
    posPtr = RECOMP_ARG(PTR(float), 3);

    if (g_networkClient != nullptr && posPtr)
    {
        uint32_t x_bits = MEM_W(0, posPtr);
        uint32_t y_bits = MEM_W(4, posPtr);
        uint32_t z_bits = MEM_W(8, posPtr);
        uint32_t yaw_bits = MEM_W(12, posPtr);

        float x = *reinterpret_cast<float *>(&x_bits);
        float y = *reinterpret_cast<float *>(&y_bits);
        float z = *reinterpret_cast<float *>(&z_bits);
        float yaw = *reinterpret_cast<float *>(&yaw_bits);

        g_networkClient->SendPlayerInfoResponse(targetPlayerId, mapId, levelId, x, y, z, yaw);
    }

    RECOMP_RETURN(int, 1);
}

RECOMP_DLL_FUNC(net_send_puppet_update)
{
    void *puppet_data = RECOMP_ARG(void *, 0);

    if (g_networkClient != nullptr && puppet_data != nullptr)
    {
        PuppetUpdatePacket pak;

        uint8_t *byte_ptr = (uint8_t *)puppet_data;
        float *float_ptr = (float *)puppet_data;

        pak.x = float_ptr[0];
        pak.y = float_ptr[1];
        pak.z = float_ptr[2];
        pak.yaw = float_ptr[3];
        pak.pitch = float_ptr[4];
        pak.roll = float_ptr[5];
        pak.anim_duration = float_ptr[6];
        pak.anim_timer = float_ptr[7];

        int16_t *int16_ptr_32 = (int16_t *)&byte_ptr[32];
        pak.level_id = int16_ptr_32[0];
        pak.map_id = int16_ptr_32[1];

        uint16_t *uint16_ptr_38 = (uint16_t *)&byte_ptr[38];
        pak.anim_id = uint16_ptr_38[0];

        pak.model_id = byte_ptr[40];
        pak.flags = byte_ptr[41];
        pak.playback_type = byte_ptr[43];
        pak.playback_direction = byte_ptr[42];

        g_networkClient->SendPuppetUpdate(pak);
    }

    RECOMP_RETURN(int, 1);
}

RECOMP_DLL_FUNC(GetClockMS)
{
    uint32_t time = 0;
    if (g_networkClient != nullptr)
    {
        time = g_networkClient->GetClockMS();
    }

    RECOMP_RETURN(uint32_t, time);
}

#if defined(_WIN32)
static bool g_last_tilde_state = false;
static bool g_console_is_open = false;
static std::string g_input_buffer;

static bool IsKeyJustPressed(int vkCode, bool &lastState)
{
    bool currentState = (GetAsyncKeyState(vkCode) & 0x8000) != 0;
    bool justPressed = currentState && !lastState;
    lastState = currentState;
    return justPressed;
}

static void PollConsoleInput()
{
    bool tildePressed = (GetAsyncKeyState(VK_OEM_3) & 0x8000) != 0;
    if (tildePressed && !g_last_tilde_state)
    {
        g_console_is_open = !g_console_is_open;
        GameMessage msg;
        memset(&msg, 0, sizeof(GameMessage));
        msg.type = 21;
        g_messageQueue.Push(msg);
    }
    g_last_tilde_state = tildePressed;

    if (!g_console_is_open)
        return;

    for (int vk = 0x20; vk <= 0x5A; vk++)
    {
        if (GetAsyncKeyState(vk) & 0x8000)
        {
            bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            char c = 0;

            if (vk >= 0x30 && vk <= 0x39)
            {
                if (shift)
                {
                    const char *shifted = ")!@#$%^&*(";
                    c = shifted[vk - 0x30];
                }
                else
                {
                    c = (char)vk;
                }
            }
            else if (vk >= 0x41 && vk <= 0x5A)
            {
                c = (char)vk;
                if (!shift)
                    c += 32;
            }
            else if (vk == VK_SPACE)
            {
                c = ' ';
            }

            if (c != 0)
            {
                static int last_vk = 0;
                static auto last_time = std::chrono::steady_clock::now();
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count();

                if (vk != last_vk || elapsed > 100)
                {
                    GameMessage msg;
                    memset(&msg, 0, sizeof(GameMessage));
                    msg.type = 20;
                    msg.param1 = (int)c;
                    g_messageQueue.Push(msg);

                    last_vk = vk;
                    last_time = now;
                }
            }
            break;
        }
    }

    if (GetAsyncKeyState(VK_BACK) & 0x8000)
    {
        static auto last_backspace = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_backspace).count();

        if (elapsed > 100)
        {
            GameMessage msg;
            memset(&msg, 0, sizeof(GameMessage));
            msg.type = 20;
            msg.param1 = (int)'\b';
            g_messageQueue.Push(msg);
            last_backspace = now;
        }
    }

    if (GetAsyncKeyState(VK_RETURN) & 0x8000)
    {
        static auto last_enter = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_enter).count();

        if (elapsed > 200)
        {
            GameMessage msg;
            memset(&msg, 0, sizeof(GameMessage));
            msg.type = 20;
            msg.param1 = (int)'\r';
            g_messageQueue.Push(msg);
            last_enter = now;
        }
    }
}
#else
static void PollConsoleInput()
{
}
#endif

RECOMP_DLL_FUNC(native_poll_console_input)
{
    PollConsoleInput();
    RECOMP_RETURN(int, 0);
}
