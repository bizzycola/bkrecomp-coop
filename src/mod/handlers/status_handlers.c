#include "handlers/status_handlers.h"
#include "../message_queue/message_queue.h"
#include "modding.h"
#include "functions.h"
#include "../toast/toast.h"
#include "../puppets/puppet.h"
#include "../collection/collection.h"
#include "../console/console.h"

extern void player_list_ui_show(void);

void handle_connection_status(const void *vmsg)
{
    const GameMessage *msg = (const GameMessage *)vmsg;
    char statusBuf[MAX_MESSAGE_DATA_SIZE + 1];
    int n = (int)msg->dataSize;
    if (n < 0)
        n = 0;
    if (n > MAX_MESSAGE_DATA_SIZE)
        n = MAX_MESSAGE_DATA_SIZE;

    memcpy(statusBuf, msg->data, (size_t)n);
    statusBuf[n] = '\0';

    if (statusBuf[0] != '\0')
    {
        toast_show_immediate_custom("Connected!", TOAST_DEFAULT_DURATION,
                                    TOAST_POS_TOP_RIGHT, TOAST_SIZE_MEDIUM, TOAST_STYLE_SUCCESS);

        player_list_ui_show();
    }
}

void handle_connection_error(const void *vmsg)
{
    const GameMessage *msg = (const GameMessage *)vmsg;
    char errBuf[MAX_MESSAGE_DATA_SIZE + 1];
    int n = (int)msg->dataSize;
    if (n < 0)
        n = 0;
    if (n > MAX_MESSAGE_DATA_SIZE)
        n = MAX_MESSAGE_DATA_SIZE;

    memcpy(errBuf, msg->data, (size_t)n);
    errBuf[n] = '\0';

    if (errBuf[0] != '\0')
    {
        toast_error(errBuf);
    }
}

void handle_level_opened(const void *vmsg)
{
    const GameMessage *msg = (const GameMessage *)vmsg;
    int worldId = msg->param1;
    int jiggyCost = msg->param2;

    toast_info("Level opened");
    open_level(worldId, jiggyCost);
}

void handle_puppet_update(const void *vmsg)
{
    const GameMessage *msg = (const GameMessage *)vmsg;
    int playerId = msg->playerId;

    float x = msg->paramF1;
    float y = msg->paramF2;
    float z = msg->paramF3;

    short mapId = (short)(msg->param4 & 0xFF);
    short levelId = (short)((msg->param4 >> 8) & 0xFF);
    unsigned short animId = (unsigned short)((msg->param4 >> 16) & 0xFFFF);

    float yaw, pitch, roll;
    memcpy(&yaw, &msg->param1, sizeof(float));
    memcpy(&pitch, &msg->param2, sizeof(float));
    memcpy(&roll, &msg->param3, sizeof(float));

    float anim_duration = msg->paramF4;
    if (anim_duration <= 0.0f)
    {
        anim_duration = 1.0f;
    }
    float anim_timer = msg->paramF5;

    u8 playback_type = (u8)msg->param5;
    u8 playback_direction = (u8)msg->param6;

    PuppetUpdateData data;
    data.x = x;
    data.y = y;
    data.z = z;
    data.yaw = yaw;
    data.pitch = pitch;
    data.roll = roll;
    data.map_id = mapId;
    data.level_id = levelId;
    data.anim_id = (u16)animId;
    data.anim_duration = anim_duration;
    data.anim_timer = anim_timer;
    data.playback_type = playback_type;
    data.playback_direction = playback_direction;
    data.model_id = 0;
    data.flags = 0;

    puppet_handle_remote_update(playerId, &data);
}

void handle_console_toggle(const void *vmsg)
{
    (void)vmsg;
    console_toggle();
}

void handle_console_key(const void *vmsg)
{
    const GameMessage *msg = (const GameMessage *)vmsg;
    char key = (char)msg->param1;

    if (key == '\b')
    {
        console_handle_backspace();
    }
    else if (key == '\r' || key == '\n')
    {
        console_handle_enter();
    }
    else if (key >= 32 && key < 127)
    {
        console_handle_key(key);
    }
}
