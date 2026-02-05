#include "message_queue.h"
#include "modding.h"
#include "handlers/collection_handlers.h"
#include "handlers/savedata_handlers.h"
#include "handlers/player_handlers.h"
#include "handlers/status_handlers.h"

RECOMP_IMPORT(".", int net_msg_poll(void *buffer));

static GameMessage s_poll_buf;

int poll_queue_message(GameMessage *out_message)
{
    if (!out_message)
    {
        return 0;
    }

    return net_msg_poll(out_message);
}

void process_queue_message(const GameMessage *msg)
{
    if (!msg)
        return;

    switch (msg->type)
    {
    case MSG_INITIAL_SAVE_DATA_REQUEST:
        handle_initial_save_data_request(msg);
        break;

    case MSG_PLAYER_CONNECTED:
        handle_player_connected(msg);
        break;

    case MSG_PLAYER_DISCONNECTED:
        handle_player_disconnected(msg);
        break;

    case MSG_JIGGY_COLLECTED:
        handle_jiggy_collected(msg);
        break;

    case MSG_NOTE_COLLECTED:
        handle_note_collected(msg);
        break;

    case MSG_PUPPET_UPDATE:
        handle_puppet_update(msg);
        break;

    case MSG_LEVEL_OPENED:
        handle_level_opened(msg);
        break;

    case MSG_NOTE_SAVE_DATA:
        handle_note_save_data(msg);
        break;

    case MSG_FILE_PROGRESS_FLAGS:
        handle_file_progress_flags(msg);
        break;

    case MSG_ABILITY_PROGRESS:
        handle_ability_progress(msg);
        break;

    case MSG_HONEYCOMB_SCORE:
        handle_honeycomb_score(msg);
        break;

    case MSG_MUMBO_SCORE:
        handle_mumbo_score(msg);
        break;

    case MSG_HONEYCOMB_COLLECTED:
        handle_honeycomb_collected(msg);
        break;

    case MSG_MUMBO_TOKEN_COLLECTED:
        handle_mumbo_token_collected(msg);
        break;

    case MSG_CONNECTION_STATUS:
        handle_connection_status(msg);
        break;

    case MSG_CONNECTION_ERROR:
        handle_connection_error(msg);
        break;

    case MSG_PLAYER_INFO_REQUEST:
        handle_player_info_request(msg);
        break;

    case MSG_PLAYER_INFO_RESPONSE:
        handle_player_info_response(msg);
        break;

    case MSG_PLAYER_LIST_UPDATE:
        handle_player_list_update(msg);
        break;

    case MSG_CONSOLE_TOGGLE:
        handle_console_toggle(msg);
        break;

    case MSG_CONSOLE_KEY:
        handle_console_key(msg);
        break;

    default:
        break;
    }
}
