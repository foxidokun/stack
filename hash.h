#ifndef HASH_H
#define HASH_H

#include <stdint.h>
#include <stdlib.h>

typedef uint64_t hash_t;
typedef hash_t (*hash_f) (const void *obj, size_t obj_size);

hash_t djb2 (const void *obj, size_t obj_size);

#endif