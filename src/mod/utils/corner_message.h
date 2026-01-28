
#include "modding.h"
#include "functions.h"
#include "recompui.h"

#define UI_MSG_QUEUE_SIZE 16
#define UI_MSG_MAX_LENGTH 128
#define UI_MSG_DISPLAY_FRAMES 180


#define UI_MSG_QUEUE_SIZE 16
#define UI_MSG_MAX_LENGTH 128
#define UI_MSG_DISPLAY_FRAMES 180


typedef struct {
    char messages[UI_MSG_QUEUE_SIZE][UI_MSG_MAX_LENGTH];
    int durations[UI_MSG_QUEUE_SIZE];
    int head;
    int tail;
    int count;
} UIMessageQueue;

extern UIMessageQueue ui_msg_queue;
extern RecompuiContext ui_msg_context;
extern int ui_msg_display_timer;
extern int ui_msg_is_visible;

extern enum map_e map_get(void);
extern s32 level_get(void);

void ShowCornerMessage(const char* message);
void ShowCornerMessageWithParams(const char* message, int duration_frames, int immediate);
void UIUpdateMessageQueue(void);