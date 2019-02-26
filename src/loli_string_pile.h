#ifndef LOLI_STRING_PILE_H
# define LOLI_STRING_PILE_H

# include <stdint.h>


typedef struct  {
    char *buffer;
    uint32_t size;
    uint32_t pad;
} loli_string_pile;

loli_string_pile *loli_new_string_pile(void);

void loli_free_string_pile(loli_string_pile *);

void loli_sp_insert(loli_string_pile *, const char *, uint16_t *);

char *loli_sp_get(loli_string_pile *, int);

#endif
