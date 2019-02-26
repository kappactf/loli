#ifndef LOLI_RAISER_H
# define LOLI_RAISER_H

# include <setjmp.h>

# include "loli.h"

# include "loli_core_types.h"

typedef struct loli_jump_link_ {
    struct loli_jump_link_ *prev;
    struct loli_jump_link_ *next;

    jmp_buf jump;
} loli_jump_link;

typedef struct loli_raiser_ {
    loli_jump_link *all_jumps;

     
    loli_msgbuf *msgbuf;

     
    loli_msgbuf *aux_msgbuf;

     
    uint32_t line_adjust;
    uint32_t is_syn_error;
} loli_raiser;

loli_raiser *loli_new_raiser(void);
void loli_free_raiser(loli_raiser *);
void loli_raise_syn(loli_raiser *, const char *, ...);
void loli_raise_err(loli_raiser *, const char *, ...);
loli_jump_link *loli_jump_setup(loli_raiser *);
void loli_release_jump(loli_raiser *);

const char *loli_name_for_error(loli_raiser *);

#endif
