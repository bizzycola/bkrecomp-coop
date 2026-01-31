#ifndef COOP_MESSAGES_H
#define COOP_MESSAGES_H

typedef enum
{
    MSG_NONE = 0,
    MSG_PLAYER_CONNECTED = 1,
    MSG_PLAYER_DISCONNECTED = 2,
    MSG_JIGGY_COLLECTED = 3,
    MSG_NOTE_COLLECTED = 4,
    MSG_PUPPET_UPDATE = 5,
    MSG_LEVEL_OPENED = 6,
    MSG_NOTE_SAVE_DATA = 7,
    MSG_CONNECTION_STATUS = 8,
    MSG_CONNECTION_ERROR = 9,
} MessageType;

#define MAX_MESSAGE_DATA_SIZE 256

typedef struct
{
    unsigned char type; // MessageType
    int playerId;
    int param1;
    int param2;
    int param3;
    int param4;
    float paramF1;
    float paramF2;
    float paramF3;
    unsigned short dataSize;
    unsigned char data[MAX_MESSAGE_DATA_SIZE];
} GameMessage;

int poll_queue_message(GameMessage *out_message);

static inline const char *message_queue_get_string(const GameMessage *msg)
{
    return (const char *)msg->data;
}

static inline void message_queue_get_rotation(const GameMessage *msg, float *yaw, float *pitch, float *roll)
{
    const float *rotData = (const float *)msg->data;
    if (yaw)
        *yaw = rotData[0];
    if (pitch)
        *pitch = rotData[1];
    if (roll)
        *roll = rotData[2];
}

void process_queue_message(const GameMessage *msg);

#endif
