#ifndef CONSOLE_H
#define CONSOLE_H

#include "recompui.h"

#define CONSOLE_MAX_HISTORY 20
#define CONSOLE_MAX_INPUT 128
#define CONSOLE_MAX_COMMAND_NAME 32
#define CONSOLE_MAX_COMMANDS 32

// Command handler function type
// Returns 1 on success, 0 on failure
typedef int (*ConsoleCommandHandler)(int argc, char **argv);

// Console command structure
typedef struct {
    char name[CONSOLE_MAX_COMMAND_NAME];
    ConsoleCommandHandler handler;
    const char *description;
    int is_active;
} ConsoleCommand;

// Initialize the console system
void console_init(void);

// Show/hide the console
void console_show(void);
void console_hide(void);
void console_toggle(void);
int console_is_visible(void);

// Add a message to the console history
void console_log(const char *message);
void console_log_info(const char *message);
void console_log_success(const char *message);
void console_log_error(const char *message);
void console_log_warning(const char *message);

// Register a command
// Returns 1 on success, 0 if command table is full
int console_register_command(const char *name, ConsoleCommandHandler handler, const char *description);

// Handle keyboard input from lib side
void console_handle_key(char key);
void console_handle_backspace(void);
void console_handle_enter(void);

void console_update(void);

#endif // CONSOLE_H
