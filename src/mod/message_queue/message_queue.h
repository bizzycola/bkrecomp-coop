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
    MSG_INITIAL_SAVE_DATA_REQUEST = 10,
    MSG_FILE_PROGRESS_FLAGS = 11,
    MSG_ABILITY_PROGRESS = 12,
    MSG_HONEYCOMB_SCORE = 13,
    MSG_MUMBO_SCORE = 14,

    MSG_HONEYCOMB_COLLECTED = 15,
    MSG_MUMBO_TOKEN_COLLECTED = 16,
    MSG_PLAYER_INFO_REQUEST = 17,
    MSG_PLAYER_INFO_RESPONSE = 18,
    MSG_PLAYER_LIST_UPDATE = 19,
    MSG_CONSOLE_KEY = 20,
    MSG_CONSOLE_TOGGLE = 21,
} MessageType;

#define MAX_MESSAGE_DATA_SIZE 256

typedef struct
{
    unsigned char type;
    int playerId;
    int param1;
    int param2;
    int param3;
    int param4;
    int param5;
    int param6;
    float paramF1;
    float paramF2;
    float paramF3;
    float paramF4;
    float paramF5;
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
    union
    {
        float f;
        unsigned char bytes[4];
    } temp;

    if (yaw)
    {
        temp.bytes[0] = msg->data[3];
        temp.bytes[1] = msg->data[2];
        temp.bytes[2] = msg->data[1];
        temp.bytes[3] = msg->data[0];
        *yaw = temp.f;
    }
    if (pitch)
    {
        temp.bytes[0] = msg->data[7];
        temp.bytes[1] = msg->data[6];
        temp.bytes[2] = msg->data[5];
        temp.bytes[3] = msg->data[4];
        *pitch = temp.f;
    }
    if (roll)
    {
        temp.bytes[0] = msg->data[11];
        temp.bytes[1] = msg->data[10];
        temp.bytes[2] = msg->data[9];
        temp.bytes[3] = msg->data[8];
        *roll = temp.f;
    }
}

void process_queue_message(const GameMessage *msg);

#endif
