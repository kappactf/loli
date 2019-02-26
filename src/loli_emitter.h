#ifndef LOLI_EMITTER_H
# define LOLI_EMITTER_H

# include "loli_raiser.h"
# include "loli_symtab.h"
# include "loli_type_system.h"
# include "loli_type_maker.h"
# include "loli_buffer_u16.h"
# include "loli_string_pile.h"

typedef enum {
    block_if,
    block_if_elif,
    block_if_else,
    block_while,
    block_do_while,
    block_for_in,
    block_try,
    block_try_except,
    block_try_except_all,
    block_match,
    block_enum,
     
    block_define,
    block_class,
    block_lambda,
    block_file
} loli_block_type;

# define BLOCK_MAKE_CLOSURE 0x1
# define BLOCK_ALWAYS_EXITS 0x2

typedef struct loli_block_ {
     
    loli_var *function_var;

     
    uint16_t patch_start;

    uint16_t storage_start;

     
    uint16_t match_case_start;

    uint16_t var_count;

    uint8_t flags;

    loli_block_type block_type : 8;

    uint16_t pending_future_decls;

     
    uint32_t code_start;

     
    uint32_t next_reg_spot;

     
    int32_t last_exit;

     
    loli_class *class_entry;

     
    struct loli_block_ *prev_function_block;

     
    loli_storage *self;

    struct loli_block_ *next;
    struct loli_block_ *prev;
} loli_block;

typedef struct loli_storage_stack_ {
    loli_storage **data;
    uint16_t scope_end;
    uint16_t size;
    uint32_t pad;
} loli_storage_stack;

typedef struct loli_proto_stack_ {
    loli_proto **data;
    uint32_t pos;
    uint32_t size;
} loli_proto_stack;

struct loli_parse_state_t;

typedef struct {
     
    loli_buffer_u16 *patches;

     
    int *match_cases;

     
    loli_buffer_u16 *code;

     
    loli_buffer_u16 *closure_aux_code;

    loli_buffer_u16 *closure_spots;

    uint16_t *transform_table;

    uint64_t transform_size;

    uint16_t match_case_pos;

    uint16_t match_case_size;

    uint32_t pad;

    struct loli_storage_stack_ *storages;

    struct loli_proto_stack_ *protos;

     
    loli_block *main_block;

     
    loli_block *function_block;

     
    loli_block *block;

     
    uint16_t class_block_depth;

     
    uint16_t function_depth;

     
    uint32_t expr_num;

     
    uint32_t *lex_linenum;

    loli_raiser *raiser;

     
    loli_string_pile *expr_strings;

    loli_type_system *ts;

    loli_type_maker *tm;

     
    struct loli_parse_state_ *parser;


     
    loli_symtab *symtab;
} loli_emit_state;

void loli_emit_eval_condition(loli_emit_state *, loli_expr_state *);
void loli_emit_eval_expr_to_var(loli_emit_state *, loli_expr_state *,
        loli_var *);
void loli_emit_eval_expr(loli_emit_state *, loli_expr_state *);
void loli_emit_finalize_for_in(loli_emit_state *, loli_var *, loli_var *,
        loli_var *, loli_sym *, int);
void loli_emit_eval_lambda_body(loli_emit_state *, loli_expr_state *, loli_type *);
void loli_emit_write_import_call(loli_emit_state *, loli_var *);
void loli_emit_eval_optarg(loli_emit_state *, loli_ast *);
void loli_emit_eval_optarg_keyed(loli_emit_state *, loli_ast *);

void loli_emit_eval_match_expr(loli_emit_state *, loli_expr_state *);
int loli_emit_is_duplicate_case(loli_emit_state *, loli_class *);
void loli_emit_change_match_branch(loli_emit_state *);
void loli_emit_write_match_case(loli_emit_state *, loli_sym *, loli_class *);
void loli_emit_decompose(loli_emit_state *, loli_sym *, int, uint16_t);

void loli_emit_break(loli_emit_state *);
void loli_emit_continue(loli_emit_state *);
void loli_emit_eval_return(loli_emit_state *, loli_expr_state *, loli_type *);

void loli_emit_change_block_to(loli_emit_state *emit, int);

void loli_emit_enter_block(loli_emit_state *, loli_block_type);
void loli_emit_enter_call_block(loli_emit_state *, loli_block_type, loli_var *);
void loli_emit_leave_block(loli_emit_state *);
void loli_emit_leave_call_block(loli_emit_state *, uint16_t);
void loli_emit_leave_future_call(loli_emit_state *);
void loli_emit_resolve_future_decl(loli_emit_state *, loli_var *);

void loli_emit_try(loli_emit_state *, int);
void loli_emit_except(loli_emit_state *, loli_type *, loli_var *, int);
void loli_emit_raise(loli_emit_state *, loli_expr_state *);

void loli_emit_write_keyless_optarg_header(loli_emit_state *, loli_type *);
void loli_emit_write_class_header(loli_emit_state *, loli_type *, uint16_t);
void loli_emit_write_shorthand_ctor(loli_emit_state *, loli_class *, loli_var *,
        uint16_t);

loli_proto *loli_emit_new_proto(loli_emit_state *, const char *, const char *,
        const char *);
loli_proto *loli_emit_proto_for_var(loli_emit_state *, loli_var *);

void loli_prepare_main(loli_emit_state *, loli_function_val *);
void loli_reset_main(loli_emit_state *);

void loli_free_emit_state(loli_emit_state *);
loli_emit_state *loli_new_emit_state(loli_symtab *, loli_raiser *);
#endif
