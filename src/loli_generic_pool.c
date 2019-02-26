#include "loli_core_types.h"
#include "loli_generic_pool.h"
#include "loli_symtab.h"
#include "loli_type_maker.h"
#include "loli_alloc.h"

loli_generic_pool *loli_new_generic_pool(void)
{
    loli_generic_pool *gp = loli_malloc(sizeof(*gp));
    loli_class **cache_generics = loli_malloc(4 * sizeof(*cache_generics));
    loli_class **scope_generics = loli_malloc(4 * sizeof(*scope_generics));

    gp->cache_generics = cache_generics;
    gp->cache_size = 4;

    int i;
    for (i = 0;i < 4;i++)
        cache_generics[i] = NULL;

    gp->scope_generics = scope_generics;
    gp->scope_start = 0;
    gp->scope_end = 0;
    gp->scope_size = 4;

    return gp;
}

void loli_free_generic_pool(loli_generic_pool *gp)
{
    int i;
    for (i = 0;i < gp->cache_size;i++) {
        loli_class *c = gp->cache_generics[i];
        if (c == NULL)
            break;

        loli_free(c->self_type);
        loli_free(c->name);
        loli_free(c);
    }

    loli_free(gp->cache_generics);
    loli_free(gp->scope_generics);
    loli_free(gp);
}

static loli_class *find_in_cache(loli_generic_pool *gp, const char *name,
        int generic_pos, int *next_pos)
{
    int i = 0;
    loli_class *c = gp->cache_generics[i];

    while (c) {
        if (c->name[0] == name[0])
            return c;

        i++;
        c = gp->cache_generics[i];
    }

    *next_pos = i;
    return NULL;
}

void loli_gp_push(loli_generic_pool *gp, const char *name, int generic_pos)
{
    int i;
    loli_class *result = find_in_cache(gp, name, generic_pos, &i);

    if (result == NULL) {
        loli_class *new_generic = loli_new_raw_class(name);
        loli_type *t = loli_new_raw_type(new_generic);

        t->flags |= TYPE_IS_UNRESOLVED;
        t->generic_pos = generic_pos;

        new_generic->id = LOLI_ID_GENERIC;
        new_generic->self_type = t;
        new_generic->all_subtypes = t;

        result = new_generic;
        gp->cache_generics[i] = new_generic;

        if (i + 1 == gp->cache_size) {
            gp->cache_size *= 2;
            loli_class **new_cache = loli_realloc(gp->cache_generics,
                    gp->cache_size * sizeof(*new_cache));

            for (i = i + 1;i < gp->cache_size;i++)
                new_cache[i] = NULL;

            gp->cache_generics = new_cache;
        }
    }

    if (gp->scope_end == gp->scope_size) {
        gp->scope_size *= 2;
        loli_class **new_scope = loli_realloc(gp->scope_generics,
                gp->scope_size * sizeof(*new_scope));

        gp->scope_generics = new_scope;
    }

    gp->scope_generics[gp->scope_end] = result;
    gp->scope_end++;
}

loli_class *loli_gp_find(loli_generic_pool *gp, const char *name)
{
    char ch = name[0];
    int i;

    for (i = gp->scope_start;i < gp->scope_end;i++) {
        loli_class *c = gp->scope_generics[i];
        if (c->name[0] == ch)
            return c;
    }

    return NULL;
}

int loli_gp_num_in_scope(loli_generic_pool *gp)
{
    return gp->scope_end - gp->scope_start;
}

uint16_t loli_gp_save(loli_generic_pool *gp)
{
    return gp->scope_end;
}

void loli_gp_restore(loli_generic_pool *gp, uint16_t old_end)
{
    gp->scope_end = old_end;
}

uint16_t loli_gp_save_and_hide(loli_generic_pool *gp)
{
    uint16_t result = gp->scope_start;
    gp->scope_start = gp->scope_end;
    return result;
}

void loli_gp_restore_and_unhide(loli_generic_pool *gp, uint16_t old_start)
{
    gp->scope_end = gp->scope_start;
    gp->scope_start = old_start;
}
