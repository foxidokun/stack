#include <assert.h>
#include "hash.h"

hash_t djb2 (const void *obj, size_t obj_size)
{
    assert (obj != nullptr && "Pointer can't be null");

    const unsigned char *obj_s = (const unsigned char *) obj;

    hash_t hash = 5381;

    for (; obj_size > 0; obj_size--)
    {
        hash = ((hash << 5) + hash) + obj_s[obj_size-1];
    }

    return hash;
}

hash_t strhash (const void *str, size_t obj_size)
{
    const unsigned char *str_c = (const unsigned char *) str;

    unsigned long hash = 5381;
    unsigned int c;

    while ((c = *str_c++) != 0)
    {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}