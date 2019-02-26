#ifndef LOLI_GENERIC_POOL_H
# define LOLI_GENERIC_POOL_H

typedef struct {
    struct loli_class_ **cache_generics;
    struct loli_class_ **scope_generics;
    uint16_t cache_size;

    uint16_t scope_start;
    uint16_t scope_end;
    uint16_t scope_size;
} loli_generic_pool;

loli_generic_pool *loli_new_generic_pool(void);

struct loli_class_ *loli_gp_find(loli_generic_pool *, const char *);

void loli_gp_push(loli_generic_pool *, const char *, int);

int loli_gp_num_in_scope(loli_generic_pool *);

uint16_t loli_gp_save(loli_generic_pool *);

uint16_t loli_gp_save_and_hide(loli_generic_pool *);

void loli_gp_restore(loli_generic_pool *, uint16_t);

void loli_gp_restore_and_unhide(loli_generic_pool *, uint16_t);

void loli_free_generic_pool(loli_generic_pool *);

#endif
