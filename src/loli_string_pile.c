#include <string.h>

#include "loli_string_pile.h"
#include "loli_alloc.h"

loli_string_pile *loli_new_string_pile(void)
{
    loli_string_pile *sp = loli_malloc(sizeof(*sp));

    sp->buffer = loli_malloc(64 * sizeof(*sp->buffer));
    sp->size = 63;

    return sp;
}

void loli_free_string_pile(loli_string_pile *sp)
{
    loli_free(sp->buffer);
    loli_free(sp);
}

void loli_sp_insert(loli_string_pile *sp, const char *new_str, uint16_t *pos)
{
    size_t want_size = *pos + 1 + strlen(new_str);
    if (sp->size < want_size) {
        while (sp->size < want_size)
            sp->size *= 2;

        char *new_buffer = loli_realloc(sp->buffer,
                sp->size * sizeof(*new_buffer));
        sp->buffer = new_buffer;
    }

    strcpy(sp->buffer + *pos, new_str);
    *pos = want_size;
}

char *loli_sp_get(loli_string_pile *sp, int pos)
{
    return sp->buffer + pos;
}
