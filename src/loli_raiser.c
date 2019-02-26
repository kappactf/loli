#include <stdarg.h>
#include <string.h>

#include "loli_raiser.h"
#include "loli_alloc.h"

loli_raiser *loli_new_raiser(void)
{
    loli_raiser *raiser = loli_malloc(sizeof(*raiser));
    loli_jump_link *first_jump = loli_malloc(sizeof(*first_jump));
    first_jump->prev = NULL;
    first_jump->next = NULL;

    raiser->msgbuf = loli_new_msgbuf(64);
    raiser->aux_msgbuf = loli_new_msgbuf(64);
    raiser->all_jumps = first_jump;
    raiser->line_adjust = 0;

    return raiser;
}

void loli_free_raiser(loli_raiser *raiser)
{
    loli_jump_link *jump_next;

     
    while (raiser->all_jumps) {
        jump_next = raiser->all_jumps->next;
        loli_free(raiser->all_jumps);
        raiser->all_jumps = jump_next;
    }

    loli_free_msgbuf(raiser->aux_msgbuf);
    loli_free_msgbuf(raiser->msgbuf);
    loli_free(raiser);
}

loli_jump_link *loli_jump_setup(loli_raiser *raiser)
{
    if (raiser->all_jumps->next)
        raiser->all_jumps = raiser->all_jumps->next;
    else {
        loli_jump_link *new_link = loli_malloc(sizeof(*new_link));
        new_link->prev = raiser->all_jumps;
        raiser->all_jumps->next = new_link;

        new_link->next = NULL;

        raiser->all_jumps = new_link;
    }

    return raiser->all_jumps;
}

void loli_release_jump(loli_raiser *raiser)
{
    raiser->all_jumps = raiser->all_jumps->prev;
}

void loli_raise_syn(loli_raiser *raiser, const char *fmt, ...)
{
    loli_mb_flush(raiser->msgbuf);

    va_list var_args;
    va_start(var_args, fmt);
    loli_mb_add_fmt_va(raiser->msgbuf, fmt, var_args);
    va_end(var_args);

    raiser->is_syn_error = 1;
    longjmp(raiser->all_jumps->jump, 1);
}

void loli_raise_err(loli_raiser *raiser, const char *fmt, ...)
{
    loli_mb_flush(raiser->msgbuf);

    va_list var_args;
    va_start(var_args, fmt);
    loli_mb_add_fmt_va(raiser->msgbuf, fmt, var_args);
    va_end(var_args);

    raiser->is_syn_error = 0;
    longjmp(raiser->all_jumps->jump, 1);
}

const char *loli_name_for_error(loli_raiser *raiser)
{
    const char *result;

    if (raiser->is_syn_error)
        result = "SynError";
    else
        result = "Error";

    return result;
}
