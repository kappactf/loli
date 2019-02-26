#include "loli_alloc.h"

void *loli_malloc(size_t size)
{
    void *result = malloc(size);
    if (result == NULL)
        abort();

    return result;
}

void *loli_realloc(void *ptr, size_t new_size)
{
    void *result = realloc(ptr, new_size);
    if (result == NULL)
        abort();

    return result;
}

void loli_free(void *ptr)
{
    free(ptr);
}
