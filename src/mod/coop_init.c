// =========================================================================== //
// This is the recomp mod file.
//
// It handles all game code, and calls our lib_main functions as needed
// to handle networking.
// =========================================================================== //

#include "modding.h"
#include "functions.h"
#include "variables.h"
#include "recomputils.h"
#include "recompconfig.h"
#include "recompui.h"
#include "notes/note_saving.h"
#include "notes/demo_playback_functions.h"
#include "utils/corner_message.h"

extern void jiggyscore_setCollected(int levelid, int jiggy_id);
extern int jiggyscore_isCollected(int levelid, int jiggy_id);
extern void item_adjustByDiffWithoutHud(int item, int diff);


// Import functions from lib_main
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

// Recomp imports
RECOMP_IMPORT("*", s32 bkrecomp_note_saving_active());
RECOMP_IMPORT("*", s32 bkrecomp_note_saving_enabled());
// RECOMP_IMPORT("*", s32 recomp_in_demo_playback_game_mode());
RECOMP_IMPORT("*", void *bkrecomp_get_extended_prop_data(Cube* cube, Prop* prop, PropExtensionId extension_id));


// Sync-related functions from lib_main
RECOMP_IMPORT(".", int net_send_jiggy(int level_id, int jiggy_id));
RECOMP_IMPORT(".", int net_send_note(int mapId, int levelId, int isDynamic, int noteIndex));

// most of these are defined by lib_packets now and can probably
// be removed
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

extern enum map_e map_get(void);
extern s32 level_get(void);




// ======================================================================== //
// This code is for receiving messages from lib_main.
//
// Recomp only has functions for returning numbers from our library imports,
// so we need to do fun things to get strings using only 32bit numbers.
// ======================================================================== //

#define NET_MSG_TEXT_SIZE 128

/**
 * Retrieves the text portion of the current network message
 */
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

/**
 * Builds a display message from a username and action
 */
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

/**
 * Handles a remote jiggy collected message.
 * In future, major item collection messages will likely
 * be shown in smaller text on the bottom right,
 * like when you collect items in OOT/Anchor.
 */
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

static void HandleRemoteNoteCollected(void) {
    int map_id = net_msg_get_data_int(0);
    int level_id = net_msg_get_data_int(1);
    int is_dynamic = net_msg_get_data_int(2);
    int note_index = net_msg_get_data_int(3);

    if(!is_note_collected(map_id, level_id, note_index)) {
        if(is_dynamic) {
            ShowCornerMessage("A player collected a dynamic note");
            collect_dynamic_note(map_id, level_id);
        } else {
            ShowCornerMessage("A player collected a note");
            set_note_collected(map_id, level_id, note_index);
        }

        item_adjustByDiffWithoutHud(ITEM_C_NOTE, 1);
    }
}

/**
 * Processes incoming network messages
 */
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
                HandleRemoteNoteCollected();
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

// ============================================== //
// The rest of the code here is for networking and
// game logic, syncing stuff.
// ============================================== //

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

/**
 * This function is the main BK loop.
 * We hook it to check for network packets and 
 * handle the messagebox queue.
 */
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

/**
 * This hook is called when the player collects a jiggy.
 */
RECOMP_HOOK_RETURN("jiggyscore_setCollected") void jiggyscore_setCollected_hook(int level_id, int jiggy_id) {
    if (applying_remote_state) return;
    
    recomp_printf("[Local] Jiggy collected: level=%d jiggy=%d\n", level_id, jiggy_id);
    net_send_jiggy(level_id, jiggy_id);
}

/**
 * This hook is called when the player collects a note.
 * Intention here is to use the recomp note saving functionality
 * and sync collected notes specifically, hence why note saving has to be turned
 * on to use this mod.
 */
RECOMP_HOOK("__baMarker_resolveMusicNoteCollision") void note_collide(Prop *arg0) {
    // null check arg
    if (arg0 == NULL) {
        recomp_printf("Music collision arg is null\n");
        return;
    }

   if (!recomp_in_demo_playback_game_mode()) {
        if (arg0->is_actor) {
            collect_dynamic_note(map_get(), level_get());
            net_send_note(map_get(), level_get(), 1, 0);
            ShowCornerMessage("Dynamic Note collected!");
        }
        else if (!arg0->is_3d) {
            Cube *prop_cube = find_cube_for_prop(arg0);
            if (prop_cube != NULL) {
                NoteSavingExtensionData *note_data = (NoteSavingExtensionData *)bkrecomp_get_extended_prop_data(prop_cube, arg0, get_note_saving_prop_extension_id());
                // set_note_collected(map_get(), level_get(), note_data->note_index);
                net_send_note(map_get(), level_get(), 0, note_data->note_index);

                // print note ID
                recomp_printf("Collected note index: %d\n", note_data->note_index);
            }
            ShowCornerMessage("Note collected!");
        }
    }
}

/**
 * This hook is called when the mod is initialized, which happens when you click Start Game
 * and the actual game itself is loaded and run.
 */
RECOMP_CALLBACK("*", recomp_on_init) void on_init(void) {
    if(!bkrecomp_note_saving_enabled()) {
        ShowCornerMessage("Network mod disabled. Enable Note Saving in your settings and restart.");
        return;
    }

    init_note_saving();
    calculate_map_start_note_indices();

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
    } else {
        recomp_printf("Network init succeeded.\n");
    }
}

