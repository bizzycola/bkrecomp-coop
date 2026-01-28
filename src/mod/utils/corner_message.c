#import "corner_message.h"

// Define the global variables
UIMessageQueue ui_msg_queue = { .head = 0, .tail = 0, .count = 0 };
RecompuiContext ui_msg_context = NULL;
int ui_msg_display_timer = 0;
int ui_msg_is_visible = 0;

/**
 * Queue a message to be shown onscreen
 */
static void UIQueueMessageWithDuration(const char* message, int duration_frames) {
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
    ui_msg_queue.durations[ui_msg_queue.head] = duration_frames;

    ui_msg_queue.head = (ui_msg_queue.head + 1) % UI_MSG_QUEUE_SIZE;
    ui_msg_queue.count++;
}

static void UIQueueMessage(const char* message) {
    UIQueueMessageWithDuration(message, UI_MSG_DISPLAY_FRAMES);
}

/**
 * Dequeues a message from the queue and returns it
 */
static const char* UIDequeueMessageWithDuration(int* out_duration) {
    if (ui_msg_queue.count == 0) return NULL;
    
    const char* message = ui_msg_queue.messages[ui_msg_queue.tail];
    if (out_duration != NULL) {
        *out_duration = ui_msg_queue.durations[ui_msg_queue.tail];
    }

    ui_msg_queue.tail = (ui_msg_queue.tail + 1) % UI_MSG_QUEUE_SIZE;
    ui_msg_queue.count--;

    return message;
}

static const char* UIDequeueMessage(void) {
    return UIDequeueMessageWithDuration(NULL);
}

/**
 * Hides the currently displayed message
 */
static void UIHideMessage(void) {
    if (ui_msg_context != NULL) {
        recompui_hide_context(ui_msg_context);
        ui_msg_context = NULL;
    }
    ui_msg_is_visible = 0;
    ui_msg_display_timer = 0;
}

/**
 * Displays a message in the corner of the screen
 */
static void UIDisplayMessageWithDuration(const char* message, int duration_frames) {
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
    ui_msg_display_timer = duration_frames;
}

static void UIDisplayMessage(const char* message) {
    UIDisplayMessageWithDuration(message, UI_MSG_DISPLAY_FRAMES);
}

/**
 * Updates the message queue, showing/hiding messages as needed
 */
void UIUpdateMessageQueue(void) {
    if (ui_msg_is_visible) {
        ui_msg_display_timer--;
        if (ui_msg_display_timer <= 0) {
            UIHideMessage();
            int next_duration = UI_MSG_DISPLAY_FRAMES;
            const char* next = UIDequeueMessageWithDuration(&next_duration);
            if (next != NULL) UIDisplayMessageWithDuration(next, next_duration);
        }
    } else {
        int next_duration = UI_MSG_DISPLAY_FRAMES;
        const char* next = UIDequeueMessageWithDuration(&next_duration);
        if (next != NULL) UIDisplayMessageWithDuration(next, next_duration);
    }
}

void ShowCornerMessageWithParams(const char* message, int duration_frames, int immediate) {
    int actual_duration = (duration_frames > 0) ? duration_frames : UI_MSG_DISPLAY_FRAMES;
    
    if (immediate) {
        UIHideMessage();
        UIDisplayMessageWithDuration(message, actual_duration);
    } else if (!ui_msg_is_visible) {
        UIDisplayMessageWithDuration(message, actual_duration);
    } else {
        UIQueueMessageWithDuration(message, actual_duration);
    }
}

/**
 * Shows a corner message, queuing it if another message is visible
 */
void ShowCornerMessage(const char* message) {
   ShowCornerMessageWithParams(message, UI_MSG_DISPLAY_FRAMES, 0);
}