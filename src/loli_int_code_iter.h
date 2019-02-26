#ifndef LOLI_INT_CODE_ITER_H
# define LOLI_INT_CODE_ITER_H

# include <stdint.h>

typedef struct {
    uint16_t *buffer;

    uint16_t offset;
    uint16_t stop;
    uint16_t round_total;
    uint16_t opcode;

    uint16_t special_1;
    uint16_t counter_2;
    uint16_t inputs_3;
    uint16_t outputs_4;

    uint16_t jumps_5;
    uint16_t line_6;
    uint32_t pad;
} loli_code_iter;

struct loli_function_val_;

void loli_ci_init(loli_code_iter *, uint16_t *, uint16_t, uint16_t);
int loli_ci_next(loli_code_iter *);

void loli_ci_from_native(loli_code_iter *, struct loli_function_val_ *);

#endif
