#ifndef TOAST_H
#define TOAST_H

#include "recompui.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define TOAST_QUEUE_SIZE 16
#define TOAST_MAX_LENGTH 256
#define TOAST_DEFAULT_DURATION 180

    typedef enum
    {
        TOAST_POS_TOP_LEFT,
        TOAST_POS_TOP_CENTER,
        TOAST_POS_TOP_RIGHT,
        TOAST_POS_BOTTOM_LEFT,
        TOAST_POS_BOTTOM_CENTER,
        TOAST_POS_BOTTOM_RIGHT,
        TOAST_POS_CENTER,
    } ToastPosition;

    typedef enum
    {
        TOAST_SIZE_SMALL,
        TOAST_SIZE_MEDIUM,
        TOAST_SIZE_LARGE,
    } ToastSize;

    typedef enum
    {
        TOAST_STYLE_INFO,
        TOAST_STYLE_SUCCESS,
        TOAST_STYLE_WARNING,
        TOAST_STYLE_ERROR,
    } ToastStyle;

    typedef struct
    {
        char message[TOAST_MAX_LENGTH];
        int duration_frames;
        ToastPosition position;
        ToastSize size;
        ToastStyle style;
    } ToastConfig;

    typedef struct
    {
        ToastConfig toasts[TOAST_QUEUE_SIZE];
        int head;
        int tail;
        int count;
    } ToastQueue;

    typedef struct
    {
        RecompuiContext context;
        int display_timer;
        int is_visible;
        ToastConfig current;
    } ToastState;

    extern ToastQueue g_toast_queue;
    extern ToastState g_toast_state;

    void toast_init(void);
    void toast_update(void);
    void toast_show(const char *message);
    void toast_show_timed(const char *message, int duration_frames);
    void toast_show_custom(const char *message, int duration_frames,
                           ToastPosition position, ToastSize size, ToastStyle style);
    void toast_show_immediate(const char *message);
    void toast_show_immediate_custom(const char *message, int duration_frames,
                                     ToastPosition position, ToastSize size, ToastStyle style);
    void toast_clear_queue(void);
    void toast_hide_current(void);
    void toast_info(const char *message);
    void toast_success(const char *message);
    void toast_warning(const char *message);
    void toast_error(const char *message);

#ifdef __cplusplus
}
#endif

#endif
