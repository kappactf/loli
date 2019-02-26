#include <string.h>

#include "loli_type_maker.h"
#include "loli_alloc.h"

#define BUBBLE_FLAGS \
    (TYPE_IS_UNRESOLVED | TYPE_IS_INCOMPLETE | TYPE_HAS_SCOOP | TYPE_HAS_OPTARGS)

loli_type_maker *loli_new_type_maker(void)
{
    loli_type_maker *tm = loli_malloc(sizeof(*tm));

    tm->types = loli_malloc(sizeof(*tm->types) * 4);
    tm->pos = 0;
    tm->size = 4;

    return tm;
}

loli_type *loli_new_raw_type(loli_class *cls)
{
    loli_type *new_type = loli_malloc(sizeof(*new_type));
    new_type->item_kind = ITEM_TYPE_TYPE;
    new_type->cls = cls;
    new_type->flags = 0;
    new_type->generic_pos = 0;
    new_type->subtype_count = 0;
    new_type->subtypes = NULL;
    new_type->next = NULL;

    return new_type;
}

void loli_tm_reserve(loli_type_maker *tm, int amount)
{
    if (tm->pos + amount > tm->size) {
        while (tm->pos + amount > tm->size)
            tm->size *= 2;

        tm->types = loli_realloc(tm->types, sizeof(*tm->types) * tm->size);
    }
}

void loli_tm_add_unchecked(loli_type_maker *tm, loli_type *type)
{
    tm->types[tm->pos] = type;
    tm->pos++;
}

void loli_tm_add(loli_type_maker *tm, loli_type *type)
{
    if (tm->pos + 1 == tm->size) {
        tm->size *= 2;
        tm->types = loli_realloc(tm->types, sizeof(*tm->types) * tm->size);
    }

    tm->types[tm->pos] = type;
    tm->pos++;
}

void loli_tm_insert(loli_type_maker *tm, int pos, loli_type *type)
{
    tm->types[pos] = type;
}

loli_type *loli_tm_pop(loli_type_maker *tm)
{
    tm->pos--;
    loli_type *result = tm->types[tm->pos];
    return result;
}

static loli_type *lookup_type(loli_type *input_type)
{
    loli_type *iter_type = input_type->cls->all_subtypes;
    loli_type *ret = NULL;

    while (iter_type) {
        if (iter_type->subtype_count == input_type->subtype_count &&
             
            (iter_type->flags & TYPE_IS_VARARGS) ==
                (input_type->flags & TYPE_IS_VARARGS)) {
            int i, match = 1;
            for (i = 0;i < iter_type->subtype_count;i++) {
                if (iter_type->subtypes[i] != input_type->subtypes[i]) {
                    match = 0;
                    break;
                }
            }

            if (match == 1) {
                ret = iter_type;
                break;
            }
        }

        iter_type = iter_type->next;
    }

    return ret;
}

static loli_type *build_real_type_for(loli_type *fake_type)
{
     
    loli_type *new_type = loli_new_raw_type(fake_type->cls);

    memcpy(new_type, fake_type, sizeof(loli_type));

    int count = fake_type->subtype_count;
    loli_type **new_subtypes = loli_malloc(count * sizeof(*new_subtypes));
    memcpy(new_subtypes, fake_type->subtypes, count * sizeof(*new_subtypes));
    new_type->subtypes = new_subtypes;
    new_type->subtype_count = count;

    new_type->next = new_type->cls->all_subtypes;
    new_type->cls->all_subtypes = new_type;
     
    new_type->flags &= TYPE_IS_VARARGS;

    int i;
    for (i = 0;i < new_type->subtype_count;i++) {
        loli_type *subtype = new_type->subtypes[i];
        if (subtype)
            new_type->flags |= subtype->flags & BUBBLE_FLAGS;
    }

    return new_type;
}

loli_type *loli_tm_make(loli_type_maker *tm, loli_class *cls, int num_entries)
{
    loli_type fake_type;

    fake_type.cls = cls;
    fake_type.generic_pos = 0;
    fake_type.subtypes = tm->types + (tm->pos - num_entries);
    fake_type.subtype_count = num_entries;
    fake_type.flags = 0;
    fake_type.next = NULL;

    loli_type *result_type = lookup_type(&fake_type);
    if (result_type == NULL) {
        fake_type.item_kind = ITEM_TYPE_TYPE;
        result_type = build_real_type_for(&fake_type);
    }

    tm->pos -= num_entries;
    return result_type;
}

loli_type *loli_tm_make_call(loli_type_maker *tm, int flags, loli_class *cls,
        int num_entries)
{
    loli_type fake_type;

    fake_type.cls = cls;
    fake_type.generic_pos = 0;
    fake_type.subtypes = tm->types + (tm->pos - num_entries);
    fake_type.subtype_count = num_entries;
    fake_type.flags = flags;
    fake_type.next = NULL;

    loli_type *result_type = lookup_type(&fake_type);
    if (result_type == NULL) {
        fake_type.item_kind = ITEM_TYPE_TYPE;
        result_type = build_real_type_for(&fake_type);
    }

    tm->pos -= num_entries;
    return result_type;
}

int loli_tm_pos(loli_type_maker *tm)
{
    return tm->pos;
}

void loli_tm_restore(loli_type_maker *tm, int pos)
{
    tm->pos = pos;
}

void loli_free_type_maker(loli_type_maker *tm)
{
    loli_free(tm->types);
    loli_free(tm);
}
