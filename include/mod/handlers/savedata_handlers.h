#ifndef COOP_SAVEDATA_HANDLERS_H
#define COOP_SAVEDATA_HANDLERS_H

void handle_initial_save_data_request(const void *msg);
void handle_file_progress_flags(const void *msg);
void handle_ability_progress(const void *msg);
void handle_honeycomb_score(const void *msg);
void handle_mumbo_score(const void *msg);
void handle_note_save_data(const void *msg);

#endif
