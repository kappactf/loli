#ifndef LOLI_TYPE_MAKER_H
# define LOLI_TYPE_MAKER_H

# include "loli_core_types.h"

typedef struct {
    loli_type **types;
    uint32_t pos;
    uint32_t size;
} loli_type_maker;

loli_type_maker *loli_new_type_maker(void);
void loli_tm_add(loli_type_maker *, loli_type *);
void loli_tm_add_unchecked(loli_type_maker *, loli_type *);
void loli_tm_insert(loli_type_maker *, int, loli_type *);
void loli_tm_reserve(loli_type_maker *, int);
loli_type *loli_tm_pop(loli_type_maker *);
loli_type *loli_tm_make(loli_type_maker *, loli_class *, int);
loli_type *loli_tm_make_call(loli_type_maker *, int, loli_class *, int);
int loli_tm_pos(loli_type_maker *);
void loli_tm_restore(loli_type_maker *, int);

void loli_free_type_maker(loli_type_maker *);

loli_type *loli_new_raw_type(loli_class *);

#endif
