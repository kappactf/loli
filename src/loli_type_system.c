#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "loli.h"

#include "loli_type_system.h"
#include "loli_alloc.h"

extern loli_type *loli_question_type;
extern loli_type *loli_unit_type;

# define ENSURE_TYPE_STACK(new_size) \
if (new_size >= ts->max) \
    grow_types(ts);

#define T_DONT_SOLVE 0x1

#define T_COVARIANT 0x2

#define T_CONTRAVARIANT 0x4

#define T_UNIFY 0x8

loli_type_system *loli_new_type_system(loli_type_maker *tm)
{
    loli_type_system *ts = loli_malloc(sizeof(*ts));
    loli_type **types = loli_malloc(4 * sizeof(*types));

    ts->tm = tm;
    ts->types = types;
    ts->base = types;
    ts->pos = 0;
    ts->max = 4;
    ts->max_seen = 1;
    ts->num_used = 0;
    ts->types[0] = loli_question_type;
    ts->scoop_count = 0;

    return ts;
}

void loli_free_type_system(loli_type_system *ts)
{
    loli_free(ts->types);
    loli_free(ts);
}

static void grow_types(loli_type_system *ts)
{
    ptrdiff_t offset = ts->base - ts->types;
    ts->max *= 2;
    ts->types = loli_realloc(ts->types, sizeof(*ts->types) * ts->max);
    ts->base = ts->types + offset;
}

static void do_scoop_resolve(loli_type_system *ts, loli_type *type)
{
    if ((type->flags & (TYPE_IS_UNRESOLVED | TYPE_HAS_SCOOP)) == 0)
        loli_tm_add_unchecked(ts->tm, type);
    else if (type->cls->generic_count != 0) {
        loli_tm_reserve(ts->tm, type->subtype_count + 1 + ts->num_used);

        loli_type **subtypes = type->subtypes;
        int start = ts->tm->pos;
        int i = 0;

        for (;i < type->subtype_count;i++)
            do_scoop_resolve(ts, subtypes[i]);

        loli_type *t;

        if (type->cls->id == LOLI_ID_FUNCTION)
            t = loli_tm_make_call(ts->tm, type->flags, type->cls,
                    ts->tm->pos - start);
        else
            t = loli_tm_make(ts->tm, type->cls, ts->tm->pos - start);

        loli_tm_add_unchecked(ts->tm, t);
    }
    else if (type->cls->id == LOLI_ID_GENERIC)
        loli_tm_add_unchecked(ts->tm, ts->base[type->generic_pos]);
    else if (type->cls->id == LOLI_ID_SCOOP) {
        int i;
        loli_type **base = ts->base + ts->num_used - ts->scoop_count;
        loli_tm_reserve(ts->tm, ts->scoop_count);

        for (i = 0;i < ts->scoop_count;i++)
            loli_tm_add_unchecked(ts->tm, base[i]);
    }
}

loli_type *loli_ts_resolve_unscoop(loli_type_system *ts, loli_type *type)
{
    do_scoop_resolve(ts, type);
    return loli_tm_pop(ts->tm);
}

loli_type *loli_ts_resolve(loli_type_system *ts, loli_type *type)
{
    loli_type *ret = type;

    if ((type->flags & (TYPE_IS_UNRESOLVED | TYPE_HAS_SCOOP)) == 0)
        ;
    else if (type->cls->generic_count != 0) {
         
        loli_tm_reserve(ts->tm, type->subtype_count);
        loli_type **subtypes = type->subtypes;
        int start = ts->tm->pos;
        int i = 0;

        for (;i < type->subtype_count;i++)
            loli_tm_add_unchecked(ts->tm, loli_ts_resolve(ts, subtypes[i]));

        if (type->cls->id == LOLI_ID_FUNCTION)
            ret = loli_tm_make_call(ts->tm, type->flags, type->cls,
                    ts->tm->pos - start);
        else
            ret = loli_tm_make(ts->tm, type->cls, ts->tm->pos - start);
    }
    else if (type->cls->id == LOLI_ID_GENERIC)
        ret = ts->base[type->generic_pos];

    return ret;
}

static void unify_call(loli_type_system *ts, loli_type *left,
        loli_type *right, int num_subtypes)
{
    loli_class *cls = left->cls;
    int flags = (left->flags & TYPE_IS_VARARGS) &
                (right->flags & TYPE_IS_VARARGS);
    loli_tm_add(ts->tm, loli_tm_make_call(ts->tm, flags, cls, num_subtypes));
}

static void unify_simple(loli_type_system *ts, loli_type *left,
        loli_type *right, int num_subtypes)
{
    loli_class *cls = left->cls->id < right->cls->id ? left->cls : right->cls;

    if (num_subtypes)
        loli_tm_add(ts->tm, loli_tm_make(ts->tm, cls, num_subtypes));
    else
        loli_tm_add(ts->tm, cls->self_type);
}

static int check_raw(loli_type_system *, loli_type *, loli_type *, int);

static int check_generic(loli_type_system *ts, loli_type *left,
        loli_type *right, int flags)
{
    int ret;
    if (flags & T_DONT_SOLVE) {
        ret = (left == right);
        if (ret && flags & T_UNIFY)
            loli_tm_add(ts->tm, left);
    }
    else {
        int generic_pos = left->generic_pos;
        loli_type *cmp_type = ts->base[generic_pos];
        ret = 1;

         
        if (right->cls->id == LOLI_ID_SCOOP)
            ret = 0;
        else if (cmp_type == loli_question_type)
            ts->base[generic_pos] = right;
        else if (cmp_type == right)
            ;
        else if (cmp_type->flags & TYPE_IS_INCOMPLETE) {
            loli_type *unify_type;
            unify_type = loli_ts_unify(ts, cmp_type, right);
            if (unify_type)
                ts->base[generic_pos] = unify_type;
            else
                ret = 0;
        }
        else
            ret = check_raw(ts, cmp_type, right, flags | T_DONT_SOLVE);
    }

    return ret;
}

static int check_function(loli_type_system *ts, loli_type *left,
        loli_type *right, int flags)
{
    int ret = 1;
    int tm_start = loli_tm_pos(ts->tm);
     
    loli_type *left_type = left->subtypes[0];
    loli_type *right_type = right->subtypes[0];

    flags &= T_DONT_SOLVE | T_UNIFY;

    if (check_raw(ts, left_type, right_type, flags | T_COVARIANT) == 0) {
         
        if (left_type == loli_unit_type) {
            if (flags & T_UNIFY) {
                loli_tm_restore(ts->tm, tm_start);
                loli_tm_add(ts->tm, loli_unit_type);
            }
        }
        else
            ret = 0;
    }

    int i;
    int count = left->subtype_count;

    if (left->subtype_count > right->subtype_count)
        count = right->subtype_count;

    flags |= T_CONTRAVARIANT;

    for (i = 1;i < count;i++) {
        left_type = left->subtypes[i];
        right_type = right->subtypes[i];

        if (right_type->cls->id == LOLI_ID_OPTARG &&
            left_type->cls->id != LOLI_ID_OPTARG) {
            right_type = right_type->subtypes[0];
        }

        if (check_raw(ts, left_type, right_type, flags) == 0) {
            ret = 0;
            break;
        }
    }

    if (left->subtype_count == right->subtype_count)
        ;
     
    else if (left->subtype_count > right->subtype_count) {
        if (left->subtypes[i]->cls->id == LOLI_ID_SCOOP &&
            (flags & T_UNIFY) == 0 &&
            ret == 1)
            ret = 1;
        else
            ret = 0;
    }
     
    else if (right->subtypes[i]->cls->id != LOLI_ID_OPTARG)
        ret = 0;

    if (ret && flags & T_UNIFY)
        unify_call(ts, left, right, left->subtype_count);

    return ret;
}

static int invariant_check(loli_type *left, loli_type *right, int *num_subtypes)
{
    int ret = left->cls == right->cls;
    *num_subtypes = left->subtype_count;

    return ret;
}

static int non_invariant_check(loli_type *left, loli_type *right, int *num_subtypes)
{
    int ret = loli_class_greater_eq(left->cls, right->cls);
    *num_subtypes = left->subtype_count;

    return ret;
}

static int check_misc(loli_type_system *ts, loli_type *left, loli_type *right,
        int flags)
{
    int ret;
    int num_subtypes;

    if (flags & T_COVARIANT)
        ret = non_invariant_check(left, right, &num_subtypes);
    else if (flags & T_CONTRAVARIANT)
         
        ret = non_invariant_check(right, left, &num_subtypes);
    else
        ret = invariant_check(left, right, &num_subtypes);

    if (ret && num_subtypes) {
         
        flags &= T_DONT_SOLVE | T_UNIFY;

        ret = 1;

        loli_type **left_subtypes = left->subtypes;
        loli_type **right_subtypes = right->subtypes;
        int i;
        for (i = 0;i < num_subtypes;i++) {
            loli_type *left_entry = left_subtypes[i];
            loli_type *right_entry = right_subtypes[i];
            if (check_raw(ts, left_entry, right_entry, flags) == 0) {
                ret = 0;
                break;
            }
        }
    }

    if (ret && flags & T_UNIFY)
        unify_simple(ts, left, right, num_subtypes);

    return ret;
}


static int check_tuple(loli_type_system *ts, loli_type *left, loli_type *right,
        int flags)
{
     
    if (left->subtype_count != right->subtype_count)
        return 0;

    return check_misc(ts, left, right, flags);
}

static int collect_scoop(loli_type_system *ts, loli_type *left,
        loli_type *right, int flags)
{
     
    if (flags & T_UNIFY)
        return 0;

    ENSURE_TYPE_STACK(ts->pos + ts->num_used + 1)

    ts->base[ts->num_used] = right;

    ts->num_used += 1;
    ts->scoop_count += 1;

    return 1;
}

static int check_raw(loli_type_system *ts, loli_type *left, loli_type *right, int flags)
{
    int ret = 0;

    if (left->cls->id == LOLI_ID_QUESTION) {
        ret = 1;
        if (flags & T_UNIFY)
            loli_tm_add(ts->tm, right);
    }
    else if (right->cls->id == LOLI_ID_QUESTION) {
        ret = 1;
        if (flags & T_UNIFY)
            loli_tm_add(ts->tm, left);
    }
    else if (left->cls->id == LOLI_ID_GENERIC)
        ret = check_generic(ts, left, right, flags);
    else if (left->cls->id == LOLI_ID_FUNCTION &&
             right->cls->id == LOLI_ID_FUNCTION)
        ret = check_function(ts, left, right, flags);
    else if (left->cls->id == LOLI_ID_TUPLE)
        ret = check_tuple(ts, left, right, flags);
    else if (left->cls->id == LOLI_ID_SCOOP)
         
        ret = collect_scoop(ts, left, right, flags);
    else
        ret = check_misc(ts, left, right, flags);

    return ret;
}

int loli_ts_check(loli_type_system *ts, loli_type *left, loli_type *right)
{
    return check_raw(ts, left, right, T_COVARIANT);
}

loli_type *loli_ts_unify(loli_type_system *ts, loli_type *left, loli_type *right)
{
    int save_pos = ts->tm->pos;
    int ok = check_raw(ts, left, right, T_DONT_SOLVE | T_COVARIANT | T_UNIFY);
    loli_type *result;

    if (ok)
        result = loli_tm_pop(ts->tm);
    else {
        ts->tm->pos = save_pos;
        result = NULL;
    }

    return result;
}

int loli_ts_type_greater_eq(loli_type_system *ts, loli_type *left, loli_type *right)
{
    return check_raw(ts, left, right, T_DONT_SOLVE | T_COVARIANT);
}

loli_type *loli_ts_resolve_by_second(loli_type_system *ts, loli_type *first,
        loli_type *second)
{
     
    loli_type **save_base = ts->base;
    ts->base = first->subtypes;
    loli_type *result_type = loli_ts_resolve(ts, second);
    ts->base = save_base;

    return result_type;
}

void loli_ts_reset_scoops(loli_type_system *ts)
{
    ts->scoop_count = 0;
}

#define COPY(to, from) \
to->pos = from->pos; \
to->num_used = from->num_used; \
to->scoop_count = from->scoop_count;

void loli_ts_scope_save(loli_type_system *ts, loli_ts_save_point *p)
{
    COPY(p, ts)

    ts->base += ts->num_used;
    ts->pos += ts->num_used;
    ts->num_used = ts->max_seen;
    ts->scoop_count = 0;

    ENSURE_TYPE_STACK(ts->pos + ts->num_used);

    int i;
    for (i = 0;i < ts->num_used;i++)
        ts->base[i] = loli_question_type;
}

void loli_ts_scope_restore(loli_type_system *ts, loli_ts_save_point *p)
{
    COPY(ts, p)
    ts->base -= ts->num_used;
}

void loli_ts_generics_seen(loli_type_system *ts, int amount)
{
    if (amount > ts->max_seen)
        ts->max_seen = amount;
}

int loli_func_type_num_optargs(loli_type *type)
{
    int i;
    for (i = type->subtype_count - 1;i > 0;i--) {
        loli_type *inner = type->subtypes[i];
        if (inner->cls->id != LOLI_ID_OPTARG)
            break;
    }


    return type->subtype_count - i - 1;
}

int loli_class_greater_eq(loli_class *left, loli_class *right)
{
    int ret = 0;
    if (left != right) {
        while (right != NULL) {
            right = right->parent;
            if (right == left) {
                ret = 1;
                break;
            }
        }
    }
    else
        ret = 1;

    return ret;
}

int loli_class_greater_eq_id(int left_id, loli_class *right)
{
    int ret = 0;

    while (right != NULL) {
        if (right->id == left_id) {
            ret = 1;
            break;
        }

        right = right->parent;
    }

    return ret;
}
