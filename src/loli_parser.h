#ifndef LOLI_PARSER_H
# define LOLI_PARSER_H

# include "loli.h"

# include "loli_raiser.h"
# include "loli_expr.h"
# include "loli_lexer.h"
# include "loli_emitter.h"
# include "loli_symtab.h"
# include "loli_vm.h"
# include "loli_type_maker.h"
# include "loli_buffer_u16.h"
# include "loli_value_stack.h"
# include "loli_generic_pool.h"

struct loli_rewind_state_;
struct loli_import_state_;

typedef struct loli_parse_state_ {
    loli_module_entry *module_start;
    loli_module_entry *module_top;

    loli_module_entry *main_module;

    loli_buffer_u16 *data_stack;

    uint16_t executing;

     
    uint16_t content_to_parse;

     
    uint32_t import_pile_current;

     
    uint16_t keyarg_current;

     
    uint16_t rendering;
    uint32_t pad;

     
    loli_expr_state *expr;

     
    loli_string_pile *keyarg_strings;

     
    loli_string_pile *expr_strings;

     
    loli_string_pile *import_ref_strings;

     
    loli_generic_pool *generics;

    loli_function_val *toplevel_func;

    loli_type *class_self_type;
    loli_msgbuf *msgbuf;
    loli_type *default_call_type;
    loli_lex_state *lex;
    loli_emit_state *emit;
    loli_symtab *symtab;
    loli_vm_state *vm;
    loli_type_maker *tm;
    loli_raiser *raiser;
    loli_config *config;
    struct loli_rewind_state_ *rs;
    struct loli_import_state_ *ims;
} loli_parse_state;

loli_var *loli_parser_lambda_eval(loli_parse_state *, int, const char *,
        loli_type *);
loli_item *loli_find_or_dl_member(loli_parse_state *, loli_class *,
        const char *, loli_class *);
loli_class *loli_dynaload_exception(loli_parse_state *, const char *);

#endif
