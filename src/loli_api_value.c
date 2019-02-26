#include <string.h>

#include "loli.h"

#include "loli_value_structs.h"
#include "loli_vm.h"
#include "loli_value_flags.h"
#include "loli_alloc.h"
#include "loli_value_raw.h"

#define DEFINE_GETTERS(name, action, ...) \
int loli_##name##_boolean(__VA_ARGS__) \
{ return source  action->value.integer; } \
uint8_t loli_##name##_byte(__VA_ARGS__) \
{ return source  action->value.integer; } \
loli_bytestring_val *loli_##name##_bytestring(__VA_ARGS__) \
{ return (loli_bytestring_val *)source  action->value.string; } \
loli_container_val *loli_##name##_container(__VA_ARGS__) \
{ return source  action->value.container; } \
loli_coroutine_val *loli_##name##_coroutine(__VA_ARGS__) \
{ return source  action->value.coroutine; } \
double loli_##name##_double(__VA_ARGS__) \
{ return source  action->value.doubleval; } \
loli_file_val *loli_##name##_file(__VA_ARGS__) \
{ return source  action->value.file; } \
loli_function_val *loli_##name##_function(__VA_ARGS__) \
{ return source  action->value.function; } \
loli_hash_val *loli_##name##_hash(__VA_ARGS__) \
{ return source  action->value.hash; } \
loli_generic_val *loli_##name##_generic(__VA_ARGS__) \
{ return source  action->value.generic; } \
int64_t loli_##name##_integer(__VA_ARGS__) \
{ return source  action->value.integer; } \
loli_string_val *loli_##name##_string(__VA_ARGS__) \
{ return source  action->value.string; } \
char *loli_##name##_string_raw(__VA_ARGS__) \
{ return source  action->value.string->string; } \

DEFINE_GETTERS(arg, ->call_chain->start[index], loli_state *source, int index)
DEFINE_GETTERS(as, , loli_value *source)

char* loli_get_version()
{
    return LOLI_VERSION;
}

loli_value *loli_arg_value(loli_state *s, int index)
{
    return s->call_chain->start[index];
}

loli_value *loli_con_get(loli_container_val *c, int index)
{
    return c->values[index];
}

void loli_con_set(loli_container_val *c, int index, loli_value *v)
{
    loli_value_assign(c->values[index], v);
}

void loli_con_set_from_stack(loli_state *s, loli_container_val *c, int index)
{
    loli_value *target = c->values[index];

    if (target->flags & VAL_IS_DEREFABLE)
        loli_deref(target);

    s->call_chain->top--;

    loli_value *top = *(s->call_chain->top);
    *target = *top;

    top->flags = 0;
}

uint32_t loli_con_size(loli_container_val *c)
{
    return c->num_values;
}

int loli_arg_count(loli_state *s)
{
    return (int)(s->call_chain->top - s->call_chain->start);
}

int loli_arg_isa(loli_state *s, int index, uint16_t class_id)
{
    loli_value *value = s->call_chain->start[index];
    int base = FLAGS_TO_BASE(value);
    uint16_t result_id;

    switch (base) {
        case V_VARIANT_BASE:
        case V_INSTANCE_BASE:
        case V_FOREIGN_BASE:
            result_id = (uint16_t) value->value.container->class_id;
            break;
        case V_EMPTY_VARIANT_BASE:
            result_id = (uint16_t) value->value.integer;
            break;
        case V_COROUTINE_BASE:
            result_id = LOLI_ID_COROUTINE;
            break;

        case V_UNIT_BASE:
            result_id = LOLI_ID_UNIT;
            break;

        default:
            result_id = (uint16_t) base;
    }

    return result_id == class_id;
}


loli_value *loli_stack_take(loli_state *s)
{
    s->call_chain->top--;
    return *s->call_chain->top;
}

void loli_stack_push_and_destroy(loli_state *s, loli_value *v)
{
    loli_push_value(s, v);
    loli_deref(v);
    loli_free(v);
}

loli_value *loli_stack_get_top(loli_state *s)
{
    return *(s->call_chain->top - 1);
}

void loli_stack_drop_top(loli_state *s)
{
    s->call_chain->top--;
    loli_value *z = *s->call_chain->top;
    loli_deref(z);
    z->flags = 0;
}


void loli_list_take(loli_state *s, loli_container_val *c, int index)
{
    loli_value *v = c->values[index];
    loli_push_value(s, v);

    loli_deref(v);
    loli_free(v);

    if (index != c->num_values)
        memmove(c->values + index, c->values + index + 1,
                (c->num_values - index - 1) * sizeof(*c->values));

    c->num_values--;
    c->extra_space++;
}

static void grow_list(loli_container_val *lv)
{
     
    int extra = (lv->num_values + 8) >> 2;
    lv->values = loli_realloc(lv->values,
            (lv->num_values + extra) * sizeof(*lv->values));
    lv->extra_space = extra;
}

void loli_list_reserve(loli_container_val *c, int new_size)
{
    int size = c->num_values + c->extra_space;

    if (size > new_size)
        return;

    if (size == 0)
        size = 8;

    while (size < new_size)
        size *= 2;

    c->values = loli_realloc(c->values, size * sizeof(*c->values));
    c->extra_space = size - c->num_values;
}

void loli_list_push(loli_container_val *c, loli_value *v)
{
    if (c->extra_space == 0)
        grow_list(c);

    c->values[c->num_values] = loli_value_copy(v);
    c->num_values++;
    c->extra_space--;
}

void loli_list_insert(loli_container_val *c, int index, loli_value *v)
{
    if (c->extra_space == 0)
        grow_list(c);

    if (index != c->num_values)
        memmove(c->values + index + 1, c->values + index,
                (c->num_values - index) * sizeof(*c->values));

    c->values[index] = loli_value_copy(v);
    c->num_values++;
    c->extra_space--;
}

char *loli_bytestring_raw(loli_bytestring_val *sv)
{
    return sv->string;
}

int loli_bytestring_length(loli_bytestring_val *sv)
{
    return sv->size;
}

FILE *loli_file_for_write(loli_state *s, loli_file_val *filev)
{
    if (filev->inner_file == NULL)
        loli_IOError(s, "IO operation on closed file.");

    if (filev->write_ok == 0)
        loli_IOError(s, "File not open for writing.");

    return filev->inner_file;
}

FILE *loli_file_for_read(loli_state *s, loli_file_val *filev)
{
    if (filev->inner_file == NULL)
        loli_IOError(s, "IO operation on closed file.");

    if (filev->read_ok == 0)
        loli_IOError(s, "File not open for reading.");

    return filev->inner_file;
}

int loli_function_is_foreign(loli_function_val *fv)
{
    return fv->code == NULL;
}

int loli_function_is_native(loli_function_val *fv)
{
    return fv->code != NULL;
}

char *loli_string_raw(loli_string_val *sv)
{
    return sv->string;
}

int loli_string_length(loli_string_val *sv)
{
    return sv->size;
}


extern void loli_destroy_hash(loli_value *);
extern loli_gc_entry *loli_gc_stopper;

static void destroy_container(loli_value *v)
{
    loli_container_val *iv = v->value.container;
    if (iv->gc_entry == loli_gc_stopper)
        return;

    int full_destroy = 1;
    if (iv->gc_entry) {
        if (iv->gc_entry->last_pass == -1) {
            full_destroy = 0;
            iv->gc_entry = loli_gc_stopper;
        }
        else
            iv->gc_entry->value.generic = NULL;
    }

    int i;
    for (i = 0;i < iv->num_values;i++) {
        loli_deref(iv->values[i]);
        loli_free(iv->values[i]);
    }

    loli_free(iv->values);

    if (full_destroy)
        loli_free(iv);
}

static void destroy_list(loli_value *v)
{
    loli_container_val *lv = v->value.container;

    int i;
    for (i = 0;i < lv->num_values;i++) {
        loli_deref(lv->values[i]);
        loli_free(lv->values[i]);
    }

    loli_free(lv->values);
    loli_free(lv);
}

static void destroy_string(loli_value *v)
{
    loli_string_val *sv = v->value.string;

    loli_free(sv->string);
    loli_free(sv);
}

static void destroy_function(loli_value *v)
{
    loli_function_val *fv = v->value.function;
    if (fv->gc_entry == loli_gc_stopper)
        return;

    int full_destroy = 1;

    if (fv->gc_entry) {
        if (fv->gc_entry->last_pass == -1) {
            full_destroy = 0;
            fv->gc_entry = loli_gc_stopper;
        }
        else
            fv->gc_entry->value.generic = NULL;
    }

    loli_value **upvalues = fv->upvalues;
    int count = fv->num_upvalues;
    int i;

    for (i = 0;i < count;i++) {
        loli_value *up = upvalues[i];
        if (up) {
            up->cell_refcount--;

            if (up->cell_refcount == 0) {
                loli_deref(up);
                loli_free(up);
            }
        }
    }
    loli_free(upvalues);

    if (full_destroy)
        loli_free(fv);
}

static void destroy_file(loli_value *v)
{
    loli_file_val *filev = v->value.file;

    if (filev->inner_file && filev->is_builtin == 0)
        fclose(filev->inner_file);

    loli_free(filev);
}

void loli_destroy_vm(loli_vm_state *);

static void destroy_coroutine(loli_value *v)
{
    loli_coroutine_val *co_val = v->value.coroutine;
    if (co_val->gc_entry == loli_gc_stopper)
        return;

    int full_destroy = 1;

     

    if (co_val->gc_entry->last_pass == -1) {
        full_destroy = 0;
        co_val->gc_entry = loli_gc_stopper;
    }
    else
        co_val->gc_entry->value.generic = NULL;

    loli_value *receiver = co_val->receiver;

     
    if (receiver->flags & VAL_IS_DEREFABLE)
        loli_deref(receiver);

    loli_vm_state *base_vm = co_val->vm;

     
    loli_free_raiser(base_vm->raiser);

     
    loli_value func_v;
    func_v.flags = V_FUNCTION_BASE;
    func_v.value.function = co_val->base_function;

    destroy_function(&func_v);

    loli_destroy_vm(base_vm);
    loli_free(base_vm);
    loli_free(receiver);

    if (full_destroy)
        loli_free(co_val);
}

void loli_value_destroy(loli_value *v)
{
    switch (FLAGS_TO_BASE(v)) {
        case V_LIST_BASE:
        case V_TUPLE_BASE:
            destroy_list(v);
            break;

        case V_VARIANT_BASE:
        case V_INSTANCE_BASE:
            destroy_container(v);
            break;

        case V_STRING_BASE:
        case V_BYTESTRING_BASE:
            destroy_string(v);
            break;

        case V_FUNCTION_BASE:
            destroy_function(v);
            break;

        case V_HASH_BASE:
            loli_destroy_hash(v);
            break;

        case V_FILE_BASE:
            destroy_file(v);

        case V_COROUTINE_BASE:
            destroy_coroutine(v);
            break;

        case V_FOREIGN_BASE:
            v->value.foreign->destroy_func(v->value.generic);
            loli_free(v->value.generic);
            break;
    }
}

void loli_deref(loli_value *value)
{
    if (value->flags & VAL_IS_DEREFABLE) {
        value->value.generic->refcount--;
        if (value->value.generic->refcount == 0)
            loli_value_destroy(value);
    }
}

void loli_value_assign(loli_value *left, loli_value *right)
{
    if (right->flags & VAL_IS_DEREFABLE)
        right->value.generic->refcount++;

    if (left->flags & VAL_IS_DEREFABLE)
        loli_deref(left);

    left->value = right->value;
    left->flags = right->flags;
}

loli_value *loli_value_copy(loli_value *input)
{
    if (input->flags & VAL_IS_DEREFABLE)
        input->value.generic->refcount++;

    loli_value *result = loli_malloc(sizeof(*result));
    result->flags = input->flags;
    result->value = input->value;

    return result;
}

static int loli_value_compare_raw(loli_vm_state *, int *, loli_value *,
        loli_value *);

static int subvalue_eq(loli_state *s, int *depth, loli_value *left,
        loli_value *right)
{
    loli_container_val *left_list = left->value.container;
    loli_container_val *right_list = right->value.container;
    int ok;
    if (left_list->num_values == right_list->num_values) {
        ok = 1;
        int i;
        for (i = 0;i < left_list->num_values;i++) {
            loli_value *left_item = left_list->values[i];
            loli_value *right_item = right_list->values[i];
            (*depth)++;
            if (loli_value_compare_raw(s, depth, left_item, right_item) == 0) {
                (*depth)--;
                ok = 0;
                break;
            }
            (*depth)--;
        }
    }
    else
        ok = 0;

    return ok;
}

int loli_value_compare_raw(loli_state *s, int *depth, loli_value *left,
        loli_value *right)
{
    int left_base = FLAGS_TO_BASE(left);
    int right_base = FLAGS_TO_BASE(right);

    if (*depth == 100) loli_RuntimeError(s, "Infinite loop in comparison.");

    if (left_base != right_base) return 0;

    switch (left_base) {
        case V_INTEGER_BASE:
        case V_BOOLEAN_BASE:
            return left->value.integer == right->value.integer;

        case V_STRING_BASE:
            return strcmp(left->value.string->string, right->value.string->string) == 0;

        case V_BYTESTRING_BASE: {
            loli_string_val *left_sv = left->value.string;
            loli_string_val *right_sv = right->value.string;

            char *left_s = left_sv->string;
            char *right_s = right_sv->string;
            int left_size = left_sv->size;

            return (left_size == right_sv->size && memcmp(left_s, right_s, left_size) == 0);
        }

        case V_LIST_BASE:
        case V_TUPLE_BASE:
            return subvalue_eq(s, depth, left, right);

        case V_HASH_BASE: {
            loli_hash_val *left_hash = left->value.hash;
            loli_hash_val *right_hash = right->value.hash;

            int ok = 1;
            if (left_hash->num_entries != right_hash->num_entries)
                ok = 0;

            if (ok) {
                (*depth)++;
                for (int i = 0; i < left_hash->num_bins; i++) {
                    loli_hash_entry *left = left_hash->bins[i];
                    if (left) {
                        loli_value *right = loli_hash_get(s, right_hash,
                                                          left->boxed_key);

                        if (right == NULL ||
                            loli_value_compare_raw(s, depth, left->record, right) == 0) {
                            ok = 0;
                            break;
                        }
                    }
                }
                (*depth)--;
            }
            return ok;
        } case V_VARIANT_BASE: {
            int ok = 0;
            if (left->value.container->class_id == right->value.container->class_id)
                ok = subvalue_eq(s, depth, left, right);

            return ok;
        } case V_EMPTY_VARIANT_BASE:
            return left->value.integer == right->value.integer;

        default:
            return left->value.generic == right->value.generic;
    }
}

int loli_value_compare(loli_state *s, loli_value *left, loli_value *right)
{
    int depth = 0;
    return loli_value_compare_raw(s, &depth, left, right);
}

uint16_t loli_cid_at(loli_vm_state *vm, int n)
{
    return vm->call_chain->function->cid_table[n];
}
