#include "loli_value_stack.h"
#include "loli_alloc.h"

loli_value_stack *loli_new_value_stack(void)
{
    loli_value_stack *vs = loli_malloc(sizeof(*vs));

    vs->data = loli_malloc(4 * sizeof(*vs->data));
    vs->pos = 0;
    vs->size = 4;

    return vs;
}

void loli_vs_push(loli_value_stack *vs, loli_value *value)
{
    if (vs->pos + 1 > vs->size) {
        vs->size *= 2;
        vs->data = loli_realloc(vs->data, vs->size * sizeof(*vs->data));
    }

    vs->data[vs->pos] = value;
    vs->pos++;
}

loli_value *loli_vs_pop(loli_value_stack *vs)
{
    vs->pos--;
    loli_value *result = vs->data[vs->pos];
    return result;
}

void loli_free_value_stack(loli_value_stack *vs)
{
    loli_free(vs->data);
    loli_free(vs);
}
