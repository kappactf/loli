#ifndef LOLI_VALUE_STRUCTS_H
# define LOLI_VALUE_STRUCTS_H

# include <stdint.h>
# include <stdio.h>

struct loli_vm_state_;
struct loli_value_;

typedef void (*loli_foreign_func)(struct loli_vm_state_ *);

typedef union loli_raw_value_ {
    int64_t integer;
    double doubleval;
    struct loli_string_val_ *string;
     
    struct loli_generic_val_ *generic;
     
    struct loli_generic_gc_val_ *gc_generic;
    struct loli_coroutine_val_ *coroutine;
    struct loli_function_val_ *function;
    struct loli_hash_val_ *hash;
    struct loli_file_val_ *file;
    struct loli_container_val_ *container;
    struct loli_foreign_val_ *foreign;
} loli_raw_value;

typedef struct loli_literal_ {
    uint32_t flags;
    union {
        uint16_t pad;
        uint16_t next_index;
    };
    uint16_t reg_spot;
    loli_raw_value value;
} loli_literal;

typedef struct loli_gc_entry_ {
     
    uint32_t flags;
     
    int32_t last_pass;
     
    loli_raw_value value;
    struct loli_gc_entry_ *next;
} loli_gc_entry;

typedef struct loli_value_ {
    uint32_t flags;
     
    uint32_t cell_refcount;
    loli_raw_value value;
} loli_value;

typedef struct loli_string_val_ {
    uint32_t refcount;
    uint32_t size;
    char *string;
} loli_string_val;

typedef struct loli_bytestring_val_ {
    uint32_t refcount;
    uint32_t size;
    char *string;
} loli_bytestring_val;

typedef struct loli_container_val_ {
    uint32_t refcount;
    uint16_t class_id;
    uint16_t instance_ctor_need;
    uint32_t num_values;
    uint32_t extra_space;
    struct loli_value_ **values;
    struct loli_gc_entry_ *gc_entry;
} loli_container_val;

typedef struct loli_hash_entry_ {
    uint64_t hash;
    loli_raw_value raw_key;
    loli_value *boxed_key;
    loli_value *record;
    struct loli_hash_entry_ *next;
} loli_hash_entry;

typedef struct loli_hash_val_ {
    uint32_t refcount;
    uint32_t iter_count;
    int num_bins;
    int num_entries;
    loli_hash_entry **bins;
} loli_hash_val;

typedef struct loli_file_val_ {
    uint32_t refcount;
    uint8_t read_ok;
    uint8_t write_ok;
    uint8_t is_builtin;
    uint8_t pad1;
    uint32_t pad2;
    FILE *inner_file;
} loli_file_val;

typedef struct loli_function_val_ {
    uint32_t refcount;
    uint32_t pad1;

    uint16_t pad2;

    uint16_t code_len;

    uint16_t num_upvalues;

     
    uint16_t reg_count;

     
    struct loli_proto_ *proto;

    struct loli_gc_entry_ *gc_entry;

     
    loli_foreign_func foreign_func;

     
    uint16_t *code;

    struct loli_value_ **upvalues;

     
    uint16_t *cid_table;
} loli_function_val;

typedef struct loli_generic_val_ {
    uint32_t refcount;
} loli_generic_val;

typedef enum {
    co_failed,
    co_done,
    co_running,
    co_waiting
} loli_coroutine_status;

typedef struct loli_coroutine_val_ {
    uint32_t refcount;
    uint16_t class_id;
    uint16_t pad;
    uint64_t pad2;
    loli_function_val *base_function;
    struct loli_gc_entry_ *gc_entry;
    struct loli_vm_state_ *vm;
    loli_value *receiver;
    uint64_t status;
} loli_coroutine_val;

typedef struct loli_foreign_val_ {
    uint32_t refcount;
    uint16_t class_id;
    uint16_t pad;
    void (*destroy_func)(loli_generic_val *);
} loli_foreign_val;

typedef struct loli_generic_gc_val_ {
    uint32_t refcount;
    uint32_t pad;
    uint64_t pad2;
    void *pad3;
    struct loli_gc_entry_ *gc_entry;
} loli_generic_gc_val;

#endif
