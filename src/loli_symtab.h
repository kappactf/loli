#ifndef LOLI_SYMTAB_H
# define LOLI_SYMTAB_H

# include "loli_core_types.h"
# include "loli_generic_pool.h"
# include "loli_value_structs.h"
# include "loli_value_stack.h"

typedef struct loli_symtab_ {
    loli_value_stack *literals;

    loli_module_entry *builtin_module;
    loli_module_entry *active_module;

     
    loli_var *old_function_chain;

     
    loli_class *old_class_chain;

    loli_class *hidden_class_chain;

     
    loli_generic_pool *generics;

     
    uint16_t next_class_id;

    uint16_t next_global_id;

     
    uint16_t next_reverse_id;

    uint16_t pad;

     
    loli_class *integer_class;
    loli_class *double_class;
    loli_class *string_class;
    loli_class *byte_class;
    loli_class *bytestring_class;
    loli_class *boolean_class;
    loli_class *function_class;
    loli_class *list_class;
    loli_class *hash_class;
    loli_class *tuple_class;
    loli_class *optarg_class;
} loli_symtab;

loli_symtab *loli_new_symtab(loli_generic_pool *);
void loli_set_builtin(loli_symtab *, loli_module_entry *);
void loli_free_module_symbols(loli_symtab *, loli_module_entry *);
void loli_hide_module_symbols(loli_symtab *, loli_module_entry *);
void loli_rewind_symtab(loli_symtab *, loli_module_entry *, loli_class *,
        loli_var *, loli_boxed_sym *, int);
void loli_free_symtab(loli_symtab *);

loli_literal *loli_get_integer_literal(loli_symtab *, int64_t);
loli_literal *loli_get_double_literal(loli_symtab *, double);
loli_literal *loli_get_bytestring_literal(loli_symtab *, const char *, int);
loli_literal *loli_get_string_literal(loli_symtab *, const char *);
loli_literal *loli_get_unit_literal(loli_symtab *);

loli_class *loli_find_class(loli_symtab *, loli_module_entry *, const char *);
loli_var *loli_find_method(loli_class *, const char *);
loli_prop_entry *loli_find_property(loli_class *, const char *);
loli_variant_class *loli_find_variant(loli_class *, const char *);
loli_class *loli_find_class_of_member(loli_class *, const char *);
loli_named_sym *loli_find_member(loli_class *, const char *, loli_class *);
loli_var *loli_find_var(loli_symtab *, loli_module_entry *, const char *);

loli_class *loli_new_raw_class(const char *);
loli_class *loli_new_class(loli_symtab *, const char *);
loli_class *loli_new_enum_class(loli_symtab *, const char *);
loli_variant_class *loli_new_variant_class(loli_symtab *, loli_class *,
        const char *);

loli_prop_entry *loli_add_class_property(loli_symtab *, loli_class *,
        loli_type *, const char *, int);
void loli_add_symbol_ref(loli_module_entry *, loli_sym *);

void loli_fix_enum_variant_ids(loli_symtab *, loli_class *);
void loli_register_classes(loli_symtab *, struct loli_vm_state_ *);

loli_module_entry *loli_find_module(loli_symtab *, loli_module_entry *,
        const char *);
loli_module_entry *loli_find_module_by_path(loli_symtab *, const char *);
loli_module_entry *loli_find_registered_module(loli_symtab *, const char *);
#endif
