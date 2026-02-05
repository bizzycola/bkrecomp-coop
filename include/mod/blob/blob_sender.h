#ifndef BLOB_SENDER_H
#define BLOB_SENDER_H

void send_ability_progress_blob(void);
void send_honeycomb_score_blob(void);
void send_mumbo_score_blob(void);

int send_ability_progress_blob_cmd(int argc, char **argv);
int send_honeycomb_score_blob_cmd(int argc, char **argv);
int send_mumbo_score_blob_cmd(int argc, char **argv);

#endif
