#include "coop_messages.h"
#include "modding.h"
#include "functions.h"
#include "variables.h"
#include "../toast/toast.h"
#include "recomputils.h"

RECOMP_IMPORT(".", int native_poll_message(void *buffer));

int coop_poll_message(GameMessage *out_message)
{
    if (!out_message)
    {
        return 0;
    }

    return native_poll_message(out_message);
}

// Process a message based on its type
void coop_process_message(const GameMessage *msg)
{
    if (!msg)
    {
        return;
    }

    switch (msg->type)
    {
    case MSG_PLAYER_CONNECTED:
    {
        const char *username = coop_message_get_string(msg);
        toast_success(username);
        break;
    }

    case MSG_PLAYER_DISCONNECTED:
    {
        toast_info("Player disconnected");
        break;
    }

    case MSG_JIGGY_COLLECTED:
    {
        int levelId = msg->param1;
        int jiggyId = msg->param2;
        break;
    }

    case MSG_NOTE_COLLECTED:
    {
        int mapId = msg->param1;
        int levelId = msg->param2;
        int isDynamic = (msg->param3 != 0);
        int noteIndex = msg->param4;
        break;
    }

    case MSG_PUPPET_UPDATE:
    {
        int playerId = msg->playerId;
        float x = msg->paramF1;
        float y = msg->paramF2;
        float z = msg->paramF3;
        short mapId = (short)msg->param1;
        short levelId = (short)msg->param2;

        float yaw, pitch, roll;
        coop_message_get_rotation(msg, &yaw, &pitch, &roll);

        break;
    }

    case MSG_LEVEL_OPENED:
    {
        int worldId = msg->param1;
        int jiggyCost = msg->param2;
        break;
    }

    case MSG_NOTE_SAVE_DATA:
    {
        break;
    }

    case MSG_CONNECTION_STATUS:
    {
        const char *status = coop_message_get_string(msg);
        toast_info(status);
        break;
    }

    case MSG_CONNECTION_ERROR:
    {
        const char *error = coop_message_get_string(msg);
        toast_error(error);
        break;
    }

    default:
        break;
    }
}
