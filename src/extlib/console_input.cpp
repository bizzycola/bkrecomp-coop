#include "console_input.h"
#include "lib_message_queue.h"
#include <cstring>
#include <chrono>

#if defined(_WIN32)
#include <windows.h>
#endif

#if defined(_WIN32)
static bool g_last_tilde_state = false;
#endif

static bool g_console_is_open = false;

void ConsoleInput_Init()
{
#if defined(_WIN32)
    g_last_tilde_state = false;
#endif
    g_console_is_open = false;
}

bool ConsoleInput_IsOpen()
{
    return g_console_is_open;
}

#if defined(_WIN32)

static char VKToChar(int vk, bool shift)
{
    // Numbers (0-9)
    if (vk >= 0x30 && vk <= 0x39)
    {
        if (shift)
        {
            const char *shifted = ")!@#$%^&*(";
            return shifted[vk - 0x30];
        }
        else
        {
            return (char)vk;
        }
    }
    else if (vk >= 0x41 && vk <= 0x5A)
    {
        char c = (char)vk;
        if (!shift)
            c += 32;
        return c;
    }
    else if (vk == VK_SPACE)
    {
        return ' ';
    }
    else if (vk == VK_OEM_1) // ;:
    {
        return shift ? ':' : ';';
    }
    else if (vk == VK_OEM_PLUS) // =+
    {
        return shift ? '+' : '=';
    }
    else if (vk == VK_OEM_COMMA) // ,<
    {
        return shift ? '<' : ',';
    }
    else if (vk == VK_OEM_MINUS) // -_
    {
        return shift ? '_' : '-';
    }
    else if (vk == VK_OEM_PERIOD) // .>
    {
        return shift ? '>' : '.';
    }
    else if (vk == VK_OEM_2) // /?
    {
        return shift ? '?' : '/';
    }
    else if (vk == VK_OEM_4) // [{
    {
        return shift ? '{' : '[';
    }
    else if (vk == VK_OEM_5) // \|
    {
        return shift ? '|' : '\\';
    }
    else if (vk == VK_OEM_6) // ]}
    {
        return shift ? '}' : ']';
    }
    else if (vk == VK_OEM_7) // '"
    {
        return shift ? '"' : '\'';
    }

    return 0;
}

static void SendCharMessage(char c, int vk)
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

static void CheckSpecialKey(int vk, char c, int debounce_ms)
{
    static auto last_time = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count();

    if ((GetAsyncKeyState(vk) & 0x8000) && elapsed > debounce_ms)
    {
        GameMessage msg;
        memset(&msg, 0, sizeof(GameMessage));
        msg.type = 20;
        msg.param1 = (int)c;
        g_messageQueue.Push(msg);
        last_time = now;
    }
}

void ConsoleInput_Poll()
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

    bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

    for (int vk = 0x30; vk <= 0x5A; vk++)
    {
        if (vk > 0x39 && vk < 0x41)
            continue;
        
        if (GetAsyncKeyState(vk) & 0x8000)
        {
            char c = VKToChar(vk, shift);
            if (c != 0)
            {
                SendCharMessage(c, vk);
            }
            break;
        }
    }

    if (GetAsyncKeyState(VK_SPACE) & 0x8000)
    {
        SendCharMessage(' ', VK_SPACE);
    }

    static const int oem_keys[] = {
        VK_OEM_1, VK_OEM_PLUS, VK_OEM_COMMA, VK_OEM_MINUS,
        VK_OEM_PERIOD, VK_OEM_2, VK_OEM_4, VK_OEM_5, VK_OEM_6, VK_OEM_7
    };

    for (int i = 0; i < sizeof(oem_keys) / sizeof(oem_keys[0]); i++)
    {
        if (GetAsyncKeyState(oem_keys[i]) & 0x8000)
        {
            char c = VKToChar(oem_keys[i], shift);
            if (c != 0)
            {
                SendCharMessage(c, oem_keys[i]);
            }
            break;
        }
    }

    static auto last_backspace = std::chrono::steady_clock::now();
    if (GetAsyncKeyState(VK_BACK) & 0x8000)
    {
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

    static auto last_enter = std::chrono::steady_clock::now();
    if (GetAsyncKeyState(VK_RETURN) & 0x8000)
    {
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

void ConsoleInput_Poll()
{
}

#endif
