#ifndef UTIL_H
#define UTIL_H

#ifndef _ULTRATYPES_H_
typedef short s16;
typedef long s32;
typedef unsigned short u16;
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

void util_safe_strcpy(char *dest, const char *src, int max_len);
void util_safe_strcat(char *dest, const char *src, int max_len);
int util_str_equals(const char *a, const char *b);
int util_str_length(const char *str);

void *util_memset(void *s, int c, unsigned long n);
void *util_memcpy(void *dest, const void *src, unsigned long n);

int util_pack_xy_coords(s16 x, s16 y);
int util_positions_match_tolerance(s16 x1, s16 y1, s16 z1, s16 x2, s16 y2, s16 z2, int tolerance);

typedef void (*GetSizeAndPtrFunc)(s32 *sizeOut, void **ptrOut);
typedef void (*NativeSendFunc)(void *data, int size);
void util_send_blob_if_valid(GetSizeAndPtrFunc getter, NativeSendFunc sender);

#endif
