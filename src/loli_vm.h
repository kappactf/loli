#ifndef LOLI_VM_H
# define LOLI_VM_H

# include "loli.h"

# include "loli_raiser.h"
# include "loli_symtab.h"

typedef struct loli_call_frame_ {
     
    loli_value **start;
     
    loli_value **top;
     
    loli_value **register_end;
     
    uint16_t *code;
    loli_function_val *function;
     
    loli_value *return_target;

    struct loli_call_frame_ *prev;
    struct loli_call_frame_ *next;
} loli_call_frame;

typedef enum {
    catch_native,
    catch_callback
} loli_catch_kind;

typedef struct loli_vm_catch_entry_ {
    loli_call_frame *call_frame;
    int code_pos;
    uint32_t call_frame_depth;
    loli_catch_kind catch_kind : 32;

    union {
        loli_jump_link *jump_entry;
        loli_error_callback_func callback_func;
    };

    struct loli_vm_catch_entry_ *next;
    struct loli_vm_catch_entry_ *prev;
} loli_vm_catch_entry;

typedef struct loli_global_state_ {
    loli_value **regs_from_main;

    loli_value **readonly_table;
    loli_class **class_table;
    uint32_t class_count;
    uint32_t readonly_count;

     
    loli_gc_entry *gc_live_entries;

     
    loli_gc_entry *gc_spare_entries;

     
    uint32_t gc_live_entry_count;
     
    uint32_t gc_threshold;
     
    uint32_t gc_pass;

     
    uint32_t gc_multiplier;

    struct loli_vm_state_ *first_vm;

     
    struct loli_parse_state_ *parser;
} loli_global_state;

typedef struct loli_vm_state_ {
    loli_value **register_root;

    uint32_t call_depth;

    uint32_t depth_max;

    loli_call_frame *call_chain;

    loli_global_state *gs;

    loli_vm_catch_entry *catch_chain;

     
    loli_value *exception_value;

    loli_class *exception_cls;

     
    loli_msgbuf *vm_buffer;

    loli_raiser *raiser;

     
    void *data;
} loli_vm_state;

loli_vm_state *loli_new_vm_state(loli_raiser *);
void loli_free_vm(loli_vm_state *);
void loli_vm_execute(loli_vm_state *);

void loli_vm_ensure_class_table(loli_vm_state *, int);
void loli_vm_add_class_unchecked(loli_vm_state *, loli_class *);

#endif
