#ifndef LOLI_EXPR_H
# define LOLI_EXPR_H

# include <stdint.h>

# include "loli_core_types.h"

typedef enum {
    expr_plus,
    expr_plus_plus,
    expr_minus,
    expr_eq_eq,
    expr_lt,
    expr_lt_eq,
    expr_gr,
    expr_gr_eq,
    expr_not_eq,
    expr_modulo,
    expr_multiply,
    expr_divide,
    expr_left_shift,
    expr_right_shift,
    expr_bitwise_and,
    expr_bitwise_or,
    expr_bitwise_xor,
    expr_unary_not,
    expr_unary_minus,
    expr_unary_bitwise_not,
    expr_logical_and,
    expr_logical_or,
    expr_func_pipe,
    expr_assign,
    expr_plus_assign,
    expr_minus_assign,
    expr_modulo_assign,
    expr_mul_assign,
    expr_div_assign,
    expr_left_shift_assign,
    expr_right_shift_assign,
    expr_bitwise_and_assign,
    expr_bitwise_or_assign,
    expr_bitwise_xor_assign,
    expr_named_arg,
} loli_expr_op;

typedef enum {
    tree_call, tree_subscript, tree_list, tree_hash, tree_parenth,
    tree_local_var, tree_defined_func, tree_global_var, tree_oo_access,
    tree_unary, tree_type, tree_typecast, tree_tuple, tree_property,
    tree_variant, tree_lambda, tree_literal, tree_inherited_new, tree_method,
    tree_static_func, tree_self, tree_upvalue, tree_boolean, tree_byte,
    tree_integer, tree_oo_cached, tree_named_call, tree_named_arg, tree_binary
} loli_tree_type;

typedef struct loli_ast_ {
    loli_sym *result;

    loli_tree_type tree_type: 8;

    union {
        loli_expr_op op: 8;
        loli_tree_type first_tree_type: 8;
    };

    union {
        uint8_t priority;
        uint16_t call_op;
    };

    uint32_t line_num;

    union {
        uint16_t call_source_reg;
         
        uint16_t keyword_arg_pos;
    };

    uint16_t args_collected;

    union {
        uint32_t pile_pos;
         
        int16_t backing_value;
         
        uint16_t literal_reg_spot;
        uint16_t keep_first_call_arg;
    };

    union {
        loli_item *item;
        loli_sym *sym;
        loli_prop_entry *property;
        loli_variant_class *variant;
        struct loli_ast_ *left;
        loli_type *type;
    };

    union {
         
        struct loli_ast_ *arg_start;
         
        struct loli_ast_ *right;
    };

     
    struct loli_ast_ *parent;

     
    struct loli_ast_ *next_arg;

     
    struct loli_ast_ *next_tree;
} loli_ast;

typedef struct loli_ast_save_entry_ {
     
    loli_ast *active_tree;

     
    loli_ast *root_tree;

     
    loli_ast *entered_tree;

    struct loli_ast_save_entry_ *next;
    struct loli_ast_save_entry_ *prev;
} loli_ast_save_entry;

typedef struct loli_ast_checkpoint_entry_ {
    loli_ast *first_tree;
    uint16_t pile_start;
    uint16_t pad;
    uint32_t pad2;
    loli_ast *root;
    loli_ast *active;
} loli_ast_checkpoint_entry;

typedef struct loli_expr_state_ {
     
    loli_ast *root;

     
    loli_ast *active;

     
    loli_ast *next_available;

     
    loli_ast *first_tree;

     
    loli_ast_save_entry *save_chain;

     
    uint16_t save_depth;

     
    uint16_t pile_start;

     
    uint16_t pile_current;

    uint16_t pad;

    loli_ast_checkpoint_entry **checkpoints;
    uint32_t checkpoint_pos;
    uint32_t checkpoint_size;

    uint32_t *lex_linenum;
} loli_expr_state;

void loli_es_flush(loli_expr_state *);
void loli_es_checkpoint_save(loli_expr_state *);
void loli_es_checkpoint_restore(loli_expr_state *);
void loli_es_checkpoint_reverse_n(loli_expr_state *, int);

void loli_es_collect_arg(loli_expr_state *);
void loli_es_enter_tree(loli_expr_state *, loli_tree_type);
void loli_free_expr_state(loli_expr_state *);
loli_expr_state *loli_new_expr_state(void);
void loli_es_leave_tree(loli_expr_state *);
loli_ast *loli_es_get_saved_tree(loli_expr_state *);
void loli_es_enter_typecast(loli_expr_state *ap, loli_type *type);
void loli_es_push_local_var(loli_expr_state *, loli_var *);
void loli_es_push_binary_op(loli_expr_state *, loli_expr_op);
void loli_es_push_global_var(loli_expr_state *, loli_var *);
void loli_es_push_defined_func(loli_expr_state *, loli_var *);
void loli_es_push_method(loli_expr_state *, loli_var *);
void loli_es_push_static_func(loli_expr_state *, loli_var *);
void loli_es_push_literal(loli_expr_state *, loli_type *, uint16_t);
void loli_es_push_unary_op(loli_expr_state *, loli_expr_op);
void loli_es_push_property(loli_expr_state *, loli_prop_entry *);
void loli_es_push_variant(loli_expr_state *, loli_variant_class *);
void loli_es_push_text(loli_expr_state *, loli_tree_type, uint32_t, int);
void loli_es_push_inherited_new(loli_expr_state *, loli_var *);
void loli_es_push_self(loli_expr_state *);
void loli_es_push_upvalue(loli_expr_state *, loli_var *);
void loli_es_push_integer(loli_expr_state *, int16_t);
void loli_es_push_boolean(loli_expr_state *, int16_t);
void loli_es_push_byte(loli_expr_state *, uint8_t);

#endif
