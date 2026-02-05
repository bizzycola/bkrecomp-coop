#ifndef COOP_STATUS_HANDLERS_H
#define COOP_STATUS_HANDLERS_H

void handle_connection_status(const void *msg);
void handle_connection_error(const void *msg);
void handle_level_opened(const void *msg);
void handle_puppet_update(const void *msg);
void handle_console_toggle(const void *msg);
void handle_console_key(const void *msg);

#endif
