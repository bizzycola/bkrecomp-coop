#include "blob/blob_sender.h"
#include "modding.h"
#include "../collection/collection.h"
#include "console/console.h"

RECOMP_IMPORT(".", void native_send_ability_progress(void *moves_data, int size));
RECOMP_IMPORT(".", void native_send_honeycomb_score(void *score_data, int size));
RECOMP_IMPORT(".", void native_send_mumbo_score(void *score_data, int size));

extern void ability_getSizeAndPtr(s32 *sizeOut, void **ptrOut);
extern void honeycombscore_getSizeAndPtr(s32 *sizeOut, void **ptrOut);
extern void mumboscore_getSizeAndPtr(s32 *sizeOut, void **ptrOut);

void send_ability_progress_blob(void)
{
    void *ptr = NULL;
    s32 size = 0;
    ability_getSizeAndPtr(&size, &ptr);

    if (ptr != NULL && size > 0)
    {
        native_send_ability_progress(ptr, size);
    }
}

int send_ability_progress_blob_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (applying_remote_state())
    {
        return 1;
    }

    send_ability_progress_blob();
    console_log_info("Sent ability progress blob");

    return 1;
}

void send_honeycomb_score_blob(void)
{
    void *ptr = NULL;
    s32 size = 0;
    honeycombscore_getSizeAndPtr(&size, &ptr);

    if (ptr != NULL && size > 0)
    {
        native_send_honeycomb_score(ptr, size);
    }
}

int send_honeycomb_score_blob_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (applying_remote_state())
    {
        return 1;
    }

    send_honeycomb_score_blob();
    console_log_info("Sent Honeycomb score blob");

    return 1;
}

void send_mumbo_score_blob(void)
{
    void *ptr = NULL;
    s32 size = 0;
    mumboscore_getSizeAndPtr(&size, &ptr);

    if (ptr != NULL && size > 0)
    {
        native_send_mumbo_score(ptr, size);
    }
}

int send_mumbo_score_blob_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (applying_remote_state())
    {
        return 1;
    }

    send_mumbo_score_blob();
    console_log_info("Sent Mumbo score blob");

    return 1;
}
