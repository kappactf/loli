#include <string.h>

#include "loli_expr.h"
#include "loli_alloc.h"

#define AST_COMMON_INIT(a, tt) \
loli_ast *a; \
a = es->next_available; \
if (a->next_tree == NULL) \
    add_new_tree(es); \
\
es->next_available = a->next_tree; \
a->op = 0; \
a->tree_type = tt; \
a->next_arg = NULL; \
a->line_num = *es->lex_linenum; \
a->parent = NULL;

#define AST_ENTERABLE_INIT(a, tt) \
AST_COMMON_INIT(a, tt) \
a->args_collected = 0; \
a->arg_start = NULL; \
a->result = NULL;

static void add_save_entry(loli_expr_state *);
static void grow_checkpoints(loli_expr_state *);


loli_expr_state *loli_new_expr_state(void)
{
    loli_expr_state *es = loli_malloc(sizeof(*es));

    int i;
    loli_ast *last_tree = NULL;
    for (i = 0;i < 4;i++) {
        loli_ast *new_tree = loli_malloc(sizeof(*new_tree));

        new_tree->next_tree = last_tree;
        last_tree = new_tree;
    }

     
    es->checkpoints = NULL;
    es->checkpoint_pos = 0;
    es->checkpoint_size = 1;

    grow_checkpoints(es);

    es->first_tree = last_tree;
    es->next_available = last_tree;
    es->save_chain = NULL;
    es->save_depth = 0;
    es->pile_start = 0;
    es->pile_current = 0;
    es->root = NULL;
    es->active = NULL;

    add_save_entry(es);

    return es;
}

void loli_free_expr_state(loli_expr_state *es)
{
    loli_ast *ast_iter;
    loli_ast *ast_temp;

    if (es->checkpoint_pos)
        ast_iter = es->checkpoints[0]->first_tree;
    else
        ast_iter = es->first_tree;

    while (ast_iter) {
        ast_temp = ast_iter->next_tree;
        loli_free(ast_iter);
        ast_iter = ast_temp;
    }

    loli_ast_save_entry *save_iter = es->save_chain;
    loli_ast_save_entry *save_temp;

    while (save_iter->prev)
        save_iter = save_iter->prev;

    while (save_iter) {
        save_temp = save_iter->next;
        loli_free(save_iter);
        save_iter = save_temp;
    }

    int i;
    for (i = 0;i < es->checkpoint_size;i++)
        loli_free(es->checkpoints[i]);

    loli_free(es->checkpoints);
    loli_free(es);
}

static void add_save_entry(loli_expr_state *es)
{
    loli_ast_save_entry *new_entry = loli_malloc(sizeof(*new_entry));

    if (es->save_chain == NULL) {
        es->save_chain = new_entry;
        new_entry->prev = NULL;
    }
    else {
        es->save_chain->next = new_entry;
        new_entry->prev = es->save_chain;
    }

    new_entry->root_tree = NULL;
    new_entry->active_tree = NULL;
    new_entry->entered_tree = NULL;
    new_entry->next = NULL;
}

static void grow_checkpoints(loli_expr_state *es)
{
    es->checkpoint_size *= 2;

    es->checkpoints = loli_realloc(es->checkpoints,
            es->checkpoint_size * sizeof(*es->checkpoints));

    int i;
    for (i = es->checkpoint_pos;i < es->checkpoint_size;i++) {
        loli_ast_checkpoint_entry *new_point = loli_malloc(sizeof(*new_point));
        es->checkpoints[i] = new_point;
    }
}

void loli_es_flush(loli_expr_state *es)
{
    es->root = NULL;
    es->active = NULL;
    es->next_available = es->first_tree;
    es->pile_current = es->pile_start;
}

void loli_es_checkpoint_save(loli_expr_state *es)
{
    if (es->checkpoint_pos == es->checkpoint_size)
        grow_checkpoints(es);

    loli_ast_checkpoint_entry *checkpoint = es->checkpoints[es->checkpoint_pos];

    checkpoint->root = es->root;
    checkpoint->active = es->active;
    checkpoint->pile_start = es->pile_start;
    checkpoint->first_tree = es->first_tree;

    es->active = NULL;
    es->root = NULL;
    es->first_tree = es->next_available;

    es->checkpoint_pos++;
}

void loli_es_checkpoint_restore(loli_expr_state *es)
{
    es->checkpoint_pos--;

    loli_ast_checkpoint_entry *checkpoint = es->checkpoints[es->checkpoint_pos];

    es->root = checkpoint->root;
    es->active = checkpoint->active;
    es->pile_start = checkpoint->pile_start;
    es->first_tree = checkpoint->first_tree;
}

void loli_es_checkpoint_reverse_n(loli_expr_state *es, int count)
{
    int to = es->checkpoint_pos - 1;
    int from = to - count + 1;
    int range = (to + 1 - from) / 2;

    for (;range;range--, from++, to--) {
        loli_ast_checkpoint_entry *temp = es->checkpoints[from];
        es->checkpoints[from] = es->checkpoints[to];
        es->checkpoints[to] = temp;
    }
}

static void add_new_tree(loli_expr_state *es)
{
    loli_ast *new_tree = loli_malloc(sizeof(*new_tree));

    new_tree->next_tree = NULL;

    es->next_available->next_tree = new_tree;
}



static void merge_absorb(loli_expr_state *es, loli_ast *given, loli_ast *new_tree)
{
     
    if (given == es->active) {
        es->active = new_tree;
        if (given == es->root)
            es->root = new_tree;
    }

    given->parent = new_tree;
    new_tree->arg_start = given;
    new_tree->args_collected = 1;
    new_tree->next_arg = NULL;
}

static void merge_unary(loli_expr_state *es, loli_ast *given, loli_ast *new_tree)
{
     
    while (given->tree_type == tree_unary &&
           given->left != NULL &&
           given->left->tree_type == tree_unary)
        given = given->left;

    if (given->left == NULL)
         
        given->left = new_tree;
    else {
         
        merge_absorb(es, given->left, new_tree);
        given->left = new_tree;
    }

    new_tree->parent = given;
}

static void merge_value(loli_expr_state *es, loli_ast *new_tree)
{
    loli_ast *active = es->active;

    if (active != NULL) {
        if (active->tree_type == tree_binary) {
             
            if (active->right == NULL) {
                active->right = new_tree;
                new_tree->parent = active;
            }
            else if (active->right->tree_type == tree_unary)
                 
                merge_unary(es, active->right, new_tree);
            else {
                 
                merge_absorb(es, active->right, new_tree);
                active->right = new_tree;
                new_tree->parent = active;
            }
        }
        else if (active->tree_type == tree_unary)
            merge_unary(es, active, new_tree);
        else
            merge_absorb(es, active, new_tree);
    }
    else {
         
        es->root = new_tree;
        es->active = new_tree;
    }
}


static uint8_t priority_for_op(loli_expr_op o)
{
    int prio;

    switch (o) {
        case expr_assign:
        case expr_div_assign:
        case expr_mul_assign:
        case expr_plus_assign:
        case expr_minus_assign:
        case expr_left_shift_assign:
        case expr_right_shift_assign:
            prio = 1;
            break;
        case expr_logical_or:
            prio = 2;
            break;
        case expr_logical_and:
            prio = 3;
            break;
        case expr_eq_eq:
        case expr_not_eq:
        case expr_lt:
        case expr_gr:
        case expr_lt_eq:
        case expr_gr_eq:
            prio = 4;
            break;
         
        case expr_plus_plus:
            prio = 5;
            break;
         
        case expr_func_pipe:
            prio = 6;
            break;
        case expr_bitwise_or:
        case expr_bitwise_xor:
        case expr_bitwise_and:
            prio = 7;
            break;
        case expr_left_shift:
        case expr_right_shift:
            prio = 8;
            break;
        case expr_plus:
        case expr_minus:
            prio = 9;
            break;
        case expr_multiply:
        case expr_divide:
        case expr_modulo:
            prio = 10;
            break;
        case expr_named_arg:
            prio = 0;
            break;
        default:
             
            prio = -1;
            break;
    }

    return prio;
}

static void push_tree_arg(loli_expr_state *es, loli_ast *entered_tree,
        loli_ast *arg)
{
     
    if (arg == NULL)
        return;

    if (entered_tree->arg_start == NULL)
        entered_tree->arg_start = arg;
    else {
        loli_ast *tree_iter = entered_tree->arg_start;
        while (tree_iter->next_arg != NULL)
            tree_iter = tree_iter->next_arg;

        tree_iter->next_arg = arg;
    }

    arg->parent = entered_tree;
    arg->next_arg = NULL;
    entered_tree->args_collected++;
}

void loli_es_collect_arg(loli_expr_state *es)
{
    loli_ast_save_entry *entry = es->save_chain;

    push_tree_arg(es, entry->entered_tree, es->root);

     
    es->root = NULL;
    es->active = NULL;
}

void loli_es_enter_tree(loli_expr_state *es, loli_tree_type tree_type)
{
    AST_ENTERABLE_INIT(a, tree_type)

    merge_value(es, a);

    loli_ast_save_entry *save_entry = es->save_chain;
     
    if (save_entry->entered_tree != NULL) {
        if (save_entry->next == NULL)
            add_save_entry(es);

        es->save_chain = es->save_chain->next;
        save_entry = es->save_chain;
    }

    save_entry->root_tree = es->root;
    save_entry->active_tree = es->active;
    save_entry->entered_tree = a;
    es->save_depth++;

    es->root = NULL;
    es->active = NULL;
}

void loli_es_leave_tree(loli_expr_state *es)
{
    loli_ast_save_entry *entry = es->save_chain;

    push_tree_arg(es, entry->entered_tree, es->root);

    es->root = entry->root_tree;
    es->active = entry->active_tree;

    if (es->save_chain->prev == NULL)
         
        es->save_chain->entered_tree = NULL;
    else
        es->save_chain = es->save_chain->prev;

    es->save_depth--;
}

loli_ast *loli_es_get_saved_tree(loli_expr_state *es)
{
    return es->save_chain->entered_tree;
}



void loli_es_push_binary_op(loli_expr_state *es, loli_expr_op op)
{
     
    AST_COMMON_INIT(new_ast, tree_binary)
    new_ast->priority = priority_for_op(op);
    new_ast->op = op;
    new_ast->left = NULL;
    new_ast->right = NULL;

    loli_ast *active = es->active;

     
    if (active->tree_type < tree_binary) {
         
        if (es->root == active)
            es->root = new_ast;

        active->parent = new_ast;

        new_ast->left = active;
        es->active = new_ast;
    }
    else if (active->tree_type == tree_binary) {
         
        int new_prio, active_prio;
        new_prio = new_ast->priority;
        active_prio = active->priority;
        if ((new_prio > active_prio) || new_prio == 1) {
             
            new_ast->left = active->right;
            new_ast->left->parent = new_ast;

            active->right = new_ast;
            active->right->parent = active;

            es->active = new_ast;
        }
        else {
             
            loli_ast *tree = active;
            while (tree->parent) {
                if (new_prio > tree->parent->priority)
                    break;

                tree = tree->parent;
            }
            if (tree->parent != NULL) {
                 
                loli_ast *parent = tree->parent;

                parent->right = new_ast;
                new_ast->parent = parent;
            }
            else
                 
                es->root = new_ast;

            new_ast->left = tree;
            new_ast->left->parent = new_ast;

            es->active = new_ast;
        }
    }
}

static void push_type(loli_expr_state *es, loli_type *type)
{
    AST_COMMON_INIT(a, tree_type)
    a->type = type;

    merge_value(es, a);
}

void loli_es_enter_typecast(loli_expr_state *es, loli_type *type)
{
    loli_es_enter_tree(es, tree_typecast);
    push_type(es, type);
    loli_es_collect_arg(es);
}

void loli_es_push_unary_op(loli_expr_state *es, loli_expr_op op)
{
    AST_COMMON_INIT(a, tree_unary)

    a->left = NULL;
    a->op = op;

    merge_value(es, a);
}

void loli_es_push_local_var(loli_expr_state *es, loli_var *var)
{
    AST_COMMON_INIT(a, tree_local_var);
    a->result = (loli_sym *)var;
    a->sym = (loli_sym *)var;

    merge_value(es, a);
}

void loli_es_push_global_var(loli_expr_state *es, loli_var *var)
{
    AST_COMMON_INIT(a, tree_global_var);
    a->result = (loli_sym *)var;
    a->sym = (loli_sym *)var;

    merge_value(es, a);
}

void loli_es_push_upvalue(loli_expr_state *es, loli_var *var)
{
    AST_COMMON_INIT(a, tree_upvalue);
    a->sym = (loli_sym *)var;

    merge_value(es, a);
}

void loli_es_push_defined_func(loli_expr_state *es, loli_var *func)
{
    AST_COMMON_INIT(a, tree_defined_func);
    a->result = (loli_sym *)func;
    a->sym = (loli_sym *)func;

    merge_value(es, a);
}

void loli_es_push_method(loli_expr_state *es, loli_var *func)
{
    AST_COMMON_INIT(a, tree_method);
    a->result = (loli_sym *)func;
    a->sym = (loli_sym *)func;

    merge_value(es, a);
}

void loli_es_push_static_func(loli_expr_state *es, loli_var *func)
{
    AST_COMMON_INIT(a, tree_static_func);
    a->result = (loli_sym *)func;
    a->sym = (loli_sym *)func;

    merge_value(es, a);
}

void loli_es_push_inherited_new(loli_expr_state *es, loli_var *func)
{
    AST_COMMON_INIT(a, tree_inherited_new);
    a->result = (loli_sym *)func;
    a->sym = (loli_sym *)func;

    merge_value(es, a);
}

void loli_es_push_literal(loli_expr_state *es, loli_type *t, uint16_t reg_spot)
{
    AST_COMMON_INIT(a, tree_literal);
    a->type = t;
    a->literal_reg_spot = reg_spot;

    merge_value(es, a);
}

void loli_es_push_boolean(loli_expr_state *es, int16_t value)
{
    AST_COMMON_INIT(a, tree_boolean);
    a->backing_value = value;

    merge_value(es, a);
}

void loli_es_push_byte(loli_expr_state *es, uint8_t value)
{
    AST_COMMON_INIT(a, tree_byte);
    a->backing_value = value;

    merge_value(es, a);
}

void loli_es_push_integer(loli_expr_state *es, int16_t value)
{
    AST_COMMON_INIT(a, tree_integer);
    a->backing_value = value;

    merge_value(es, a);
}

void loli_es_push_property(loli_expr_state *es, loli_prop_entry *prop)
{
    AST_COMMON_INIT(a, tree_property);
    a->property = prop;

    merge_value(es, a);
}

void loli_es_push_variant(loli_expr_state *es, loli_variant_class *variant)
{
    AST_COMMON_INIT(a, tree_variant);
    a->variant = variant;

    merge_value(es, a);
}

void loli_es_push_self(loli_expr_state *es)
{
    AST_COMMON_INIT(a, tree_self);

    merge_value(es, a);
}

void loli_es_push_text(loli_expr_state *es, loli_tree_type tt, uint32_t start,
        int pos)
{
    AST_COMMON_INIT(a, tt)

    a->pile_pos = pos;
    a->line_num = start;

    merge_value(es, a);
}
