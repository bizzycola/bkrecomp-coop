
void *memset(void *s, int c, unsigned long n)
{
    unsigned char *p = (unsigned char *)s;
    unsigned long i;

    for (i = 0; i < n; i++)
    {
        p[i] = (unsigned char)c;
    }

    return s;
}

void *memcpy(void *dest, const void *src, unsigned long n)
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
