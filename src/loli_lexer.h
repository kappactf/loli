#ifndef LOLI_LEXER_H
# define LOLI_LEXER_H

# include <stdio.h>

# include "loli_raiser.h"
# include "loli_symtab.h"

typedef enum {
    tk_right_parenth,
    tk_comma,
    tk_left_curly,
    tk_right_curly,
    tk_left_bracket,
    tk_colon,
    tk_tilde,
    tk_bitwise_xor,
    tk_bitwise_xor_eq,
    tk_not,
    tk_not_eq,
    tk_modulo,
    tk_modulo_eq,
    tk_multiply,
    tk_multiply_eq,
    tk_divide,
    tk_divide_eq,
    tk_plus,
    tk_plus_plus,
    tk_plus_eq,
    tk_minus,
    tk_minus_eq,
     
    tk_lt,
    tk_lt_eq,
    tk_left_shift,
    tk_left_shift_eq,
    tk_gt,
    tk_gt_eq,
    tk_right_shift,
    tk_right_shift_eq,
    tk_equal,
    tk_eq_eq,
    tk_left_parenth,
    tk_lambda,        
    tk_tuple_open,    
    tk_tuple_close,   
     
    tk_right_bracket,
    tk_arrow,
    tk_word,
    tk_prop_word,
    tk_double_quote,
    tk_bytestring,
    tk_byte,
    tk_integer,
    tk_double,
    tk_docstring,
    tk_keyword_arg,
    tk_dot,
    tk_bitwise_and,
    tk_bitwise_and_eq,
    tk_logical_and,
    tk_bitwise_or,
    tk_bitwise_or_eq,
    tk_logical_or,
    tk_typecast_parenth,
    tk_three_dots,
    tk_func_pipe,
    tk_scoop,
    tk_invalid,
    tk_end_lambda,
    tk_end_tag,
    tk_eof
} loli_token;

typedef enum {
    et_file,
    et_shallow_string,
    et_copied_string,
    et_lambda,
} loli_lex_entry_type;

typedef struct loli_lex_entry_ {
    struct loli_lex_state_ *lexer;

    loli_literal *saved_last_literal;
    char *saved_input;
    uint16_t saved_input_pos;
    uint16_t saved_input_size;
    loli_token saved_token : 16;
    loli_token final_token : 16;
    uint32_t saved_line_num;
    loli_lex_entry_type entry_type : 16;
    uint16_t pad;
    int64_t saved_last_integer;

    void *source;
    void *extra;

    struct loli_lex_entry_ *prev;
    struct loli_lex_entry_ *next;
} loli_lex_entry;

typedef struct loli_lex_state_ {
    loli_lex_entry *entry;
    char *ch_class;
    char *input_buffer;
    char *label;

    uint32_t line_num;
    uint32_t expand_start_line;

    uint16_t pad;
    uint16_t label_size;

    uint16_t input_size;
    uint16_t input_pos;

    int64_t last_integer;

    loli_token token;

     
    loli_literal *last_literal;
    loli_symtab *symtab;
    loli_raiser *raiser;
} loli_lex_state;

void loli_free_lex_state(loli_lex_state *);
void loli_rewind_lex_state(loli_lex_state *);
void loli_grow_lexer_buffers(loli_lex_state *);
void loli_lexer(loli_lex_state *);
int loli_lexer_digit_rescan(loli_lex_state *);
void loli_verify_template(loli_lex_state *);
int loli_lexer_read_content(loli_lex_state *, char **);
void loli_lexer_verify_path_string(loli_lex_state *);

void loli_lexer_load(loli_lex_state *, loli_lex_entry_type, const void *);

void loli_pop_lex_entry(loli_lex_state *);
loli_lex_state *loli_new_lex_state(loli_raiser *);
char *tokname(loli_token);

#endif
