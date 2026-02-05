#include "util.h"

void util_safe_strcpy(char *dest, const char *src, int max_len)
{
    int i;
    for (i = 0; i < max_len - 1 && src[i] != '\0'; i++)
    {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

void util_safe_strcat(char *dest, const char *src, int max_len)
{
    int dest_len = 0;
    while (dest[dest_len] != '\0' && dest_len < max_len - 1)
    {
        dest_len++;
    }

    int i = 0;
    while (src[i] != '\0' && dest_len < max_len - 1)
    {
        dest[dest_len++] = src[i++];
    }
    dest[dest_len] = '\0';
}

int util_str_equals(const char *a, const char *b)
{
    int i = 0;
    while (a[i] != '\0' && b[i] != '\0')
    {
        if (a[i] != b[i])
            return 0;
        i++;
    }
    return a[i] == b[i];
}

int util_str_length(const char *str)
{
    int len = 0;
    while (str[len] != '\0')
    {
        len++;
    }
    return len;
}

void *util_memset(void *s, int c, unsigned long n)
{
    unsigned char *p = (unsigned char *)s;
    unsigned long i;

    for (i = 0; i < n; i++)
    {
        p[i] = (unsigned char)c;
    }

    return s;
}

void *util_memcpy(void *dest, const void *src, unsigned long n)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    unsigned long i;

    for (i = 0; i < n; i++)
    {
        d[i] = s[i];
    }

    return dest;
}

void *memset(void *s, int c, unsigned long n)
{
    return util_memset(s, c, n);
}

void *memcpy(void *dest, const void *src, unsigned long n)
{
    return util_memcpy(dest, src, n);
}

int util_pack_xy_coords(s16 x, s16 y)
{
    return (((int)(u16)y) << 16) | ((u16)x);
}

int util_positions_match_tolerance(s16 x1, s16 y1, s16 z1, s16 x2, s16 y2, s16 z2, int tolerance)
{
    int dx = x1 - x2;
    int dy = y1 - y2;
    int dz = z1 - z2;

    if (dx < 0)
        dx = -dx;
    if (dy < 0)
        dy = -dy;
    if (dz < 0)
        dz = -dz;

    return (dx <= tolerance && dy <= tolerance && dz <= tolerance);
}

void util_send_blob_if_valid(GetSizeAndPtrFunc getter, NativeSendFunc sender)
{
    void *ptr = NULL;
    s32 size = 0;

    if (!getter || !sender)
    {
        return;
    }

    getter(&size, &ptr);

    if (ptr != NULL && size > 0)
    {
        sender(ptr, size);
    }
}
