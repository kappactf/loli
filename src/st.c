
#include <stdio.h>
#include <string.h>

#include "loli.h"

#include "loli_core_types.h"
#include "loli_value_structs.h"
#include "loli_alloc.h"
#include "loli_value_flags.h"
#include "loli_value_raw.h"

extern uint64_t siphash24(const void *, unsigned long, const char [16]);

static long primes[] =
{
    8 + 3,
    16 + 3,
    32 + 5,
    64 + 3,
    128 + 3,
    256 + 27,
    512 + 9,
    1024 + 9,
    2048 + 5,
    4096 + 3,
    8192 + 27,
    16384 + 43,
    32768 + 3,
    65536 + 45,
    131072 + 29,
    262144 + 3,
    524288 + 21,
    1048576 + 7,
    2097152 + 17,
    4194304 + 15,
    8388608 + 9,
    16777216 + 43,
    33554432 + 35,
    67108864 + 15,
    134217728 + 29,
    268435456 + 3,
    536870912 + 11,
    1073741824 + 85,
    0
};

#define MINSIZE 8
#define ST_DEFAULT_MAX_DENSITY 5
#define ST_DEFAULT_INIT_TABLE_SIZE 11
#define do_hash_bin(key,table) (key%(table)->num_bins)

#define EQUAL(table,x,y) ((cmp_fn)((x),(y)) == 0)
#define PTR_NOT_EQUAL(table, ptr, hash_val, key) \
((ptr) != 0 && (ptr->hash != (hash_val) || !EQUAL((table), (key), (ptr)->raw_key)))

#define ADD_DIRECT(table, key_box, key_raw, value, hash_val, bin_pos)\
{\
    loli_hash_entry *entry;\
    if (table->num_entries/(table->num_bins) > ST_DEFAULT_MAX_DENSITY) {\
        rehash(table);\
        bin_pos = hash_val % table->num_bins;\
    }\
    \
    entry = loli_malloc(sizeof(*entry));\
    \
    entry->boxed_key = loli_value_copy(key_box); \
    entry->raw_key = key_raw; \
    entry->hash = hash_val;\
    entry->record = loli_value_copy(value);\
    entry->next = table->bins[bin_pos];\
    table->bins[bin_pos] = entry;\
    table->num_entries++;\
}

#define FIND_ENTRY(table, ptr, hash_val, bin_pos) \
bin_pos = hash_val%(table)->num_bins;\
ptr = (table)->bins[bin_pos];\
if (PTR_NOT_EQUAL(table, ptr, hash_val, key)) {\
    while (PTR_NOT_EQUAL(table, ptr->next, hash_val, key)) {\
        ptr = ptr->next;\
    }\
    ptr = ptr->next;\
}

#define SET_HASH_OUT_AND_CMP(s, table, boxed_key) \
    int (*cmp_fn)(loli_raw_value, loli_raw_value); \
    if ((boxed_key->flags & V_STRING_FLAG) == 0) { \
        hash_out = (uint64_t)boxed_key->value.integer; \
        cmp_fn = cmp_int; \
    } \
    else { \
        loli_string_val *sv = boxed_key->value.string; \
        hash_out = siphash24(sv->string, sv->size, \
                loli_config_get(s)->sipkey); \
        cmp_fn = cmp_str; \
    } \

static int new_size(int size)
{
    int i, newsize;
     
    int out = -1;

    for (i = 0, newsize = MINSIZE;
         i < sizeof(primes)/sizeof(primes[0]);
         i++, newsize <<= 1)
    {
        if (newsize > size) {
            out = primes[i];
            break;
        }
    }

    return out;
}

loli_hash_val *loli_new_hash_raw(int size)
{
    loli_hash_val *tbl;

    size = new_size(size);  

    tbl = loli_malloc(sizeof(*tbl));
    tbl->refcount = 1;
    tbl->iter_count = 0;
    tbl->num_entries = 0;

    tbl->num_bins = size;
    tbl->bins = loli_malloc(size * sizeof(*tbl->bins));
    memset(tbl->bins, 0, size * sizeof(*tbl->bins));

    return tbl;
}

static int cmp_int(loli_raw_value raw_left, loli_raw_value raw_right)
{
    return raw_left.integer != raw_right.integer;
}

static int cmp_str(loli_raw_value raw_left, loli_raw_value raw_right)
{
    loli_string_val *left_sv = raw_left.string;
    loli_string_val *right_sv = raw_right.string;

    return left_sv->size != right_sv->size &&
           strcmp(left_sv->string, right_sv->string) != 0;
}

static void rehash(loli_hash_val *table)
{
    loli_hash_entry *ptr, *next, **new_bins;
    int i, old_num_bins = table->num_bins, new_num_bins;
    unsigned int hash_val;

    new_num_bins = new_size(old_num_bins+1);
    new_bins = loli_malloc(new_num_bins * sizeof(*new_bins));
    memset(new_bins, 0, new_num_bins * sizeof(*new_bins));

    for(i = 0; i < old_num_bins; i++) {
        ptr = table->bins[i];
        while (ptr != 0) {
            next = ptr->next;
            hash_val = ptr->hash % new_num_bins;
            ptr->next = new_bins[hash_val];
            new_bins[hash_val] = ptr;
            ptr = next;
        }
    }
    loli_free(table->bins);
    table->num_bins = new_num_bins;
    table->bins = new_bins;
}

int loli_hash_take(loli_state *s, loli_hash_val *table, loli_value *boxed_key)
{
    unsigned int hash_val;
    loli_hash_entry *tmp, *ptr;
    uint64_t hash_out;
    loli_raw_value key = boxed_key->value;

    SET_HASH_OUT_AND_CMP(s, table, boxed_key);
    hash_val = do_hash_bin(hash_out, table);
    ptr = table->bins[hash_val];

    if (ptr == 0)
        return 0;

    if (EQUAL(table, key, ptr->raw_key)) {
        table->bins[hash_val] = ptr->next;
        table->num_entries--;

        loli_stack_push_and_destroy(s, ptr->boxed_key);
        loli_stack_push_and_destroy(s, ptr->record);
        loli_free(ptr);
        return 1;
    }

    for(; ptr->next != 0; ptr = ptr->next) {
        if (EQUAL(table, ptr->next->raw_key, key)) {
            tmp = ptr->next;
            ptr->next = ptr->next->next;
            table->num_entries--;

            loli_stack_push_and_destroy(s, tmp->boxed_key);
            loli_stack_push_and_destroy(s, tmp->record);
            loli_free(tmp);
            return 1;
        }
    }

    return 0;
}

void loli_hash_set(loli_state *s, register loli_hash_val *table,
        loli_value *boxed_key, loli_value *record)
{
    unsigned int bin_pos;
    register loli_hash_entry *ptr;
    uint64_t hash_out;
    loli_raw_value key = boxed_key->value;

    SET_HASH_OUT_AND_CMP(s, table, boxed_key);
    FIND_ENTRY(table, ptr, hash_out, bin_pos);

    if (ptr == 0) {
        ADD_DIRECT(table, boxed_key, key, record, hash_out, bin_pos);
    }
    else {
        loli_value_assign(ptr->record, record);
        loli_value_assign(ptr->boxed_key, boxed_key);
         
        ptr->raw_key = key;
    }
}

void loli_hash_set_from_stack(loli_state *s, loli_hash_val *table)
{
    loli_value *record = loli_stack_take(s);
    loli_value *key = loli_stack_take(s);

    loli_hash_set(s, table, key, record);

    loli_deref(record);
    record->flags = 0;

    loli_deref(key);
    key->flags = 0;
}

loli_value *loli_hash_get(loli_state *s, loli_hash_val *table,
        loli_value *boxed_key)
{
    unsigned int bin_pos;
    register loli_hash_entry *ptr;
    uint64_t hash_out;
    loli_raw_value key = boxed_key->value;

    SET_HASH_OUT_AND_CMP(s, table, boxed_key);
    FIND_ENTRY(table, ptr, hash_out, bin_pos);

    if (ptr)
        return ptr->record;
    else
        return NULL;
}
