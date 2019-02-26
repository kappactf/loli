#include <ctype.h>
#include <string.h>

#include "loli.h"

#include "loli_symtab.h"
#include "loli_vm.h"
#include "loli_value_flags.h"
#include "loli_alloc.h"
#include "loli_value_raw.h"

#define MAX_STRING_CACHE_LENGTH 32


loli_symtab *loli_new_symtab(loli_generic_pool *gp)
{
    loli_symtab *symtab = loli_malloc(sizeof(*symtab));

    symtab->next_class_id = 1;
    symtab->old_function_chain = NULL;
    symtab->old_class_chain = NULL;
    symtab->hidden_class_chain = NULL;
    symtab->literals = loli_new_value_stack();
    symtab->generics = gp;
    symtab->next_global_id = 0;
    symtab->next_reverse_id = LOLI_LAST_ID;

    return symtab;
}

void loli_set_builtin(loli_symtab *symtab, loli_module_entry *builtin)
{
    symtab->builtin_module = builtin;
    symtab->active_module = builtin;
}

static void free_boxed_syms_since(loli_boxed_sym *sym, loli_boxed_sym *stop)
{
    loli_boxed_sym *sym_next;

    while (sym != stop) {
        sym_next = sym->next;

        loli_free(sym);

        sym = sym_next;
    }
}

#define free_boxed_syms(s) free_boxed_syms_since(s, NULL)

static void free_vars_since(loli_var *var, loli_var *stop)
{
    loli_var *var_next;

    while (var != stop) {
        var_next = var->next;

        loli_free(var->name);
        loli_free(var);

        var = var_next;
    }
}

#define free_vars(v) free_vars_since(v, NULL)

static void free_properties(loli_class *cls)
{
    loli_named_sym *prop_iter = cls->members;
    loli_named_sym *next_prop;

    while (prop_iter) {
        next_prop = prop_iter->next;

        if (prop_iter->item_kind == ITEM_TYPE_VARIANT)
            loli_free(((loli_variant_class *)prop_iter)->arg_names);

        loli_free(prop_iter->name);
        loli_free(prop_iter);

        prop_iter = next_prop;
    }
}

static void free_classes_until(loli_class *class_iter, loli_class *stop)
{
    while (class_iter != stop) {
        loli_free(class_iter->name);

        if (class_iter->members != NULL)
            free_properties(class_iter);

        loli_type *type_iter = class_iter->all_subtypes;
        loli_type *type_next;
        while (type_iter) {
            type_next = type_iter->next;
            loli_free(type_iter->subtypes);
            loli_free(type_iter);
            type_iter = type_next;
        }

        loli_class *class_next = class_iter->next;
        loli_free(class_iter);
        class_iter = class_next;
    }
}

#define free_classes(iter) free_classes_until(iter, NULL)

static void hide_classes(loli_symtab *symtab, loli_class *class_iter,
        loli_class *stop)
{
    loli_class *hidden_top = symtab->hidden_class_chain;

    while (class_iter != stop) {
        loli_class *class_next = class_iter->next;

        class_iter->next = hidden_top;
        hidden_top = class_iter;

        class_iter = class_next;
    }

    symtab->hidden_class_chain = hidden_top;
}

static void free_literals(loli_value_stack *literals)
{
    while (loli_vs_pos(literals)) {
        loli_literal *lit = (loli_literal *)loli_vs_pop(literals);

         
        if (lit->flags &
            (V_BYTESTRING_FLAG | V_STRING_FLAG | V_FUNCTION_FLAG)) {
            lit->flags |= VAL_IS_DEREFABLE;
            loli_deref((loli_value *)lit);
        }

        loli_free(lit);
    }

    loli_free_value_stack(literals);
}

void loli_hide_module_symbols(loli_symtab *symtab, loli_module_entry *entry)
{
    hide_classes(symtab, entry->class_chain, NULL);
    free_vars(entry->var_chain);
    if (entry->boxed_chain)
        free_boxed_syms(entry->boxed_chain);
}

void loli_free_module_symbols(loli_symtab *symtab, loli_module_entry *entry)
{
    (void) symtab;
    free_classes(entry->class_chain);
    free_vars(entry->var_chain);
    if (entry->boxed_chain)
        free_boxed_syms(entry->boxed_chain);
}

void loli_rewind_symtab(loli_symtab *symtab, loli_module_entry *main_module,
        loli_class *stop_class, loli_var *stop_var, loli_boxed_sym *stop_box,
        int hide)
{
    symtab->active_module = main_module;
    symtab->next_reverse_id = LOLI_LAST_ID;

    if (main_module->boxed_chain != stop_box) {
        free_boxed_syms_since(main_module->boxed_chain, stop_box);
        main_module->boxed_chain = stop_box;
    }

    if (main_module->var_chain != stop_var) {
        free_vars_since(main_module->var_chain, stop_var);
        main_module->var_chain = stop_var;
    }

    if (main_module->class_chain != stop_class) {
        if (hide)
            free_classes_until(main_module->class_chain, stop_class);
        else
            hide_classes(symtab, main_module->class_chain, stop_class);

        main_module->class_chain = stop_class;
    }
}

void loli_free_symtab(loli_symtab *symtab)
{
    free_literals(symtab->literals);

    free_classes(symtab->old_class_chain);
    free_classes(symtab->hidden_class_chain);
    free_vars(symtab->old_function_chain);

    loli_free(symtab);
}



static loli_value *new_value_of_bytestring(loli_bytestring_val *bv)
{
    loli_value *v = loli_malloc(sizeof(*v));

    v->flags = V_BYTESTRING_FLAG | V_BYTESTRING_BASE | VAL_IS_DEREFABLE;
    v->value.string = (loli_string_val *)bv;
    return v;
}

static loli_value *new_value_of_double(double d)
{
    loli_value *v = loli_malloc(sizeof(*v));

    v->flags = V_DOUBLE_FLAG | V_DOUBLE_BASE;
    v->value.doubleval = d;
    return v;
}

static loli_value *new_value_of_integer(int64_t i)
{
    loli_value *v = loli_malloc(sizeof(*v));

    v->flags = V_INTEGER_FLAG | V_INTEGER_BASE;
    v->value.integer = i;
    return v;
}

static loli_value *new_value_of_string(loli_string_val *sv)
{
    loli_value *v = loli_malloc(sizeof(*v));

    v->flags = V_STRING_FLAG | V_STRING_BASE | VAL_IS_DEREFABLE;
    v->value.string = sv;
    return v;
}

static loli_value *new_value_of_unit(void)
{
    loli_value *v = loli_malloc(sizeof(*v));

    v->flags = V_UNIT_BASE;
    v->value.integer = 0;
    return v;
}

static loli_literal *first_lit_of(loli_value_stack *vs, int to_find)
{
    int stop = loli_vs_pos(vs);
    int i;

    for (i = 0;i < stop;i++) {
        loli_literal *lit = (loli_literal *)loli_vs_nth(vs, i);
        if (FLAGS_TO_BASE(lit) == to_find)
            return lit;
    }

    return NULL;
}

loli_literal *loli_get_integer_literal(loli_symtab *symtab, int64_t int_val)
{
    loli_literal *iter = first_lit_of(symtab->literals, V_INTEGER_BASE);

    while (iter) {
        if (iter->value.integer == int_val)
            return iter;

        int next = iter->next_index;

        if (next == 0)
            break;
        else
            iter = (loli_literal *)loli_vs_nth(symtab->literals, next);
    }

    if (iter)
        iter->next_index = loli_vs_pos(symtab->literals);

    loli_literal *v = (loli_literal *)new_value_of_integer(int_val);
    v->reg_spot = loli_vs_pos(symtab->literals);
    v->next_index = 0;

    loli_vs_push(symtab->literals, (loli_value *)v);
    return (loli_literal *)v;
}

loli_literal *loli_get_double_literal(loli_symtab *symtab, double dbl_val)
{
    loli_literal *iter = first_lit_of(symtab->literals, V_DOUBLE_BASE);

    while (iter) {
        if (iter->value.doubleval == dbl_val)
            return iter;

        int next = iter->next_index;

        if (next == 0)
            break;
        else
            iter = (loli_literal *)loli_vs_nth(symtab->literals, next);
    }

    if (iter)
        iter->next_index = loli_vs_pos(symtab->literals);

    loli_literal *v = (loli_literal *)new_value_of_double(dbl_val);
    v->reg_spot = loli_vs_pos(symtab->literals);
    v->next_index = 0;

    loli_vs_push(symtab->literals, (loli_value *)v);
    return (loli_literal *)v;
}

loli_literal *loli_get_bytestring_literal(loli_symtab *symtab,
        const char *want_string, int len)
{
    loli_literal *iter = first_lit_of(symtab->literals, V_BYTESTRING_BASE);

    if (len < MAX_STRING_CACHE_LENGTH) {
        while (iter) {
            if (iter->value.string->size == len &&
                memcmp(iter->value.string->string, want_string, len) == 0)
                return iter;

            int next = iter->next_index;

            if (next == 0)
                break;
            else
                iter = (loli_literal *)loli_vs_nth(symtab->literals, next);
        }
    }
    else {
        while (iter) {
            int next = iter->next_index;

            if (next == 0)
                break;
            else
                iter = (loli_literal *)loli_vs_nth(symtab->literals, next);
        }
    }

    if (iter)
        iter->next_index = loli_vs_pos(symtab->literals);

    loli_bytestring_val *sv = loli_new_bytestring_raw(want_string, len);
    loli_literal *v = (loli_literal *)new_value_of_bytestring(sv);

     
    v->flags = V_BYTESTRING_FLAG | V_BYTESTRING_BASE;
    v->reg_spot = loli_vs_pos(symtab->literals);
    v->next_index = 0;

    loli_vs_push(symtab->literals, (loli_value *)v);
    return (loli_literal *)v;
}

loli_literal *loli_get_string_literal(loli_symtab *symtab,
        const char *want_string)
{
    loli_literal *iter = first_lit_of(symtab->literals, V_STRING_BASE);
    size_t want_string_len = strlen(want_string);

    if (want_string_len < MAX_STRING_CACHE_LENGTH) {
        while (iter) {
            if (iter->value.string->size == want_string_len &&
                strcmp(iter->value.string->string, want_string) == 0)
                return iter;

            int next = iter->next_index;

            if (next == 0)
                break;
            else
                iter = (loli_literal *)loli_vs_nth(symtab->literals, next);
        }
    }
    else {
        while (iter) {
            int next = iter->next_index;

            if (next == 0)
                break;
            else
                iter = (loli_literal *)loli_vs_nth(symtab->literals, next);
        }
    }

    if (iter)
        iter->next_index = loli_vs_pos(symtab->literals);

    loli_string_val *sv = loli_new_string_raw(want_string);
    loli_literal *v = (loli_literal *)new_value_of_string(sv);

     
    v->flags = V_STRING_FLAG | V_STRING_BASE;
    v->reg_spot = loli_vs_pos(symtab->literals);
    v->next_index = 0;

    loli_vs_push(symtab->literals, (loli_value *)v);
    return (loli_literal *)v;
}

loli_literal *loli_get_unit_literal(loli_symtab *symtab)
{
    loli_literal *lit = first_lit_of(symtab->literals, V_UNIT_BASE);

    if (lit == NULL) {
        loli_literal *v = (loli_literal *)new_value_of_unit();
        v->reg_spot = loli_vs_pos(symtab->literals);
        v->next_index = 0;

        lit = v;
        loli_vs_push(symtab->literals, (loli_value *)v);
    }

    return lit;
}



static uint64_t shorthash_for_name(const char *name)
{
    const char *ch = &name[0];
    int i, shift;
    uint64_t ret;
    for (i = 0, shift = 0, ret = 0;
         *ch != '\0' && i != 8;
         ch++, i++, shift += 8) {
        ret |= ((uint64_t)*ch) << shift;
    }
    return ret;
}

static loli_sym *find_boxed_sym(loli_module_entry *m, const char *name,
        uint64_t shorthash)
{
    loli_boxed_sym *boxed_iter = m->boxed_chain;
    loli_sym *result = NULL;

    while (boxed_iter) {
        loli_named_sym *sym = boxed_iter->inner_sym;

        if (sym->name_shorthash == shorthash &&
            strcmp(sym->name, name) == 0) {
            result = (loli_sym *)sym;
            break;
        }

        boxed_iter = boxed_iter->next;
    }

    return result;
}

static loli_var *find_boxed_var(loli_module_entry *m, const char *name,
        uint64_t shorthash)
{
    loli_sym *sym = find_boxed_sym(m, name, shorthash);

    if (sym && sym->item_kind != ITEM_TYPE_VAR)
        sym = NULL;

    return (loli_var *)sym;
}

static loli_var *find_var(loli_module_entry *m, const char *name,
        uint64_t shorthash)
{
    loli_var *var_iter = m->var_chain;

    while (var_iter != NULL) {
        if (var_iter->shorthash == shorthash &&
            strcmp(var_iter->name, name) == 0) {

            break;
        }
        var_iter = var_iter->next;
    }

    return var_iter;
}

loli_var *loli_find_var(loli_symtab *symtab, loli_module_entry *module,
        const char *name)
{
    uint64_t shorthash = shorthash_for_name(name);
    loli_var *result;

    if (module == NULL) {
        result = find_var(symtab->builtin_module, name,
                    shorthash);
        if (result == NULL) {
            result = find_var(symtab->active_module, name, shorthash);

            if (result == NULL && symtab->active_module->boxed_chain)
                result = find_boxed_var(symtab->active_module, name, shorthash);
        }
    }
    else {
        result = find_var(module, name, shorthash);
        if (result == NULL && module->boxed_chain)
            result = find_boxed_var(module, name, shorthash);
    }

    return result;
}


loli_class *loli_new_raw_class(const char *name)
{
    loli_class *new_class = loli_malloc(sizeof(*new_class));
    char *name_copy = loli_malloc((strlen(name) + 1) * sizeof(*name_copy));

    strcpy(name_copy, name);

    new_class->item_kind = 0;
    new_class->flags = 0;

     
    new_class->self_type = (loli_type *)new_class;
    new_class->type_subtype_count = 0;

    new_class->parent = NULL;
    new_class->shorthash = shorthash_for_name(name);
    new_class->name = name_copy;
    new_class->generic_count = 0;
    new_class->prop_count = 0;
    new_class->members = NULL;
    new_class->module = NULL;
    new_class->all_subtypes = NULL;
    new_class->dyna_start = 0;
    new_class->inherit_depth = 0;

    return new_class;
}

loli_class *loli_new_class(loli_symtab *symtab, const char *name)
{
    loli_class *new_class = loli_new_raw_class(name);

     
    new_class->module = symtab->active_module;

    new_class->id = symtab->next_class_id;
    symtab->next_class_id++;

    new_class->next = symtab->active_module->class_chain;
    symtab->active_module->class_chain = new_class;

    return new_class;
}

loli_class *loli_new_enum_class(loli_symtab *symtab, const char *name)
{
    loli_class *new_class = loli_new_class(symtab, name);

    symtab->next_class_id--;
    new_class->flags |= CLS_IS_ENUM;
    new_class->id = symtab->next_reverse_id;
    symtab->next_reverse_id--;

    return new_class;
}

static loli_class *find_boxed_class(loli_module_entry *m, const char *name,
        uint64_t shorthash)
{
    loli_sym *sym = find_boxed_sym(m, name, shorthash);

    if (sym && sym->item_kind == ITEM_TYPE_VAR)
        sym = NULL;

    return (loli_class *)sym;
}

static loli_class *find_class(loli_module_entry *m, const char *name,
        uint64_t shorthash)
{
    loli_class *class_iter = m->class_chain;

    while (class_iter) {
        if (class_iter->shorthash == shorthash &&
            strcmp(class_iter->name, name) == 0)
            break;

        if (class_iter->flags & CLS_IS_ENUM &&
            (class_iter->flags & CLS_ENUM_IS_SCOPED) == 0) {
            loli_named_sym *sym_iter = class_iter->members;
            while (sym_iter) {
                if (sym_iter->name_shorthash == shorthash &&
                    strcmp(sym_iter->name, name) == 0) {
                    return (loli_class *)sym_iter;
                }

                sym_iter = sym_iter->next;
            }
        }

        class_iter = class_iter->next;
    }

    return class_iter;
}

loli_class *loli_find_class(loli_symtab *symtab, loli_module_entry *module,
        const char *name)
{
    uint64_t shorthash = shorthash_for_name(name);
    loli_class *result;

    if (module == NULL) {
        if (name[1] != '\0') {
            result = find_class(symtab->builtin_module, name,
                    shorthash);
            if (result == NULL) {
                result = find_class(symtab->active_module, name,
                        shorthash);
                if (result == NULL && symtab->active_module->boxed_chain)
                    result = find_boxed_class(symtab->active_module, name,
                            shorthash);
            }
        }
        else
            result = loli_gp_find(symtab->generics, name);
    }
    else {
        result = find_class(module, name, shorthash);
        if (result == NULL && module->boxed_chain)
            result = find_boxed_class(module, name, shorthash);
    }

    return result;
}

loli_named_sym *loli_find_member(loli_class *cls, const char *name,
        loli_class *scope)
{
    loli_class *start_cls = cls;
    loli_named_sym *ret = NULL;

    while (1) {
        if (cls->members != NULL) {
            uint64_t shorthash = shorthash_for_name(name);
            loli_named_sym *sym_iter = cls->members;
            while (sym_iter) {
                if (sym_iter->name_shorthash == shorthash &&
                    strcmp(sym_iter->name, name) == 0) {

                    if ((sym_iter->flags & SYM_SCOPE_PRIVATE) == 0 ||
                        cls == start_cls)
                        ret = (loli_named_sym *)sym_iter;

                    break;
                }

                sym_iter = sym_iter->next;
            }
        }

        if (ret || scope == cls || cls->parent == NULL)
            break;

        cls = cls->parent;
    }

    return ret;
}

loli_class *loli_find_class_of_member(loli_class *cls, const char *name)
{
    loli_class *ret = NULL;

    while (1) {
        if (cls->members != NULL) {
            uint64_t shorthash = shorthash_for_name(name);
            loli_named_sym *sym_iter = cls->members;
            while (sym_iter) {
                if (sym_iter->name_shorthash == shorthash &&
                    strcmp(sym_iter->name, name) == 0) {
                    ret = cls;
                    break;
                }

                sym_iter = sym_iter->next;
            }
        }

        if (ret || cls->parent == NULL)
            break;

        cls = cls->parent;
    }

    return ret;
}

loli_var *loli_find_method(loli_class *cls, const char *name)
{
    loli_named_sym *sym = loli_find_member(cls, name, NULL);
    if (sym && sym->item_kind != ITEM_TYPE_VAR)
        sym = NULL;

    return (loli_var *)sym;
}

loli_prop_entry *loli_find_property(loli_class *cls, const char *name)
{
    loli_named_sym *sym = loli_find_member(cls, name, NULL);
    if (sym && sym->item_kind != ITEM_TYPE_PROPERTY)
        sym = NULL;

    return (loli_prop_entry *)sym;
}

static loli_module_entry *find_module(loli_module_entry *module,
        const char *name)
{
    loli_module_link *link_iter = module->module_chain;
    loli_module_entry *result = NULL;
    while (link_iter) {
        char *as_name = link_iter->as_name;
        char *loadname = link_iter->module->loadname;

         
        if ((as_name && strcmp(as_name, name) == 0) ||
            (as_name == NULL && strcmp(loadname, name) == 0)) {
            result = link_iter->module;
            break;
        }

        link_iter = link_iter->next_module;
    }

    return result;
}

loli_prop_entry *loli_add_class_property(loli_symtab *symtab, loli_class *cls,
        loli_type *type, const char *name, int flags)
{
    loli_prop_entry *entry = loli_malloc(sizeof(*entry));
    char *entry_name = loli_malloc((strlen(name) + 1) * sizeof(*entry_name));

    strcpy(entry_name, name);

    entry->item_kind = ITEM_TYPE_PROPERTY;
    entry->flags = flags;
    entry->name = entry_name;
    entry->type = type;
    entry->name_shorthash = shorthash_for_name(entry_name);
    entry->next = NULL;
    entry->id = cls->prop_count;
    entry->cls = cls;
    cls->prop_count++;

    entry->next = (loli_prop_entry *)cls->members;
    cls->members = (loli_named_sym *)entry;

    return entry;
}


loli_variant_class *loli_new_variant_class(loli_symtab *symtab,
        loli_class *enum_cls, const char *name)
{
    loli_variant_class *variant = loli_malloc(sizeof(*variant));

    variant->item_kind = ITEM_TYPE_VARIANT;
    variant->flags = CLS_EMPTY_VARIANT;
    variant->parent = enum_cls;
    variant->build_type = NULL;
    variant->shorthash = shorthash_for_name(name);
    variant->arg_names = NULL;
    variant->name = loli_malloc((strlen(name) + 1) * sizeof(*variant->name));
    strcpy(variant->name, name);

    variant->next = (loli_class *)enum_cls->members;
    enum_cls->members = (loli_named_sym *)variant;

    variant->cls_id = symtab->next_reverse_id;
    symtab->next_reverse_id--;

    return variant;
}

loli_variant_class *loli_find_variant(loli_class *enum_cls,
        const char *name)
{
    uint64_t shorthash = shorthash_for_name(name);
    loli_named_sym *sym_iter = enum_cls->members;

    while (sym_iter) {
        if (sym_iter->name_shorthash == shorthash &&
            strcmp(sym_iter->name, name) == 0 &&
            sym_iter->item_kind != ITEM_TYPE_VAR) {
            break;
        }

        sym_iter = sym_iter->next;
    }

    return (loli_variant_class *)sym_iter;
}

void loli_fix_enum_variant_ids(loli_symtab *symtab, loli_class *enum_cls)
{
    uint16_t next_id = symtab->next_class_id;

    enum_cls->id = next_id;
    next_id += enum_cls->variant_size;
    symtab->next_class_id = next_id + 1;
    symtab->next_reverse_id += enum_cls->variant_size + 1;

    loli_named_sym *member_iter = enum_cls->members;

     
    while (member_iter) {
        loli_variant_class *variant = (loli_variant_class *)member_iter;

        variant->cls_id = next_id;
        next_id--;

        member_iter = member_iter->next;
    }
}


void loli_register_classes(loli_symtab *symtab, loli_vm_state *vm)
{
    loli_vm_ensure_class_table(vm, symtab->next_class_id + 1);

    loli_module_entry *module_iter = symtab->builtin_module;
    while (module_iter) {
        loli_class *class_iter = module_iter->class_chain;
        while (class_iter) {
            loli_vm_add_class_unchecked(vm, class_iter);

            if (class_iter->flags & CLS_IS_ENUM) {
                loli_named_sym *sym_iter = class_iter->members;
                while (sym_iter) {
                    if (sym_iter->item_kind == ITEM_TYPE_VARIANT) {
                        loli_class *v = (loli_class *)sym_iter;
                        loli_vm_add_class_unchecked(vm, v);
                    }

                    sym_iter = sym_iter->next;
                }
            }
            class_iter = class_iter->next;
        }
        module_iter = module_iter->root_next;
    }

     
    loli_vm_add_class_unchecked(vm, symtab->integer_class);
}

loli_module_entry *loli_find_module(loli_symtab *symtab,
        loli_module_entry *module, const char *name)
{
    loli_module_entry *result;
    if (module == NULL)
        result = find_module(symtab->active_module, name);
    else
        result = find_module(module, name);

    return result;
}

loli_module_entry *loli_find_module_by_path(loli_symtab *symtab,
        const char *path)
{
     
    loli_module_entry *module_iter = symtab->builtin_module->root_next;
    size_t len = strlen(path);

    while (module_iter) {
        if (module_iter->cmp_len == len &&
            strcmp(module_iter->path, path) == 0) {
            break;
        }

        module_iter = module_iter->root_next;
    }

    return module_iter;
}

loli_module_entry *loli_find_registered_module(loli_symtab *symtab,
        const char *name)
{
     
    loli_module_entry *module_iter = symtab->builtin_module->root_next;

    while (module_iter) {
        if (module_iter->flags & MODULE_IS_REGISTERED &&
            strcmp(module_iter->loadname, name) == 0)
            break;

        module_iter = module_iter->root_next;
    }

    return module_iter;
}

void loli_add_symbol_ref(loli_module_entry *m, loli_sym *sym)
{
    loli_boxed_sym *box = loli_malloc(sizeof(*box));

     
    box->inner_sym = (loli_named_sym *)sym;
    box->next = m->boxed_chain;
    m->boxed_chain = box;
}
