#ifndef LOLI_VALUE_STACK_H
# define LOLI_VALUE_STACK_H

# include <stdint.h>

typedef struct loli_value_ loli_value;

typedef struct loli_value_stack_ {
    loli_value **data;
    uint32_t pos;
    uint32_t size;
} loli_value_stack;

loli_value_stack *loli_new_value_stack(void);
void loli_vs_push(loli_value_stack *, loli_value *);
loli_value *loli_vs_pop(loli_value_stack *);

#define loli_vs_pos(vs) vs->pos
#define loli_vs_nth(vs, n) vs->data[n]

void loli_free_value_stack(loli_value_stack *);

#endif
