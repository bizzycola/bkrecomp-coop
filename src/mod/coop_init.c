#include "modding.h"
#include "functions.h"
#include "variables.h"
#include "recomputils.h"
#include "recompconfig.h"
#include "recompui.h"

extern void jiggyscore_setCollected(int levelid, int jiggy_id);
extern int jiggyscore_isCollected(int levelid, int jiggy_id);
extern void item_adjustByDiffWithoutHud(int item, int diff);

//extern s32 bkrecomp_note_saving_active();
//extern s32 bkrecomp_note_saving_enabled();

RECOMP_IMPORT(".", int net_init(const char* host, const char* username, const char* lobby_name, const char* password));
RECOMP_IMPORT(".", int net_update(void));
RECOMP_IMPORT(".", int net_test_udp(void));

RECOMP_IMPORT(".", int net_msg_poll(void));
RECOMP_IMPORT(".", int net_msg_get_text_len(void));
RECOMP_IMPORT(".", int net_msg_get_text_chunk(int offset));
RECOMP_IMPORT(".", int net_msg_get_data_len(void));
RECOMP_IMPORT(".", int net_msg_get_data_chunk(int offset));
RECOMP_IMPORT(".", int net_msg_get_data_int(int index));
RECOMP_IMPORT(".", int net_msg_consume(void));

RECOMP_IMPORT("*", s32 bkrecomp_note_saving_active());
RECOMP_IMPORT("*", s32 bkrecomp_note_saving_enabled());


// sync funcs
RECOMP_IMPORT(".", int net_send_jiggy(int level_id, int jiggy_id));


#define MSGTYPE_NONE                0
#define MSGTYPE_PLAYER_CONNECTED    1
#define MSGTYPE_PLAYER_DISCONNECTED 2
#define MSGTYPE_CHAT                3
#define MSGTYPE_SERVER_INFO         4
#define MSGTYPE_JIGGY_COLLECTED     10
#define MSGTYPE_NOTE_COLLECTED      11
#define MSGTYPE_HONEYCOMB_COLLECTED 12
#define MSGTYPE_MUMBO_TOKEN_COLLECTED 13
#define MSGTYPE_JINJO_COLLECTED     14

#define UI_MSG_QUEUE_SIZE 16
#define UI_MSG_MAX_LENGTH 128
#define UI_MSG_DISPLAY_FRAMES 180

typedef struct {
    char messages[UI_MSG_QUEUE_SIZE][UI_MSG_MAX_LENGTH];
    int head;
    int tail;
    int count;
} UIMessageQueue;

static UIMessageQueue ui_msg_queue = { .head = 0, .tail = 0, .count = 0 };
static RecompuiContext ui_msg_context = NULL;
static int ui_msg_display_timer = 0;
static int ui_msg_is_visible = 0;

static void UIQueueMessage(const char* message) {
    if (ui_msg_queue.count >= UI_MSG_QUEUE_SIZE) {
        ui_msg_queue.tail = (ui_msg_queue.tail + 1) % UI_MSG_QUEUE_SIZE;
        ui_msg_queue.count--;
    }

    int i = 0;
    while (message[i] != '\0' && i < UI_MSG_MAX_LENGTH - 1) {
        ui_msg_queue.messages[ui_msg_queue.head][i] = message[i];
        i++;
    }
    ui_msg_queue.messages[ui_msg_queue.head][i] = '\0';

    ui_msg_queue.head = (ui_msg_queue.head + 1) % UI_MSG_QUEUE_SIZE;
    ui_msg_queue.count++;
}

static const char* UIDequeueMessage(void) {
    if (ui_msg_queue.count == 0) return NULL;
    const char* message = ui_msg_queue.messages[ui_msg_queue.tail];
    ui_msg_queue.tail = (ui_msg_queue.tail + 1) % UI_MSG_QUEUE_SIZE;
    ui_msg_queue.count--;
    return message;
}

static void UIHideMessage(void) {
    if (ui_msg_context != NULL) {
        recompui_hide_context(ui_msg_context);
        ui_msg_context = NULL;
    }
    ui_msg_is_visible = 0;
    ui_msg_display_timer = 0;
}

static void UIDisplayMessage(const char* message) {
    if (ui_msg_context != NULL) {
        recompui_hide_context(ui_msg_context);
    }

    ui_msg_context = recompui_create_context();
    recompui_set_context_captures_input(ui_msg_context, 0);
    recompui_set_context_captures_mouse(ui_msg_context, 0);
    recompui_open_context(ui_msg_context);

    RecompuiResource root = recompui_context_root(ui_msg_context);
    recompui_set_position(root, POSITION_ABSOLUTE);
    recompui_set_top(root, 32.0f, UNIT_DP);
    recompui_set_right(root, 32.0f, UNIT_DP);
    recompui_set_width_auto(root);
    recompui_set_height_auto(root);

    RecompuiColor bg_color = { .r = 10, .g = 10, .b = 10, .a = 230 };
    RecompuiColor text_color = { .r = 255, .g = 255, .b = 255, .a = 255 };

    RecompuiResource container = recompui_create_element(ui_msg_context, root);
    recompui_set_background_color(container, &bg_color);
    recompui_set_padding(container, 12.0f, UNIT_DP);
    recompui_set_border_radius(container, 8.0f, UNIT_DP);
    recompui_set_border_width(container, 1.0f, UNIT_DP);
    recompui_set_border_color(container, &text_color);

    recompui_create_label(ui_msg_context, container, message, LABELSTYLE_NORMAL);

    recompui_close_context(ui_msg_context);
    recompui_show_context(ui_msg_context);

    ui_msg_is_visible = 1;
    ui_msg_display_timer = UI_MSG_DISPLAY_FRAMES;
}

static void UIUpdateMessageQueue(void) {
    if (ui_msg_is_visible) {
        ui_msg_display_timer--;
        if (ui_msg_display_timer <= 0) {
            UIHideMessage();
            const char* next = UIDequeueMessage();
            if (next != NULL) UIDisplayMessage(next);
        }
    } else {
        const char* next = UIDequeueMessage();
        if (next != NULL) UIDisplayMessage(next);
    }
}

void ShowCornerMessage(const char* message) {
    if (!ui_msg_is_visible) {
        UIDisplayMessage(message);
    } else {
        UIQueueMessage(message);
    }
}

#define NET_MSG_TEXT_SIZE 128

static void RetrieveNetMessageText(char* out_buffer, int max_len) {
    int len = net_msg_get_text_len();
    if (len <= 0 || len >= max_len) {
        out_buffer[0] = '\0';
        return;
    }

    int pos = 0;
    while (pos < len) {
        int packed = net_msg_get_text_chunk(pos);
        
        for (int i = 0; i < 4 && pos < len && pos < max_len - 1; i++) {
            char c = (char)((packed >> (i * 8)) & 0xFF);
            out_buffer[pos] = c;
            pos++;
            if (c == '\0') return;
        }
    }
    out_buffer[pos] = '\0';
}

static void BuildDisplayMessage(char* out, int out_size, const char* username, const char* action) {
    int j = 0;
    int k = 0;
    
    while (username[k] != '\0' && j < out_size - 20) {
        out[j++] = username[k++];
    }
    
    k = 0;
    while (action[k] != '\0' && j < out_size - 1) {
        out[j++] = action[k++];
    }
    out[j] = '\0';
}

static void HandleRemoteJiggyCollected(void) {
    int level_id = net_msg_get_data_int(0);
    int jiggy_id = net_msg_get_data_int(1);
    
    char collector[NET_MSG_TEXT_SIZE];
    RetrieveNetMessageText(collector, NET_MSG_TEXT_SIZE);
    
    if(!jiggyscore_isCollected(level_id, jiggy_id)) {
        jiggyscore_setCollected(level_id, jiggy_id);
        item_adjustByDiffWithoutHud(ITEM_26_JIGGY_TOTAL, 1);
    }
    
    if (collector[0] != '\0') {
        char display_msg[256];
        BuildDisplayMessage(display_msg, sizeof(display_msg), collector, " collected a Jiggy!");
        ShowCornerMessage(display_msg);
    } else {
        ShowCornerMessage("A player collected a Jiggy!");
    }
}

static void ProcessNetworkMessages(void) {
    for (int i = 0; i < 4; i++) {
        int poll_result = net_msg_poll();
        
        if (poll_result == 0) break;

        int msg_type = (poll_result >> 24) & 0xFF;
        int player_id = poll_result & 0x00FFFFFF;
        
        if (player_id == 0x00FFFFFF) player_id = -1;

        switch (msg_type) {
            case MSGTYPE_PLAYER_CONNECTED: {
                char username[NET_MSG_TEXT_SIZE];
                RetrieveNetMessageText(username, NET_MSG_TEXT_SIZE);
                char display_msg[256];
                BuildDisplayMessage(display_msg, sizeof(display_msg), username, " joined");
                ShowCornerMessage(display_msg);
                break;
            }
            
            case MSGTYPE_PLAYER_DISCONNECTED: {
                char username[NET_MSG_TEXT_SIZE];
                RetrieveNetMessageText(username, NET_MSG_TEXT_SIZE);
                char display_msg[256];
                BuildDisplayMessage(display_msg, sizeof(display_msg), username, " left");
                ShowCornerMessage(display_msg);
                break;
            }
            
            case MSGTYPE_JIGGY_COLLECTED: {
                HandleRemoteJiggyCollected();
                break;
            }
            
            case MSGTYPE_NOTE_COLLECTED: {

                break;
            }
            
            case MSGTYPE_HONEYCOMB_COLLECTED: {

                break;
            }
            
            case MSGTYPE_SERVER_INFO: {
                char info[NET_MSG_TEXT_SIZE];
                RetrieveNetMessageText(info, NET_MSG_TEXT_SIZE);
                ShowCornerMessage(info);
                break;
            }
        }

        net_msg_consume();
    }
}

static int last_net_status = -999;
static int is_connected = 0;

static void HandleNetworkStatus(int status) {
    if (status == last_net_status) return;
    last_net_status = status;

    switch (status) {
        case 0:
            if (!is_connected) ShowCornerMessage("Network Error / Init Failed");
            break;
        case 1:
            if (!is_connected) ShowCornerMessage("Connecting...");
            break;
        case 2:
            if (!is_connected) {
                ShowCornerMessage("Connected to Server!");
                is_connected = 1;
            }
            break;
        case 3:
            ShowCornerMessage("Disconnected.");
            is_connected = 0;
            break;
        case 4:
            break;
    }
}

static int applying_remote_state = 0;

RECOMP_HOOK_RETURN("mainLoop") void mainLoop(void) {
    if(!bkrecomp_note_saving_active()) {
        return;
    }


    UIUpdateMessageQueue();

    int net_status = net_update();
    HandleNetworkStatus(net_status);

    if (is_connected) {
        ProcessNetworkMessages();
    }
}

RECOMP_HOOK_RETURN("jiggyscore_setCollected") void jiggyscore_setCollected_hook(int level_id, int jiggy_id) {
    if (applying_remote_state) return;
    
    recomp_printf("[Local] Jiggy collected: level=%d jiggy=%d\n", level_id, jiggy_id);
    net_send_jiggy(level_id, jiggy_id);
}

RECOMP_HOOK_RETURN("__baMarker_resolveMusicNoteCollision") void note_collide(Prop *arg0) {
    if (!arg0->is_3d) {
        // Cube *prop_cube = find_cube_for_prop(arg0);
        // if (prop_cube != NULL) {
        //     // NoteSavingExtensionData* note_data = (NoteSavingExtensionData*)bkrecomp_get_extended_prop_data(prop_cube, arg0, note_saving_prop_extension_id);
        //     // set_note_collected(map_get(), level_get(), note_data->note_index);
        //     ShowCornerMessage("Note collected");
        // }
    }
}

RECOMP_CALLBACK("*", recomp_on_init) void on_init(void) {
    if(!bkrecomp_note_saving_enabled()) {
        ShowCornerMessage("Network mod disabled. Enable Note Saving in your settings and restart.");
        return;
    }

    char* host = recomp_get_config_string("server_url");
    char* username = recomp_get_config_string("username");
    char* lobby_name = recomp_get_config_string("lobby_name");
    char* password = recomp_get_config_string("lobby_password");

    if (host == NULL) {
        recomp_printf("Config 'server_url' missing.\n");
        if (username) recomp_free_config_string(username);
        if (lobby_name) recomp_free_config_string(lobby_name);
        if (password) recomp_free_config_string(password);
        return;
    }

    int res = net_init(
        host,
        username ? username : "Player",
        lobby_name ? lobby_name : "Lobby",
        password ? password : ""
    );

    recomp_free_config_string(host);
    if (username) recomp_free_config_string(username);
    if (lobby_name) recomp_free_config_string(lobby_name);
    if (password) recomp_free_config_string(password);

    if (res != 1) {
        recomp_printf("Network init failed.\n");
    }
}