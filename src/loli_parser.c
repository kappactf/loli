#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "loli.h"

#include "loli_config.h"
#include "loli_library.h"
#include "loli_parser.h"
#include "loli_parser_tok_table.h"
#include "loli_keyword_table.h"
#include "loli_string_pile.h"
#include "loli_value_flags.h"
#include "loli_value_raw.h"
#include "loli_alloc.h"

#include "loli_int_opcode.h"

#define NEED_NEXT_TOK(expected) \
loli_lexer(lex); \
if (lex->token != expected) \
    loli_raise_syn(parser->raiser, "Expected '%s', not '%s'.", \
               tokname(expected), tokname(lex->token));

#define NEED_CURRENT_TOK(expected) \
if (lex->token != expected) \
    loli_raise_syn(parser->raiser, "Expected '%s', not '%s'.", \
               tokname(expected), tokname(lex->token));

extern loli_type *loli_question_type;
extern loli_class *loli_self_class;
extern loli_type *loli_unit_type;


static loli_module_entry *new_module(loli_parse_state *);
static void create_main_func(loli_parse_state *);
void loli_module_register(loli_state *, const char *, const char **,
        loli_call_entry_func *);
void loli_default_import_func(loli_state *, const char *);
void loli_stdout_print(loli_vm_state *);

typedef struct loli_rewind_state_
{
    loli_class *main_class_start;
    loli_var *main_var_start;
    loli_boxed_sym *main_boxed_start;
    loli_module_link *main_last_module_link;
    loli_module_entry *main_last_module;
    uint32_t line_num;
    uint32_t pending;
} loli_rewind_state;

typedef struct loli_import_state_ {
     
    loli_msgbuf *path_msgbuf;

     
    loli_module_entry *last_import;

     
    loli_module_entry *source_module;

     
    const char *pending_loadname;

     
    const char *dirname;

     
    int is_package_import;
} loli_import_state;

extern const char *loli_builtin_info_table[];
extern loli_call_entry_func loli_builtin_call_table[];

extern const char *loli_sys_info_table[];
extern loli_call_entry_func loli_sys_call_table[];

extern const char *loli_random_info_table[];
extern loli_call_entry_func loli_random_call_table[];

extern const char *loli_time_info_table[];
extern loli_call_entry_func loli_time_call_table[];

extern const char *loli_math_info_table[];
extern loli_call_entry_func loli_math_call_table[];

extern const char *loli_hash_info_table[];
extern loli_call_entry_func loli_hash_call_table[];

void loli_init_pkg_builtin(loli_symtab *);

void loli_config_init(loli_config *conf)
{
    conf->argc = 0;
    conf->argv = NULL;

    conf->copy_str_input = 0;

     
    conf->gc_start = 100;
    conf->gc_multiplier = 4;

    char key[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf};

    memcpy(conf->sipkey, key, sizeof(key));

    conf->import_func = loli_default_import_func;
    conf->render_func = (loli_render_func)fputs;
    conf->data = stdout;
}

loli_state *loli_new_state(loli_config *config)
{
    loli_parse_state *parser = loli_malloc(sizeof(*parser));
    parser->module_top = NULL;
    parser->module_start = NULL;
    parser->config = config;

    loli_raiser *raiser = loli_new_raiser();

    parser->import_pile_current = 0;
    parser->class_self_type = NULL;
    parser->raiser = raiser;
    parser->msgbuf = loli_new_msgbuf(64);
    parser->expr = loli_new_expr_state();
    parser->generics = loli_new_generic_pool();
    parser->symtab = loli_new_symtab(parser->generics);
    parser->vm = loli_new_vm_state(raiser);
    parser->rs = loli_malloc(sizeof(*parser->rs));
    parser->rs->pending = 0;
    parser->ims = loli_malloc(sizeof(*parser->ims));
    parser->ims->path_msgbuf = loli_new_msgbuf(64);

    parser->vm->gs->parser = parser;
    parser->vm->gs->gc_multiplier = config->gc_multiplier;
    parser->vm->gs->gc_threshold = config->gc_start;

     
    loli_module_register(parser->vm, "builtin", loli_builtin_info_table,
            loli_builtin_call_table);
    loli_set_builtin(parser->symtab, parser->module_top);
    loli_init_pkg_builtin(parser->symtab);

    parser->emit = loli_new_emit_state(parser->symtab, raiser);
    parser->lex = loli_new_lex_state(raiser);
    parser->data_stack = loli_new_buffer_u16(4);
    parser->keyarg_strings = loli_new_string_pile();
    parser->keyarg_current = 0;

     
    parser->tm = parser->emit->tm;

    parser->expr->lex_linenum = &parser->lex->line_num;

    parser->emit->lex_linenum = &parser->lex->line_num;
    parser->emit->symtab = parser->symtab;
    parser->emit->parser = parser;

    parser->lex->symtab = parser->symtab;

    parser->expr_strings = parser->emit->expr_strings;
    parser->import_ref_strings = loli_new_string_pile();

    loli_module_entry *main_module = new_module(parser);

    parser->main_module = main_module;
    parser->symtab->active_module = parser->main_module;

     
    create_main_func(parser);

    loli_module_register(parser->vm, "sys", loli_sys_info_table,
            loli_sys_call_table);
    loli_module_register(parser->vm, "random", loli_random_info_table,
            loli_random_call_table);
    loli_module_register(parser->vm, "time", loli_time_info_table,
            loli_time_call_table);
    loli_module_register(parser->vm, "math", loli_math_info_table,
            loli_math_call_table);
    loli_module_register(parser->vm, "hash", loli_hash_info_table,
            loli_hash_call_table);     

    parser->executing = 0;
    parser->content_to_parse = 0;

    return parser->vm;
}

static void free_links_until(loli_module_link *link_iter,
        loli_module_link *stop)
{
    while (link_iter != stop) {
        loli_module_link *link_next = link_iter->next_module;
        loli_free(link_iter->as_name);
        loli_free(link_iter);
        link_iter = link_next;
    }
}

#define free_links(iter) free_links_until(iter, NULL)

void loli_free_state(loli_state *vm)
{
    loli_parse_state *parser = vm->gs->parser;

     
    parser->toplevel_func->proto->code = NULL;

    loli_free_raiser(parser->raiser);

    loli_free_expr_state(parser->expr);

    loli_free_vm(parser->vm);

    loli_free_lex_state(parser->lex);

    loli_free_emit_state(parser->emit);

    loli_free_buffer_u16(parser->data_stack);

     
    parser->main_module->path = NULL;

    loli_module_entry *module_iter = parser->module_start;
    loli_module_entry *module_next = NULL;

    while (module_iter) {
        free_links(module_iter->module_chain);

        module_next = module_iter->root_next;

        if (module_iter->handle)
            loli_library_free(module_iter->handle);

        loli_free_module_symbols(parser->symtab, module_iter);
        loli_free(module_iter->path);
        loli_free(module_iter->dirname);
        loli_free(module_iter->loadname);
        loli_free(module_iter->cid_table);
        loli_free(module_iter);

        module_iter = module_next;
    }

    loli_free_string_pile(parser->import_ref_strings);
    loli_free_string_pile(parser->keyarg_strings);
    loli_free_symtab(parser->symtab);
    loli_free_generic_pool(parser->generics);
    loli_free_msgbuf(parser->msgbuf);
    loli_free_msgbuf(parser->ims->path_msgbuf);
    loli_free(parser->ims);
    loli_free(parser->rs);

    loli_free(parser);
}

static void rewind_parser(loli_parse_state *parser, loli_rewind_state *rs)
{
    loli_u16_set_pos(parser->data_stack, 0);
    parser->import_pile_current = 0;

    loli_module_entry *module_iter = rs->main_last_module;
    while (module_iter) {
         
        if (module_iter->flags & MODULE_IN_EXECUTION) {
            module_iter->cmp_len = 0;
            module_iter->flags &= ~MODULE_IN_EXECUTION;
        }
        module_iter = module_iter->root_next;
    }

     
    loli_generic_pool *gp = parser->generics;
    gp->scope_start = 0;
    gp->scope_end = 0;

     
    loli_expr_state *es = parser->expr;
    es->root = NULL;
    es->active = NULL;

    if (es->checkpoint_pos) {
        es->first_tree = es->checkpoints[0]->first_tree;
        es->checkpoint_pos = 0;
    }

    es->next_available = es->first_tree;

    loli_ast_save_entry *save_iter = es->save_chain;
    while (1) {
        save_iter->entered_tree = NULL;
        if (save_iter->prev == NULL)
            break;
        save_iter = save_iter->prev;
    }
    es->save_chain = save_iter;
    es->save_depth = 0;
    es->pile_start = 0;
    es->pile_current = 0;

     
    loli_emit_state *emit = parser->emit;
    loli_u16_set_pos(emit->patches, 0);
    loli_u16_set_pos(emit->code, 0);
    loli_u16_set_pos(emit->closure_spots, 0);

    emit->match_case_pos = 0;

    loli_block *block_stop = emit->block->next;
    loli_block *block_iter = emit->main_block->next;
    while (block_iter != block_stop) {
        if (block_iter->block_type >= block_define) {
            emit->storages->scope_end = block_iter->storage_start;
            break;
        }
        block_iter = block_iter->next;
    }

    emit->block = emit->main_block;
    emit->block->pending_future_decls = 0;
    emit->function_block = emit->main_block;
    emit->function_depth = 1;
    emit->class_block_depth = 0;

     
    loli_type_system *ts = parser->emit->ts;
     
    ts->base = ts->types;
    ts->num_used = 0;
    ts->pos = 0;
    loli_ts_reset_scoops(parser->emit->ts);

     
    loli_rewind_lex_state(parser->lex);
    parser->lex->line_num = rs->line_num;

     
    loli_raiser *raiser = parser->raiser;
    loli_mb_flush(raiser->msgbuf);
    loli_mb_flush(raiser->aux_msgbuf);
    raiser->line_adjust = 0;

     
    loli_vm_state *vm = parser->vm;

    loli_vm_catch_entry *catch_iter = vm->catch_chain;
    while (catch_iter->prev)
        catch_iter = catch_iter->prev;

    vm->catch_chain = catch_iter;
    vm->exception_value = NULL;
    vm->exception_cls = NULL;

    loli_call_frame *call_iter = vm->call_chain;
    while (call_iter->prev)
        call_iter = call_iter->prev;

    vm->call_chain = call_iter;
    vm->call_depth = 0;

     
    loli_rewind_symtab(parser->symtab, parser->main_module,
            rs->main_class_start, rs->main_var_start, rs->main_boxed_start,
            parser->executing);
}

static void handle_rewind(loli_parse_state *parser)
{
    loli_rewind_state *rs = parser->rs;

    if (parser->rs->pending) {
        rewind_parser(parser, rs);
        parser->rs->pending = 0;
    }

    loli_module_entry *main_module = parser->main_module;
    rs->main_class_start = main_module->class_chain;
    rs->main_var_start = main_module->var_chain;
    rs->main_boxed_start = main_module->boxed_chain;

    rs->main_last_module_link = main_module->module_chain;
    rs->main_last_module = parser->module_top;
    rs->line_num = parser->lex->line_num;
}



static loli_module_entry *new_module(loli_parse_state *parser)
{
    loli_module_entry *module = loli_malloc(sizeof(*module));

    module->loadname = NULL;
    module->dirname = NULL;
    module->path = NULL;
    module->cmp_len = 0;
    module->info_table = NULL;
    module->cid_table = NULL;
    module->root_next = NULL;
    module->module_chain = NULL;
    module->class_chain = NULL;
    module->var_chain = NULL;
    module->handle = NULL;
    module->call_table = NULL;
    module->boxed_chain = NULL;
    module->item_kind = ITEM_TYPE_MODULE;
     
    module->flags = MODULE_NOT_EXECUTED;
    module->root_dirname = NULL;

    if (parser->module_start) {
        parser->module_top->root_next = module;
        parser->module_top = module;
    }
    else {
        parser->module_start = module;
        parser->module_top = module;
    }

    parser->ims->last_import = module;

    return module;
}

static void add_data_to_module(loli_module_entry *module, void *handle,
        const char **table, loli_foreign_func *call_table)
{
    module->handle = handle;
    module->info_table = table;
    module->call_table = call_table;
    module->flags &= ~MODULE_NOT_EXECUTED;

    unsigned char cid_count = module->info_table[0][0];

    if (cid_count) {
        module->cid_table = loli_malloc(cid_count * sizeof(*module->cid_table));
        memset(module->cid_table, 0, cid_count * sizeof(*module->cid_table));
    }
}

static void add_path_to_module(loli_module_entry *module,
            const char *loadname, const char *path)
{
    module->loadname = loli_malloc(
            (strlen(loadname) + 1) * sizeof(*module->loadname));
    strcpy(module->loadname, loadname);

    if (path[0] == '.' && path[1] == LOLI_PATH_CHAR)
        path += 2;

    module->cmp_len = strlen(path);
    module->path = loli_malloc((strlen(path) + 1) * sizeof(*module->path));
    strcpy(module->path, path);
}

static char *dir_from_path(const char *path)
{
    const char *slash = strrchr(path, LOLI_PATH_CHAR);
    char *out;

    if (slash == NULL) {
        out = loli_malloc(1 * sizeof(*out));
        out[0] = '\0';
    }
    else {
        int bare_len = slash - path;
        out = loli_malloc((bare_len + 1) * sizeof(*out));

        strncpy(out, path, bare_len);
        out[bare_len] = '\0';
    }

    return out;
}

static void set_dirs_on_module(loli_parse_state *parser,
        loli_module_entry *module)
{
     
    if (parser->ims->is_package_import) {
        module->dirname = dir_from_path(module->path);
        module->root_dirname = module->dirname;
    }
    else
        module->root_dirname = parser->ims->source_module->root_dirname;
}

static void add_failed_import_path(loli_parse_state *parser, const char *path)
{
     
    loli_buffer_u16 *b = parser->data_stack;
    uint16_t pos = loli_u16_get(b, loli_u16_pos(b) - 1);
    loli_sp_insert(parser->expr_strings, path, &pos);
    loli_u16_write_1(b, pos);
}

static int import_check(loli_parse_state *parser, const char *path)
{
    loli_module_entry *m = parser->ims->last_import;

    if (m == NULL) {
        m = loli_find_module_by_path(parser->symtab, path);
        if (m != NULL)
            parser->ims->last_import = m;
    }

    return (m != NULL);
}

static loli_lex_entry_type string_input_mode(loli_parse_state *parser)
{
    loli_lex_entry_type mode = et_shallow_string;

    if (parser->config->copy_str_input)
        mode = et_copied_string;

    return mode;
}

static void add_fixslash_dir(loli_msgbuf *msgbuf, const char *input_str)
{
#ifdef _WIN32
     
    int start = 0, stop = 0;
    const char *ch = &input_str[0];

    while (1) {
        if (*ch == '/') {
            stop = (ch - input_str);
            loli_mb_add_slice(msgbuf, input_str, start, stop);
            loli_mb_add_char(msgbuf, '\\');
            start = stop + 1;
        }
        else if (*ch == '\0')
            break;

        ch++;
    }

    if (start != 0) {
        stop = (ch - input_str);
        loli_mb_add_slice(msgbuf, input_str, start, stop);
    }
#else
     
    loli_mb_add(msgbuf, input_str);
#endif
    int len = strlen(input_str);

    if (input_str[len] != LOLI_PATH_CHAR)
        loli_mb_add_char(msgbuf, LOLI_PATH_CHAR);
}

static const char *build_import_path(loli_import_state *ims, const char *target,
        const char *suffix)
{
    loli_msgbuf *path_msgbuf = loli_mb_flush(ims->path_msgbuf);
    const char *root_dirname = ims->source_module->root_dirname;

    if (root_dirname == NULL || root_dirname[0] == '\0')
        loli_mb_add_char(path_msgbuf, '.');
    else
        loli_mb_add(path_msgbuf, root_dirname);

    loli_mb_add_char(path_msgbuf, LOLI_PATH_CHAR);

    if (ims->dirname[0] != '\0')
        add_fixslash_dir(path_msgbuf, ims->dirname);

    if (ims->is_package_import == 1) {
        loli_mb_add_fmt(path_msgbuf,
                "pkg" LOLI_PATH_SLASH
                "%s" LOLI_PATH_SLASH, target);
    }

    loli_mb_add(path_msgbuf, target);
    loli_mb_add(path_msgbuf, suffix);

    return loli_mb_raw(path_msgbuf);
}

int loli_import_file(loli_state *s, const char *name)
{
    loli_parse_state *parser = s->gs->parser;
    const char *path = build_import_path(parser->ims, name, ".li");

    if (import_check(parser, path))
        return 1;

    FILE *source = fopen(path, "r");
    if (source == NULL) {
        add_failed_import_path(parser, path);
        return 0;
    }

    loli_lexer_load(parser->lex, et_file, source);

    loli_module_entry *module = new_module(parser);

    add_path_to_module(module, parser->ims->pending_loadname, path);
    set_dirs_on_module(parser, module);
    return 1;
}

int loli_import_string(loli_state *s, const char *name, const char *source)
{
    loli_parse_state *parser = s->gs->parser;
    const char *path = build_import_path(parser->ims, source, ".li");

    if (import_check(parser, path))
        return 1;

    loli_lexer_load(parser->lex, string_input_mode(parser), (char *)source);

    loli_module_entry *module = new_module(parser);

    add_path_to_module(module, parser->ims->pending_loadname, path);
    set_dirs_on_module(parser, module);
    return 1;
}

int loli_import_library(loli_state *s, const char *name)
{
    loli_parse_state *parser = s->gs->parser;
    const char *path = build_import_path(parser->ims, name,
            "." LOLI_LIB_SUFFIX);

    if (import_check(parser, path))
        return 1;

    void *handle = loli_library_load(path);
    if (handle == NULL) {
        add_failed_import_path(parser, path);
        return 0;
    }

    loli_msgbuf *msgbuf = loli_mb_flush(parser->msgbuf);
    const char *loadname = parser->ims->pending_loadname;

    const char **info_table = (const char **)loli_library_get(handle,
            loli_mb_sprintf(msgbuf, "loli_%s_info_table", loadname));

    loli_foreign_func *call_table = loli_library_get(handle,
            loli_mb_sprintf(msgbuf, "loli_%s_call_table", loadname));

    if (info_table == NULL || call_table == NULL) {
        add_failed_import_path(parser, path);
        loli_library_free(handle);
        return 0;
    }

    loli_module_entry *module = new_module(parser);

    add_path_to_module(module, parser->ims->pending_loadname, path);
    add_data_to_module(module, handle, info_table, call_table);
    return 1;
}

int loli_import_library_data(loli_state *s, const char *path,
        const char **info_table, loli_call_entry_func *call_table)
{
    loli_parse_state *parser = s->gs->parser;

    if (import_check(parser, path))
        return 1;

    loli_module_entry *module = new_module(parser);

    add_path_to_module(module, parser->ims->pending_loadname, path);
    add_data_to_module(module, NULL, info_table, call_table);
    return 1;
}

void loli_module_register(loli_state *s, const char *name,
        const char **info_table, loli_call_entry_func *call_table)
{
    loli_parse_state *parser = s->gs->parser;
    loli_module_entry *module = new_module(parser);

     
    const char *module_path = loli_mb_sprintf(parser->msgbuf, "[%s]", name);

    add_path_to_module(module, name, module_path);
    add_data_to_module(module, NULL, info_table, call_table);
    module->cmp_len = 0;
    module->flags |= MODULE_IS_REGISTERED;
}

static void link_module_to(loli_module_entry *target, loli_module_entry *to_link,
        const char *as_name)
{
    loli_module_link *new_link = loli_malloc(sizeof(*new_link));
    char *link_name;
    if (as_name == NULL)
        link_name = NULL;
    else {
        link_name = loli_malloc((strlen(as_name) + 1) * sizeof(*link_name));
        strcpy(link_name, as_name);
    }

    new_link->module = to_link;
    new_link->next_module = target->module_chain;
    new_link->as_name = link_name;

    target->module_chain = new_link;
}

void loli_import_use_local_dir(loli_state *s, const char *dirname)
{
    loli_import_state *ims = s->gs->parser->ims;

    ims->dirname = dirname;
    ims->is_package_import = 0;
}

void loli_import_use_package_dir(loli_state *s, const char *dirname)
{
    loli_import_state *ims = s->gs->parser->ims;

    ims->dirname = dirname;
    ims->is_package_import = 1;
}

const char *loli_import_current_root_dir(loli_state *s)
{
    loli_parse_state *parser = s->gs->parser;

    const char *current_root = parser->ims->source_module->root_dirname;
    const char *first_root = parser->main_module->root_dirname;
    const char *result = current_root + strlen(first_root);

    return result;
}

void loli_default_import_func(loli_state *s, const char *target)
{
     
    loli_import_use_local_dir(s, "");
    if (loli_import_file(s, target) ||
        loli_import_library(s, target))
        return;

     
    loli_import_use_package_dir(s, "");
    if (loli_import_file(s, target) ||
        loli_import_library(s, target))
        return;
}

static loli_module_entry *load_module(loli_parse_state *parser,
        const char *name)
{
    loli_import_state *ims = parser->ims;

    ims->source_module = parser->symtab->active_module;
    ims->last_import = NULL;

     
    loli_u16_write_1(parser->data_stack, 0);

    parser->config->import_func(parser->vm, name);

    if (parser->ims->last_import == NULL) {
        loli_msgbuf *msgbuf = loli_mb_flush(parser->msgbuf);
        loli_mb_add_fmt(msgbuf, "Cannot import '%s':\n", name);
        
#ifdef _WIN32
       int error;
       if ((error = GetLastError()) != 0)  {
          loli_mb_add_fmt(msgbuf, "    cannot import shared library '%s.%s': %d\n", name, LOLI_LIB_SUFFIX, error);
        }
#else
        char *error;
        if ((error = dlerror()) != NULL)  {
          loli_mb_add_fmt(msgbuf, "    cannot import shared library '%s.%s': %s\n", name, LOLI_LIB_SUFFIX, error);
        }
#endif    
        
        loli_mb_add_fmt(msgbuf, "    no preloaded package '%s'", name);

        loli_buffer_u16 *b = parser->data_stack;
        int i;

        for (i = 0;i < loli_u16_pos(b) - 1;i++) {
            uint16_t check_pos = loli_u16_get(b, i);
            loli_mb_add_fmt(msgbuf, "\n    no file '%s'",
                    loli_sp_get(parser->expr_strings, check_pos));
        }

        loli_raise_syn(parser->raiser, loli_mb_raw(msgbuf));
    }
    else        
        loli_u16_set_pos(parser->data_stack, 0);

    return parser->ims->last_import;
}



static void make_new_function(loli_parse_state *parser, const char *class_name,
        loli_var *var, loli_foreign_func foreign_func)
{
    loli_function_val *f = loli_malloc(sizeof(*f));
    loli_module_entry *m = parser->symtab->active_module;
    loli_proto *proto = loli_emit_new_proto(parser->emit, m->path, class_name,
            var->name);

     
    f->refcount = 1;
    f->foreign_func = foreign_func;
    f->code = NULL;
    f->num_upvalues = 0;
    f->upvalues = NULL;
    f->gc_entry = NULL;
    f->cid_table = m->cid_table;
    f->proto = proto;

    loli_value *v = loli_malloc(sizeof(*v));
    v->flags = V_FUNCTION_FLAG | V_FUNCTION_BASE;
    v->value.function = f;

    loli_vs_push(parser->symtab->literals, v);
}

static void put_keyargs_in_target(loli_parse_state *parser, loli_item *target,
        uint32_t arg_start)
{
    char *source = loli_sp_get(parser->keyarg_strings, arg_start);
    int len = parser->keyarg_current - arg_start + 1;
    char *buffer = loli_malloc(len * sizeof(*buffer));

    memcpy(buffer, source, len);

    if (target->item_kind == ITEM_TYPE_VAR) {
        loli_var *var = (loli_var *)target;
        loli_proto *p = loli_emit_proto_for_var(parser->emit, var);

        p->arg_names = buffer;
    }
    else {
        loli_variant_class *c = (loli_variant_class *)target;

        c->arg_names = buffer;
    }
}

static void hide_block_vars(loli_parse_state *parser)
{
    int count = parser->emit->block->var_count;

    if (count == 0)
        return;

    loli_var *var_iter = parser->symtab->active_module->var_chain;
    loli_var *var_next;

    while (count) {
        var_next = var_iter->next;

        if (var_iter->flags & VAR_IS_READONLY) {
            var_iter->next = parser->symtab->old_function_chain;
            parser->symtab->old_function_chain = var_iter;
            count--;
        }
        else {
             
            loli_free(var_iter->name);
            loli_free(var_iter);
            count--;
        }

        var_iter = var_next;
    }

    parser->symtab->active_module->var_chain = var_iter;
    parser->emit->block->var_count = 0;
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

static loli_var *make_new_var(loli_type *type, const char *name,
        uint16_t line_num)
{
    loli_var *var = loli_malloc(sizeof(*var));

    var->name = loli_malloc((strlen(name) + 1) * sizeof(*var->name));
    var->item_kind = ITEM_TYPE_VAR;
    var->flags = 0;
    strcpy(var->name, name);
    var->line_num = line_num;
    var->shorthash = shorthash_for_name(name);
    var->type = type;
    var->next = NULL;
    var->parent = NULL;

    return var;
}

static loli_var *new_local_var(loli_parse_state *parser, loli_type *type,
        const char *name, uint16_t line_num)
{
    loli_var *var = make_new_var(type, name, line_num);

     
    var->function_depth = parser->emit->function_depth;
    var->reg_spot = parser->emit->function_block->next_reg_spot;
    parser->emit->function_block->next_reg_spot++;
    var->next = parser->symtab->active_module->var_chain;
    parser->symtab->active_module->var_chain = var;
    parser->emit->block->var_count++;

    return var;
}

static loli_var *new_scoped_var(loli_parse_state *parser, loli_type *type,
        const char *name, uint16_t line_num)
{
    loli_var *var = make_new_var(type, name, line_num);

    var->next = parser->symtab->active_module->var_chain;
    parser->symtab->active_module->var_chain = var;
    var->function_depth = parser->emit->function_depth;

     
    if (var->function_depth == 1) {
         
        loli_push_unit(parser->vm);
        var->reg_spot = parser->symtab->next_global_id;
        parser->symtab->next_global_id++;
        var->flags |= VAR_IS_GLOBAL;
    }
    else {
        var->reg_spot = parser->emit->function_block->next_reg_spot;
        parser->emit->function_block->next_reg_spot++;
    }

    parser->emit->block->var_count++;

    return var;
}

static loli_var *new_global_var(loli_parse_state *parser, loli_type *type,
        const char *name)
{
     
    loli_var *var = make_new_var(type, name, 0);

    var->next = parser->symtab->active_module->var_chain;
    parser->symtab->active_module->var_chain = var;
    var->function_depth = 1;
    var->flags |= VAR_IS_GLOBAL;
    var->reg_spot = parser->symtab->next_global_id;
    parser->symtab->next_global_id++;

    return var;
}

static loli_var *new_native_define_var(loli_parse_state *parser,
        loli_class *parent, const char *name, uint16_t line_num)
{
    loli_var *var = make_new_var(NULL, name, line_num);

    var->reg_spot = loli_vs_pos(parser->symtab->literals);
    var->function_depth = 1;
    var->flags |= VAR_IS_READONLY;

    char *class_name;
    if (parent) {
        class_name = parent->name;
        var->parent = parent;
        var->next = (loli_var *)parent->members;
        parent->members = (loli_named_sym *)var;
    }
    else {
        class_name = NULL;
        var->next = parser->symtab->active_module->var_chain;
        parser->symtab->active_module->var_chain = var;
        parser->emit->block->var_count++;
    }

    make_new_function(parser, class_name, var, NULL);

    return var;
}

static void create_main_func(loli_parse_state *parser)
{
    loli_type_maker *tm = parser->emit->tm;

    loli_tm_add(tm, loli_unit_type);
    loli_type *main_type = loli_tm_make_call(tm, 0,
            parser->symtab->function_class, 1);

    loli_var *main_var = new_native_define_var(parser, NULL, "__main__", 0);
    loli_value *v = loli_vs_nth(parser->symtab->literals, 0);
    loli_function_val *f = v->value.function;

    main_var->type = main_type;

     
    parser->vm->call_chain->function = f;
    parser->toplevel_func = f;
    parser->default_call_type = main_type;
}


static loli_type *get_type_raw(loli_parse_state *, int);
static loli_class *resolve_class_name(loli_parse_state *);
static int constant_by_name(const char *);
static loli_prop_entry *get_named_property(loli_parse_state *, int);
static void expression_raw(loli_parse_state *);
static int keyword_by_name(const char *);


static void collect_optarg_for(loli_parse_state *parser, loli_var *var)
{
     
    loli_es_checkpoint_save(parser->expr);

    loli_es_push_local_var(parser->expr, var);
    loli_es_push_binary_op(parser->expr, expr_assign);
    loli_lexer(parser->lex);
    expression_raw(parser);
}

static loli_type *make_optarg_of(loli_parse_state *parser, loli_type *type)
{
    loli_tm_add(parser->tm, type);
    loli_type *t = loli_tm_make(parser->tm, parser->symtab->optarg_class, 1);

     
    t->flags |= TYPE_HAS_OPTARGS;

    return t;
}

static void ensure_valid_type(loli_parse_state *parser, loli_type *type)
{
    if (type->subtype_count != type->cls->generic_count &&
        type->cls->generic_count != -1)
        loli_raise_syn(parser->raiser,
                "Class %s expects %d type(s), but got %d type(s).",
                type->cls->name, type->cls->generic_count,
                type->subtype_count);

     
    if (type->cls == parser->symtab->hash_class) {
        loli_type *check_type = type->subtypes[0];
        if ((check_type->cls->flags & CLS_VALID_HASH_KEY) == 0 &&
            check_type->cls->id != LOLI_ID_GENERIC)
            loli_raise_syn(parser->raiser, "'^T' is not a valid key for Hash.",
                    check_type);
    }
}

static loli_class *get_scoop_class(loli_parse_state *parser, int which)
{
     
    return parser->symtab->old_class_chain;
}

#define F_SCOOP_OK          0x040000
#define F_COLLECT_FUTURE   0x080000
#define F_COLLECT_DEFINE    0x100000
#define F_COLLECT_DYNALOAD (0x200000 | F_SCOOP_OK)
#define F_COLLECT_CLASS     0x400000
#define F_COLLECT_VARIANT   0x800000
#define F_NO_COLLECT        0x00FFFF

static loli_type *get_nameless_arg(loli_parse_state *parser, int *flags)
{
    loli_lex_state *lex = parser->lex;

    if (lex->token == tk_multiply) {
        *flags |= TYPE_HAS_OPTARGS;
        loli_lexer(lex);
    }
    else if (*flags & TYPE_HAS_OPTARGS)
        loli_raise_syn(parser->raiser,
                "Non-optional argument follows optional argument.");

    loli_type *type = get_type_raw(parser, *flags);

     

    if (type->flags & TYPE_HAS_SCOOP)
        *flags |= TYPE_HAS_SCOOP;

    if (lex->token == tk_three_dots) {
        loli_tm_add(parser->tm, type);
        type = loli_tm_make(parser->tm, parser->symtab->list_class, 1);

        loli_lexer(lex);
        if (lex->token != tk_arrow &&
            lex->token != tk_right_parenth &&
            lex->token != tk_equal)
            loli_raise_syn(parser->raiser,
                    "Expected either '=>' or ')' after varargs.");

        *flags |= TYPE_IS_VARARGS;
    }

    if (*flags & TYPE_HAS_OPTARGS)
        type = make_optarg_of(parser, type);

    return type;
}

static loli_type *get_variant_arg(loli_parse_state *parser, int *flags)
{
    loli_type *type = get_nameless_arg(parser, flags);

    if (*flags & TYPE_HAS_OPTARGS)
        loli_raise_syn(parser->raiser,
                "Variant types cannot have default values.");

    return type;
}

static loli_type *get_define_arg(loli_parse_state *parser, int *flags)
{
    loli_lex_state *lex = parser->lex;

    NEED_CURRENT_TOK(tk_word)

    loli_var *var = loli_find_var(parser->symtab, NULL, lex->label);
    if (var != NULL)
        loli_raise_syn(parser->raiser, "%s has already been declared.",
                lex->label);

    var = new_scoped_var(parser, NULL, lex->label, lex->line_num);
    NEED_NEXT_TOK(tk_colon)

    loli_lexer(lex);
    loli_type *type = get_nameless_arg(parser, flags);

    if (*flags & TYPE_HAS_OPTARGS) {
         
        var->type = type->subtypes[0];
        collect_optarg_for(parser, var);
    }
    else
        var->type = type;

    return type;
}

#define PUBLIC_SCOPE 0x10000

static loli_type *get_class_arg(loli_parse_state *parser, int *flags)
{
    loli_lex_state *lex = parser->lex;
    loli_prop_entry *prop = NULL;
    loli_var *var;
    int modifiers = 0;

    NEED_CURRENT_TOK(tk_word)

    if (lex->label[0] == 'p') {
        int keyword = keyword_by_name(lex->label);
        if (keyword == KEY_PRIVATE)
            modifiers = SYM_SCOPE_PRIVATE;
        else if (keyword == KEY_PROTECTED)
            modifiers = SYM_SCOPE_PROTECTED;
        else if (keyword == KEY_PUBLIC)
            modifiers = PUBLIC_SCOPE;
    }
    else if (lex->label[0] == 'v' &&
             strcmp(lex->label, "var") == 0) {
        loli_raise_syn(parser->raiser,
                "Constructor var declaration must start with a scope.");
    }

    if (modifiers) {
        loli_lexer(lex);
        if (lex->token != tk_word ||
            strcmp(lex->label, "var") != 0) {
            loli_raise_syn(parser->raiser,
                    "Expected 'var' after scope was given.");
        }

        NEED_NEXT_TOK(tk_prop_word)
        prop = get_named_property(parser, 0);
         
        prop->flags |= SYM_NOT_INITIALIZED | modifiers;
        var = new_scoped_var(parser, NULL, "", lex->line_num);
    }
    else {
        var = new_scoped_var(parser, NULL, lex->label, lex->line_num);
        loli_lexer(lex);
    }

    NEED_CURRENT_TOK(tk_colon)
    loli_lexer(lex);
    loli_type *type = get_nameless_arg(parser, flags);

    if (*flags & TYPE_HAS_OPTARGS) {
         
        var->type = type->subtypes[0];
        collect_optarg_for(parser, var);
    }
    else
        var->type = type;

    if (prop)
        prop->type = var->type;

    return type;
}

static loli_type *get_type_raw(loli_parse_state *parser, int flags)
{
    loli_lex_state *lex = parser->lex;
    loli_type *result;
    loli_class *cls = NULL;

    if (lex->token == tk_word)
        cls = resolve_class_name(parser);
    else if ((flags & F_SCOOP_OK) && lex->token == tk_scoop)
        cls = get_scoop_class(parser, lex->last_integer);
    else {
        NEED_CURRENT_TOK(tk_word)
    }

    if (cls->item_kind == ITEM_TYPE_VARIANT)
        loli_raise_syn(parser->raiser,
                "Variant types not allowed in a declaration.");

    if (cls->generic_count == 0)
        result = cls->self_type;
    else if (cls->id != LOLI_ID_FUNCTION) {
        NEED_NEXT_TOK(tk_left_bracket)
        int i = 0;
        while (1) {
            loli_lexer(lex);
            loli_tm_add(parser->tm, get_type_raw(parser, flags));
            i++;

            if (lex->token == tk_comma)
                continue;
            else if (lex->token == tk_right_bracket)
                break;
            else
                loli_raise_syn(parser->raiser,
                        "Expected either ',' or ']', not '%s'.",
                        tokname(lex->token));
        }

        result = loli_tm_make(parser->tm, cls, i);
        ensure_valid_type(parser, result);
    }
    else {
        NEED_NEXT_TOK(tk_left_parenth)
        loli_lexer(lex);
        int arg_flags = flags & F_SCOOP_OK;
        int i = 0;
        int result_pos = parser->tm->pos;

        loli_tm_add(parser->tm, loli_unit_type);

        if (lex->token != tk_arrow && lex->token != tk_right_parenth) {
            while (1) {
                loli_tm_add(parser->tm, get_nameless_arg(parser, &arg_flags));
                i++;
                if (lex->token == tk_comma) {
                    loli_lexer(lex);
                    continue;
                }

                break;
            }
        }

        if (lex->token == tk_arrow) {
            loli_lexer(lex);
            loli_tm_insert(parser->tm, result_pos,
                    get_type_raw(parser, flags));
        }

        NEED_CURRENT_TOK(tk_right_parenth)

        result = loli_tm_make_call(parser->tm, arg_flags & F_NO_COLLECT, cls,
                i + 1);
    }

    loli_lexer(lex);
    return result;
}

#define get_type(p) get_type_raw(p, 0)

static loli_type *type_by_name(loli_parse_state *parser, const char *name)
{
    loli_lexer_load(parser->lex, et_copied_string, name);
    loli_lexer(parser->lex);
    loli_type *result = get_type(parser);
    loli_pop_lex_entry(parser->lex);

    return result;
}

static void collect_generics(loli_parse_state *parser)
{
    loli_lex_state *lex = parser->lex;

    if (lex->token == tk_left_bracket) {
        char ch = 'A' + loli_gp_num_in_scope(parser->generics);
        char name[] = {ch, '\0'};

        while (1) {
            NEED_NEXT_TOK(tk_word)
            if (lex->label[0] != ch || lex->label[1] != '\0') {
                if (ch == 'Z' + 1) {
                    loli_raise_syn(parser->raiser, "Too many generics.");
                }
                else {
                    loli_raise_syn(parser->raiser,
                            "Invalid generic name (wanted %s, got %s).",
                            name, lex->label);
                }
            }

            loli_gp_push(parser->generics, name, ch - 'A');
            loli_lexer(lex);

             
            ch++;

            if (lex->token == tk_right_bracket) {
                loli_lexer(lex);
                break;
            }
            else if (lex->token != tk_comma)
                loli_raise_syn(parser->raiser,
                        "Expected either ',' or ']', not '%s'.",
                        tokname(lex->token));

            name[0] = ch;
        }
        int seen = ch - 'A';
        loli_ts_generics_seen(parser->emit->ts, seen);
    }
}

static loli_type *build_self_type(loli_parse_state *parser, loli_class *cls)
{
    int generics_used = loli_gp_num_in_scope(parser->generics);
    loli_type *result;
    if (generics_used) {
        char name[] = {'A', '\0'};
        while (generics_used) {
            loli_class *lookup_cls = loli_find_class(parser->symtab, NULL, name);
            loli_tm_add(parser->tm, lookup_cls->self_type);
            name[0]++;
            generics_used--;
        }

        result = loli_tm_make(parser->tm, cls, (name[0] - 'A'));
        cls->self_type = result;
    }
    else
        result = cls->self_type;

    return result;
}

typedef loli_type *(*collect_fn)(loli_parse_state *, int *);

static void error_future_decl_type(loli_parse_state *parser, loli_var *var,
        loli_type *got)
{
    loli_raise_syn(parser->raiser,
            "Declaration does not match prior future declaration at line %d.\n"
            "Expected type: ^T\n"
            "But got type: ^T", var->line_num, var->type, got);
}

static void collect_call_args(loli_parse_state *parser, void *target,
        int arg_flags)
{
    loli_lex_state *lex = parser->lex;
     
    int result_pos = parser->tm->pos - 1;
    int i = 0;
    int last_keyarg_pos = 0;
    uint32_t keyarg_start = parser->keyarg_current;
    collect_fn arg_collect = NULL;

    if ((arg_flags & F_COLLECT_DEFINE)) {
        if (parser->emit->block->self) {
            i++;
            result_pos--;
        }

        arg_collect = get_define_arg;
    }
    else if (arg_flags & F_COLLECT_DYNALOAD)
        arg_collect = get_nameless_arg;
    else if (arg_flags & F_COLLECT_CLASS)
        arg_collect = get_class_arg;
    else if (arg_flags & F_COLLECT_VARIANT)
        arg_collect = get_variant_arg;
    else if (arg_flags & F_COLLECT_FUTURE) {
        if (parser->emit->block->self) {
            i++;
            result_pos--;
        }

        arg_collect = get_nameless_arg;
    }

    if (lex->token == tk_left_parenth) {
        loli_lexer(lex);

        if (lex->token == tk_right_parenth)
            loli_raise_syn(parser->raiser,
                    "Empty () found while reading input arguments. Omit instead.");

        while (1) {
            if (lex->token == tk_keyword_arg) {
                while (i != last_keyarg_pos) {
                    last_keyarg_pos++;
                    loli_sp_insert(parser->keyarg_strings, " ",
                            &parser->keyarg_current);
                }

                loli_sp_insert(parser->keyarg_strings, lex->label,
                        &parser->keyarg_current);

                last_keyarg_pos++;
                loli_lexer(lex);
            }

            loli_tm_add(parser->tm, arg_collect(parser, &arg_flags));
            i++;
            if (lex->token == tk_comma) {
                loli_lexer(lex);
                continue;
            }
            else if (lex->token == tk_right_parenth) {
                loli_lexer(lex);
                break;
            }
            else
                loli_raise_syn(parser->raiser,
                        "Expected either ',' or ')', not '%s'.",
                        tokname(lex->token));
        }
    }

    if (lex->token == tk_colon &&
        (arg_flags & (F_COLLECT_VARIANT | F_COLLECT_CLASS)) == 0) {
        loli_lexer(lex);
        if (arg_flags & F_COLLECT_DEFINE &&
            strcmp(lex->label, "self") == 0) {
            loli_block *block = parser->emit->block->prev;
            if (block == NULL || block->block_type != block_class)
                loli_raise_syn(parser->raiser,
                        "'self' return type only allowed on class methods.");

            loli_var *v = (loli_var *)target;

            if (v->flags & VAR_IS_STATIC)
                loli_raise_syn(parser->raiser,
                        "'self' return type not allowed on a static method.");

            loli_tm_insert(parser->tm, result_pos, loli_self_class->self_type);
            loli_lexer(lex);
        }
        else {
            loli_type *result_type = get_type_raw(parser, arg_flags);
            if (result_type == loli_unit_type) {
                loli_raise_syn(parser->raiser,
                        "Unit return type is automatic. Omit instead.");
            }

             
            loli_tm_insert(parser->tm, result_pos, result_type);
        }
    }

    if (last_keyarg_pos) {
        loli_sym *sym = (loli_sym *)target;

         
        if (sym->flags & VAR_IS_FUTURE) {
            loli_raise_syn(parser->raiser,
                    "Future declarations not allowed to have keyword arguments.");
        }

        while (last_keyarg_pos != i) {
            last_keyarg_pos++;
            loli_sp_insert(parser->keyarg_strings, " ",
                    &parser->keyarg_current);
        }

        loli_sp_insert(parser->keyarg_strings, "\t",
                &parser->keyarg_current);

        put_keyargs_in_target(parser, target, keyarg_start);
        parser->keyarg_current = keyarg_start;
    }

    loli_type *t = loli_tm_make_call(parser->tm, arg_flags & F_NO_COLLECT,
            parser->symtab->function_class, i + 1);

    if ((arg_flags & F_COLLECT_VARIANT) == 0) {
        loli_var *var = (loli_var *)target;
        if (var->type && var->type != t)
            error_future_decl_type(parser, var, t);

        var->type = t;
    }
    else {
        loli_variant_class *cls = (loli_variant_class *)target;
        cls->build_type = t;
    }
}


static void parse_class_body(loli_parse_state *, loli_class *);
static loli_class *find_run_class_dynaload(loli_parse_state *,
        loli_module_entry *, const char *);
static void parse_variant_header(loli_parse_state *, loli_variant_class *);
static loli_item *try_toplevel_dynaload(loli_parse_state *, loli_module_entry *,
        const char *);

#define DYNA_NAME_OFFSET 2


static void update_cid_table(loli_parse_state *parser, loli_module_entry *m)
{
    const char *cid_entry = m->info_table[0] + 1;
    int counter = 0;
    int stop = cid_entry[-1];
    uint16_t *cid_table = m->cid_table;
    loli_symtab *symtab = parser->symtab;
    loli_module_entry *builtin = parser->module_start;

    while (counter < stop) {
        if (cid_table[counter] == 0) {
            loli_class *cls = loli_find_class(symtab, m, cid_entry);
            if (cls == NULL)
                cls = loli_find_class(symtab, builtin, cid_entry);

            if (cls)
                cid_table[counter] = cls->id;
        }
        cid_entry += strlen(cid_entry) + 1;
        counter++;
    }
}

static void update_all_cid_tables(loli_parse_state *parser)
{
    loli_module_entry *entry_iter = parser->module_start;
    while (entry_iter) {
        if (entry_iter->cid_table)
            update_cid_table(parser, entry_iter);

        entry_iter = entry_iter->root_next;
    }
}

static loli_module_entry *resolve_module(loli_parse_state *parser)
{
    loli_module_entry *result = NULL;
    loli_symtab *symtab = parser->symtab;
    loli_lex_state *lex = parser->lex;
    loli_module_entry *search_entry = loli_find_module(symtab, NULL,
            lex->label);

    while (search_entry) {
        result = search_entry;
        NEED_NEXT_TOK(tk_dot)
        NEED_NEXT_TOK(tk_word)
        search_entry = loli_find_module(symtab, result, lex->label);
    }

    return result;
}

static loli_class *resolve_class_name(loli_parse_state *parser)
{
    loli_symtab *symtab = parser->symtab;
    loli_lex_state *lex = parser->lex;

    NEED_CURRENT_TOK(tk_word)

    loli_module_entry *search_module = resolve_module(parser);
    loli_class *result = loli_find_class(symtab, search_module, lex->label);
    if (result == NULL) {
        if (search_module == NULL)
            search_module = symtab->builtin_module;

        if (search_module->info_table)
            result = find_run_class_dynaload(parser, search_module, lex->label);

        if (result == NULL && symtab->active_module->info_table)
            result = find_run_class_dynaload(parser, symtab->active_module,
                    lex->label);

         
        if (result == NULL && search_module == symtab->builtin_module) {
            if (strcmp(lex->label, "Unit") == 0)
                result = loli_unit_type->cls;
        }

        if (result == NULL)
            loli_raise_syn(parser->raiser, "Class '%s' does not exist.",
                    lex->label);
    }

    return result;
}

static void dynaload_function(loli_parse_state *parser, loli_module_entry *m,
        loli_var *var, int dyna_index)
{
    loli_lex_state *lex = parser->lex;

    const char *entry = m->info_table[dyna_index];
    const char *name = entry + DYNA_NAME_OFFSET;
    const char *body = name + strlen(name) + 1;
    uint16_t save_generic_start = loli_gp_save_and_hide(parser->generics);

    loli_lexer_load(lex, et_shallow_string, body);
    loli_lexer(lex);
    collect_generics(parser);
    loli_tm_add(parser->tm, loli_unit_type);
    collect_call_args(parser, var, F_COLLECT_DYNALOAD);
    loli_gp_restore_and_unhide(parser->generics, save_generic_start);
    loli_pop_lex_entry(lex);
}

static loli_var *new_foreign_define_var(loli_parse_state *parser,
        loli_module_entry *m, loli_class *parent, int dyna_index)
{
    const char *name = m->info_table[dyna_index] + DYNA_NAME_OFFSET;
    loli_module_entry *saved_active = parser->symtab->active_module;
    loli_var *var = make_new_var(NULL, name, 0);

    parser->symtab->active_module = m;

    var->reg_spot = loli_vs_pos(parser->symtab->literals);
    var->function_depth = 1;
    var->flags |= VAR_IS_READONLY | VAR_IS_FOREIGN_FUNC;

    if (parent) {
        var->next = (loli_var *)parent->members;
        parent->members = (loli_named_sym *)var;
        var->parent = parent;
    }
    else {
        var->next = m->var_chain;
        m->var_chain = var;
    }

    char *class_name;
    if (parent)
        class_name = parent->name;
    else
        class_name = NULL;

    loli_foreign_func func = m->call_table[dyna_index];

    make_new_function(parser, class_name, var, func);
    dynaload_function(parser, m, var, dyna_index);

    loli_value *v = loli_vs_nth(parser->symtab->literals, var->reg_spot);
    loli_function_val *f = v->value.function;

    f->reg_count = var->type->subtype_count;
    parser->symtab->active_module = saved_active;

    return var;
}

static loli_class *dynaload_enum(loli_parse_state *parser, loli_module_entry *m,
        int dyna_index)
{
    loli_lex_state *lex = parser->lex;
    const char **table = m->info_table;
    const char *entry = table[dyna_index];
    const char *name = entry + DYNA_NAME_OFFSET;
    int entry_index = dyna_index;
    uint16_t save_next_class_id;

     
    if (m == parser->module_start) {
        save_next_class_id = parser->symtab->next_class_id;

        name = table[dyna_index] + DYNA_NAME_OFFSET;
        if (name[0] == 'O')
            parser->symtab->next_class_id = LOLI_ID_OPTION;
        else
            parser->symtab->next_class_id = LOLI_ID_RESULT;
    }
    else
        save_next_class_id = 0;

    uint16_t save_generics = loli_gp_save_and_hide(parser->generics);
    loli_class *enum_cls = loli_new_enum_class(parser->symtab, name);
    const char *body = name + strlen(name) + 1;

    loli_lexer_load(lex, et_shallow_string, body);
    loli_lexer(lex);
    collect_generics(parser);
    loli_pop_lex_entry(lex);

    enum_cls->generic_count = loli_gp_num_in_scope(parser->generics);
    loli_type *save_self_type = parser->class_self_type;
    parser->class_self_type = build_self_type(parser, enum_cls);

     
    if (m->info_table[entry_index + 1 + entry[1]][0] != 'V')
        enum_cls->flags |= CLS_ENUM_IS_SCOPED;

    enum_cls->dyna_start = dyna_index + 1;

    int variant_count = 0;

    do {
        entry_index++;
        entry = table[entry_index];
    } while (entry[0] != 'V');

    while (entry[0] == 'V') {
        name = entry + DYNA_NAME_OFFSET;

        loli_variant_class *variant_cls = loli_new_variant_class(parser->symtab,
                enum_cls, name);

        body = name + strlen(name) + 1;
        loli_lexer_load(lex, et_shallow_string, body);
        loli_lexer(lex);

        if (lex->token == tk_left_parenth)
            parse_variant_header(parser, variant_cls);

        entry_index++;
        variant_count++;
        entry = table[entry_index];
        loli_pop_lex_entry(lex);
    }

    enum_cls->variant_size = variant_count;
    loli_fix_enum_variant_ids(parser->symtab, enum_cls);
    loli_gp_restore_and_unhide(parser->generics, save_generics);

    if (save_next_class_id)
        parser->symtab->next_class_id = save_next_class_id;

    parser->class_self_type = save_self_type;

    return enum_cls;
}

static loli_class *dynaload_variant(loli_parse_state *parser,
        loli_module_entry *m, int dyna_index)
{
    int enum_pos = dyna_index - 1;
    const char **table = m->info_table;
    const char *entry;

    while (1) {
        entry = table[enum_pos];

        if (entry[0] == 'E')
            break;

        enum_pos--;
    }

    dynaload_enum(parser, m, enum_pos);
    entry = table[dyna_index];
    return loli_find_class(parser->symtab, m, entry + DYNA_NAME_OFFSET);
}

static loli_class *dynaload_class(loli_parse_state *parser,
        loli_module_entry *m, int dyna_index)
{
    const char *entry = m->info_table[dyna_index];
    loli_class *cls = loli_new_class(parser->symtab, entry + 2);

    cls->flags |= CLS_IS_BUILTIN;
    cls->dyna_start = dyna_index;

    return cls;
}

loli_item *try_method_dynaload(loli_parse_state *parser, loli_class *cls,
        const char *name)
{
    int index = cls->dyna_start;
    loli_module_entry *m = cls->module;
    const char **table = m->info_table;
    const char *entry = table[index];

    do {
        if (strcmp(name, entry + 2) == 0)
            break;
        index++;
        entry = table[index];
    } while (entry[0] == 'm');

    loli_item *result;

    if (entry[0] == 'm') {
        loli_var *dyna_var = new_foreign_define_var(parser, cls->module, cls,
                index);
        result = (loli_item *)dyna_var;
    }
    else
        result = NULL;

    return result;
}

static loli_class *dynaload_native(loli_parse_state *parser,
        loli_module_entry *m, int dyna_index)
{
    const char **table = m->info_table;
    const char *entry = m->info_table[dyna_index];
    const char *name = entry + DYNA_NAME_OFFSET;

    const char *body = name + strlen(name) + 1;
    int entry_index = dyna_index;
    loli_lex_state *lex = parser->lex;

    loli_lexer_load(lex, et_shallow_string, body);
    loli_lexer(lex);

    loli_class *cls = loli_new_class(parser->symtab, name);
    uint16_t save_generic_start = loli_gp_save_and_hide(parser->generics);

    collect_generics(parser);
    cls->generic_count = loli_gp_num_in_scope(parser->generics);

    if (lex->token == tk_lt) {
        loli_lexer(lex);
        loli_class *parent = loli_find_class(parser->symtab, m, lex->label);

        if (parent == NULL)
            parent = (loli_class *)try_toplevel_dynaload(parser, m, lex->label);

        cls->parent = parent;
        cls->prop_count = parent->prop_count;
    }

    loli_pop_lex_entry(parser->lex);

    cls->dyna_start = dyna_index + 1;
    if (m == parser->module_start) {
        parser->symtab->next_class_id--;

        if (strcmp(cls->name, "DivisionByZeroError") == 0)
            cls->id = LOLI_ID_DBZERROR;
        else if (strcmp(cls->name, "Exception") == 0)
            cls->id = LOLI_ID_EXCEPTION;
        else if (strcmp(cls->name, "IndexError") == 0)
            cls->id = LOLI_ID_INDEXERROR;
        else if (strcmp(cls->name, "IOError") == 0)
            cls->id = LOLI_ID_IOERROR;
        else if (strcmp(cls->name, "KeyError") == 0)
            cls->id = LOLI_ID_KEYERROR;
        else if (strcmp(cls->name, "RuntimeError") == 0)
            cls->id = LOLI_ID_RUNTIMEERROR;
        else if (strcmp(cls->name, "ValueError") == 0)
            cls->id = LOLI_ID_VALUEERROR;
        else
             
            cls->id = 12345;
    }

    do {
        entry_index++;
        entry = table[entry_index];
    } while (entry[0] == 'm');

    do {
        int flags;
        char ch = entry[0];

        if (ch == '1')
            flags = SYM_SCOPE_PRIVATE;
        else if (ch == '2')
            flags = SYM_SCOPE_PROTECTED;
        else if (ch == '3')
            flags = 0;
        else
            break;

        const char *prop_name = entry + DYNA_NAME_OFFSET;
        const char *prop_body = prop_name + strlen(prop_name) + 1;

        loli_lexer_load(lex, et_shallow_string, prop_body);
        loli_lexer(lex);
        loli_add_class_property(parser->symtab, cls, get_type(parser),
                prop_name, flags);
        loli_pop_lex_entry(lex);

        entry_index++;
        entry = table[entry_index];
    } while (1);

     
    loli_gp_restore_and_unhide(parser->generics, save_generic_start);

     
    try_method_dynaload(parser, cls, "<new>");

    return cls;
}

static loli_item *run_dynaload(loli_parse_state *parser, loli_module_entry *m,
        int dyna_pos)
{
    loli_symtab *symtab = parser->symtab;
    loli_item *result;

    char letter = m->info_table[dyna_pos][0];
    loli_module_entry *saved_active = parser->symtab->active_module;
    symtab->active_module = m;

    if (letter == 'R') {
        const char *entry = m->info_table[dyna_pos];
         
        const char *name = entry + DYNA_NAME_OFFSET;
        loli_type *var_type = type_by_name(parser, name + strlen(name) + 1);
        loli_var *new_var = new_global_var(parser, var_type, name);

         
        update_cid_table(parser, m);

         
        parser->toplevel_func->cid_table = m->cid_table;

        loli_foreign_func var_loader = m->call_table[dyna_pos];

         
        var_loader(parser->vm);

        result = (loli_item *)new_var;
    }
    else if (letter == 'F') {
        loli_var *dyna_var = new_foreign_define_var(parser, m, NULL, dyna_pos);
        result = (loli_item *)dyna_var;
    }
    else if (letter == 'C') {
        loli_class *new_cls = dynaload_class(parser, m, dyna_pos);
        result = (loli_item *)new_cls;
    }
    else if (letter == 'V') {
        loli_class *new_cls = dynaload_variant(parser, m, dyna_pos);
        result = (loli_item *)new_cls;
    }
    else if (letter == 'E') {
        loli_class *new_cls = dynaload_enum(parser, m, dyna_pos);
        result = (loli_item *)new_cls;
    }
    else if (letter == 'N') {
        loli_class *new_cls = dynaload_native(parser, m, dyna_pos);
        result = (loli_item *)new_cls;
    }
    else
        result = NULL;

    symtab->active_module = saved_active;
    return result;
}

static loli_item *try_toplevel_dynaload(loli_parse_state *parser,
        loli_module_entry *m, const char *name)
{
    int i = 1;
    const char **table = m->info_table;
    const char *entry = table[i];
    loli_item *result = NULL;

    do {
        if (strcmp(entry + DYNA_NAME_OFFSET, name) == 0) {
            result = run_dynaload(parser, m, i);
            break;
        }

        i += (unsigned char)entry[1] + 1;
        entry = table[i];
    } while (entry[0] != 'Z');

    return result;
}

loli_class *loli_dynaload_exception(loli_parse_state *parser, const char *name)
{
    loli_module_entry *m = parser->module_start;
    return (loli_class *)try_toplevel_dynaload(parser, m, name);
}

static loli_class *find_run_class_dynaload(loli_parse_state *parser,
        loli_module_entry *m, const char *name)
{
    loli_item *result = try_toplevel_dynaload(parser, m, name);
    if (result && result->item_kind != ITEM_TYPE_VAR)
        return (loli_class *)result;
    else
        return NULL;
}

loli_item *loli_find_or_dl_member(loli_parse_state *parser, loli_class *cls,
        const char *name, loli_class *scope)
{
    loli_named_sym *member = loli_find_member(cls, name, scope);
    if (member)
        return (loli_item *)member;

    if (cls->dyna_start)
        return try_method_dynaload(parser, cls, name);

    return NULL;
}

static int constant_by_name(const char *name)
{
    int i;
    uint64_t shorthash = shorthash_for_name(name);

    for (i = 0;i <= CONST_LAST_ID;i++) {
        if (constants[i].shorthash == shorthash &&
            strcmp(constants[i].name, name) == 0)
            return i;
        else if (constants[i].shorthash > shorthash)
            break;
    }

    return -1;
}

static int keyword_by_name(const char *name)
{
    int i;
    uint64_t shorthash = shorthash_for_name(name);

    for (i = 0;i <= KEY_LAST_ID;i++) {
        if (keywords[i].shorthash == shorthash &&
            strcmp(keywords[i].name, name) == 0)
            return i;
        else if (keywords[i].shorthash > shorthash)
            break;
    }

    return -1;
}





#define ST_DEMAND_VALUE         1
#define ST_WANT_OPERATOR        2
#define ST_WANT_VALUE           3
#define ST_DONE                 4
#define ST_BAD_TOKEN            5
#define ST_FUTURE              0x8

static void expression_class_access(loli_parse_state *parser, loli_class *cls,
        int *state)
{
    loli_lex_state *lex = parser->lex;
    loli_lexer(lex);
    if (lex->token != tk_dot) {
        if (cls->flags & CLS_IS_ENUM)
            loli_raise_syn(parser->raiser,
                    "Cannot implicitly use the constructor of an enum.");

         
        loli_item *target = loli_find_or_dl_member(parser, cls, "<new>", cls);
        if (target == NULL)
            loli_raise_syn(parser->raiser,
                    "Class %s does not have a constructor.", cls->name);

         
        if (target->flags & SYM_NOT_INITIALIZED)
            loli_raise_syn(parser->raiser,
                    "Constructor for class %s is not initialized.", cls->name);

        loli_es_push_static_func(parser->expr, (loli_var *)target);
        *state = ST_FUTURE | ST_WANT_OPERATOR;
        return;
    }

    *state = ST_WANT_OPERATOR;

    NEED_NEXT_TOK(tk_word)
     
    loli_item *item = loli_find_or_dl_member(parser, cls, lex->label, cls);

    if (item && item->item_kind == ITEM_TYPE_VAR) {
        loli_es_push_static_func(parser->expr, (loli_var *)item);
        return;
    }

     
    if (cls->flags & CLS_IS_ENUM) {
        loli_variant_class *variant = loli_find_variant(cls, lex->label);
        if (variant) {
            loli_es_push_variant(parser->expr, variant);
            return;
        }
    }

    loli_raise_syn(parser->raiser, "%s.%s does not exist.", cls->name,
            lex->label);
}

static void push_literal(loli_parse_state *parser, loli_literal *lit)
{
    loli_class *literal_cls;
    int base = FLAGS_TO_BASE(lit);

    if (base == V_INTEGER_BASE)
        literal_cls = parser->symtab->integer_class;
    else if (base == V_DOUBLE_BASE)
        literal_cls = parser->symtab->double_class;
    else if (base == V_STRING_BASE)
        literal_cls = parser->symtab->string_class;
    else if (base == V_BYTESTRING_BASE)
        literal_cls = parser->symtab->bytestring_class;
    else if (base == V_UNIT_BASE)
        literal_cls = loli_unit_type->cls;
    else
         
        literal_cls = loli_question_type->cls;

    loli_es_push_literal(parser->expr, literal_cls->self_type, lit->reg_spot);
}

static void push_constant(loli_parse_state *parser, int key_id)
{
    loli_expr_state *es = parser->expr;
    loli_symtab *symtab = parser->symtab;
    loli_literal *lit;

     
    if (key_id == CONST__LINE__) {
        int num = parser->lex->line_num;

        if ((int16_t)num <= INT16_MAX)
            loli_es_push_integer(es, (int16_t)num);
        else {
            lit = loli_get_integer_literal(symtab, parser->lex->line_num);
            push_literal(parser, lit);
        }
    }
    else if (key_id == CONST__FILE__) {
        lit = loli_get_string_literal(symtab, parser->symtab->active_module->path);
        push_literal(parser, lit);
    }
    else if (key_id == CONST__FUNCTION__) {
        lit = loli_get_string_literal(symtab,
                parser->emit->function_block->function_var->name);
        push_literal(parser, lit);
    }
    else if (key_id == CONST_TRUE)
        loli_es_push_boolean(es, 1);
    else if (key_id == CONST_FALSE)
        loli_es_push_boolean(es, 0);
    else if (key_id == CONST_SELF)
        loli_es_push_self(es);
    else if (key_id == CONST_UNIT) {
        lit = loli_get_unit_literal(symtab);
        push_literal(parser, lit);
    }
}

static void dispatch_word_as_class(loli_parse_state *parser, loli_class *cls,
        int *state)
{
    if (cls->item_kind == ITEM_TYPE_VARIANT) {
        loli_es_push_variant(parser->expr, (loli_variant_class *)cls);
        *state = ST_WANT_OPERATOR;
    }
    else
        expression_class_access(parser, cls, state);
}

static void dispatch_word_as_var(loli_parse_state *parser, loli_var *var,
        int *state)
{
    if (var->flags & SYM_NOT_INITIALIZED)
        loli_raise_syn(parser->raiser,
                "Attempt to use uninitialized value '%s'.",
                var->name);

     
    else if (var->flags & VAR_IS_READONLY)
        loli_es_push_defined_func(parser->expr, var);
    else if (var->flags & VAR_IS_GLOBAL)
        loli_es_push_global_var(parser->expr, var);
    else if (var->function_depth == parser->emit->function_depth)
        loli_es_push_local_var(parser->expr, var);
    else
        loli_es_push_upvalue(parser->expr, var);

    *state = ST_WANT_OPERATOR;
}

static void dispatch_dynaload(loli_parse_state *parser, loli_item *dl_item,
        int *state)
{
    loli_expr_state *es = parser->expr;

    if (dl_item->item_kind == ITEM_TYPE_VAR) {
        loli_var *v = (loli_var *)dl_item;
        if (v->flags & VAR_IS_READONLY)
            loli_es_push_defined_func(es, v);
        else
            loli_es_push_global_var(es, v);

        *state = ST_WANT_OPERATOR;
    }
    else
        dispatch_word_as_class(parser, (loli_class *)dl_item, state);
}

static void expression_word(loli_parse_state *parser, int *state)
{
    loli_symtab *symtab = parser->symtab;
    loli_lex_state *lex = parser->lex;
    loli_module_entry *search_module = resolve_module(parser);
    loli_module_entry *original_module = search_module;

    loli_var *var = loli_find_var(symtab, search_module, lex->label);
    if (var) {
        dispatch_word_as_var(parser, var, state);
        return;
    }

    if (search_module == NULL) {
        int const_id = constant_by_name(lex->label);
        if (const_id != -1) {
            if (const_id == CONST_SELF && parser->class_self_type == NULL)
                loli_raise_syn(parser->raiser,
                        "'self' must be used within a class.");

            push_constant(parser, const_id);
            *state = ST_WANT_OPERATOR;
            return;
        }
    }

    loli_class *cls = loli_find_class(parser->symtab, search_module, lex->label);

    if (cls) {
        dispatch_word_as_class(parser, cls, state);
        return;
    }

    if (search_module == NULL && parser->class_self_type) {
        var = loli_find_method(parser->class_self_type->cls, lex->label);

        if (var) {
            loli_es_push_method(parser->expr, var);
            *state = ST_WANT_OPERATOR;
            return;
        }
    }

    if (search_module == NULL)
        search_module = symtab->builtin_module;

    if (search_module->info_table) {
        loli_item *dl_result = try_toplevel_dynaload(parser,
                search_module, lex->label);
        if (dl_result) {
            dispatch_dynaload(parser, dl_result, state);
            return;
        }
    }

    if (original_module == NULL && parser->class_self_type) {
        loli_class *cls = loli_find_class_of_member(
                parser->class_self_type->cls, lex->label);

        if (cls)
            loli_raise_syn(parser->raiser,
                    "%s is a private member of class %s, and not visible here.",
                    lex->label, cls->name);
    }

    loli_raise_syn(parser->raiser, "%s has not been declared.", lex->label);
}

static void expression_property(loli_parse_state *parser, int *state)
{
    if (parser->class_self_type == NULL)
        loli_raise_syn(parser->raiser,
                "Properties cannot be used outside of a class constructor.");

    char *name = parser->lex->label;
    loli_class *current_class = parser->class_self_type->cls;

    loli_prop_entry *prop = loli_find_property(current_class, name);
    if (prop == NULL) {
        const char *extra = "";
        if (parser->emit->block->block_type == block_class)
            extra = " ('var' keyword missing?)";

        loli_raise_syn(parser->raiser, "Property %s is not in class %s.%s",
                name, current_class->name, extra);
    }

    loli_es_push_property(parser->expr, prop);
    *state = ST_WANT_OPERATOR;
}

static void check_valid_close_tok(loli_parse_state *parser)
{
    loli_token token = parser->lex->token;
    loli_ast *ast = loli_es_get_saved_tree(parser->expr);
    loli_tree_type tt = ast->tree_type;
    loli_token expect;

    if (tt == tree_call || tt == tree_parenth || tt == tree_typecast ||
        tt == tree_named_call)
        expect = tk_right_parenth;
    else if (tt == tree_tuple)
        expect = tk_tuple_close;
    else
        expect = tk_right_bracket;

    if (token != expect)
        loli_raise_syn(parser->raiser, "Expected closing token '%s', not '%s'.",
                tokname(expect), tokname(token));
}

static int maybe_digit_fixup(loli_parse_state *parser)
{
    int fixed = 0;
    int is_positive = parser->lex->last_integer >= 0;

    if (loli_lexer_digit_rescan(parser->lex)) {
        if (is_positive)
            loli_es_push_binary_op(parser->expr, expr_plus);
        else
            loli_es_push_binary_op(parser->expr, expr_minus);

        fixed = 1;
    }

    return fixed;
}

static void expression_literal(loli_parse_state *parser, int *state)
{
    loli_lex_state *lex = parser->lex;
    loli_token token = parser->lex->token;

    if (*state == ST_WANT_OPERATOR) {
        if ((token == tk_integer || token == tk_double) &&
            maybe_digit_fixup(parser))
            goto integer_case;
        else if (parser->expr->save_depth == 0)
            *state = ST_DONE;
        else
            *state = ST_BAD_TOKEN;
    }
    else if (lex->token == tk_integer) {
integer_case: ;
        if (lex->last_integer <= INT16_MAX &&
            lex->last_integer >= INT16_MIN)
            loli_es_push_integer(parser->expr, (int16_t)
                    lex->last_integer);
        else {
            loli_literal *lit = loli_get_integer_literal(parser->symtab,
                    lex->last_integer);
            push_literal(parser, lit);
        }

        *state = ST_WANT_OPERATOR;
    }
    else if (lex->token == tk_byte) {
        loli_es_push_byte(parser->expr, (uint8_t) lex->last_integer);
        *state = ST_WANT_OPERATOR;
    }
    else {
        push_literal(parser, lex->last_literal);
        *state = ST_WANT_OPERATOR;
    }
}

static void expression_comma_arrow(loli_parse_state *parser, int *state)
{
    loli_lex_state *lex = parser->lex;

    if (parser->expr->active == NULL)
        loli_raise_syn(parser->raiser, "Expected a value, not ','.");

    loli_ast *last_tree = loli_es_get_saved_tree(parser->expr);
    if (last_tree == NULL) {
        *state = ST_BAD_TOKEN;
        return;
    }

    if (lex->token == tk_comma) {
        if (last_tree->tree_type == tree_hash &&
            (last_tree->args_collected & 0x1) == 0)
            loli_raise_syn(parser->raiser,
                    "Expected a key => value pair before ','.");
        if (last_tree->tree_type == tree_subscript)
            loli_raise_syn(parser->raiser,
                    "Subscripts cannot contain ','.");
    }
    else if (lex->token == tk_arrow) {
        if (last_tree->tree_type == tree_list) {
            if (last_tree->args_collected == 0)
                last_tree->tree_type = tree_hash;
            else
                loli_raise_syn(parser->raiser, "Unexpected token '%s'.",
                        tokname(tk_arrow));
        }
        else if (last_tree->tree_type != tree_hash ||
                 (last_tree->args_collected & 0x1) == 1)
                loli_raise_syn(parser->raiser, "Unexpected token '%s'.",
                        tokname(tk_arrow));
    }

    loli_es_collect_arg(parser->expr);
    *state = ST_DEMAND_VALUE;
}

static void expression_unary(loli_parse_state *parser, int *state)
{
    if (*state == ST_WANT_OPERATOR)
        *state = ST_BAD_TOKEN;
    else {
        loli_token token = parser->lex->token;
        if (token == tk_minus)
            loli_es_push_unary_op(parser->expr, expr_unary_minus);
        else if (token == tk_not)
            loli_es_push_unary_op(parser->expr, expr_unary_not);
        else if (token == tk_tilde)
            loli_es_push_unary_op(parser->expr, expr_unary_bitwise_not);

        *state = ST_DEMAND_VALUE;
    }
}

static void expression_dot(loli_parse_state *parser, int *state)
{
    loli_lex_state *lex = parser->lex;
    loli_lexer(lex);
    if (lex->token == tk_word) {
        loli_expr_state *es = parser->expr;
        int spot = es->pile_current;
        loli_sp_insert(parser->expr_strings, lex->label, &es->pile_current);
        loli_es_push_text(es, tree_oo_access, 0, spot);
    }
    else if (lex->token == tk_typecast_parenth) {
        loli_lexer(lex);

        loli_type *cast_type = get_type(parser);

        loli_es_enter_typecast(parser->expr, cast_type);
        loli_es_leave_tree(parser->expr);
    }
    else
        loli_raise_syn(parser->raiser,
                "Expected either '%s' or '%s', not '%s'.",
                tokname(tk_word), tokname(tk_typecast_parenth),
                tokname(lex->token));

    *state = ST_WANT_OPERATOR;
}

static void expression_named_arg(loli_parse_state *parser, int *state)
{
    loli_expr_state *es = parser->expr;

    if (es->root) {
        *state = ST_BAD_TOKEN;
        return;
    }

    loli_ast *last_tree = loli_es_get_saved_tree(parser->expr);
    if (last_tree == NULL) {
        *state = ST_BAD_TOKEN;
        return;
    }

    if (last_tree->tree_type != tree_call &&
        last_tree->tree_type != tree_named_call) {
        *state = ST_BAD_TOKEN;
        return;
    }

    last_tree->tree_type = tree_named_call;

    int spot = es->pile_current;
    loli_sp_insert(parser->expr_strings, parser->lex->label, &es->pile_current);
    loli_es_push_text(es, tree_oo_access, 0, spot);
    loli_es_push_binary_op(es, expr_named_arg);
    *state = ST_DEMAND_VALUE;
}

static void expression_raw(loli_parse_state *parser)
{
    loli_lex_state *lex = parser->lex;
    int state = ST_DEMAND_VALUE;

    while (1) {
        int expr_op = parser_tok_table[lex->token].expr_op;
        if (lex->token == tk_word) {
            if (state == ST_WANT_OPERATOR)
                if (parser->expr->save_depth == 0)
                    state = ST_DONE;
                else
                    state = ST_BAD_TOKEN;
            else
                expression_word(parser, &state);
        }
        else if (expr_op != -1) {
            if (state == ST_WANT_OPERATOR) {
                loli_es_push_binary_op(parser->expr, (loli_expr_op)expr_op);
                state = ST_DEMAND_VALUE;
            }
            else if (lex->token == tk_minus)
                expression_unary(parser, &state);
            else
                state = ST_BAD_TOKEN;
        }
        else if (lex->token == tk_left_parenth) {
            if (state == ST_WANT_VALUE || state == ST_DEMAND_VALUE) {
                loli_es_enter_tree(parser->expr, tree_parenth);
                state = ST_DEMAND_VALUE;
            }
            else if (state == ST_WANT_OPERATOR) {
                loli_es_enter_tree(parser->expr, tree_call);
                state = ST_WANT_VALUE;
            }
        }
        else if (lex->token == tk_left_bracket) {
            if (state == ST_WANT_VALUE || state == ST_DEMAND_VALUE) {
                loli_es_enter_tree(parser->expr, tree_list);
                state = ST_WANT_VALUE;
            }
            else if (state == ST_WANT_OPERATOR) {
                loli_es_enter_tree(parser->expr, tree_subscript);
                state = ST_DEMAND_VALUE;
            }
        }
        else if (lex->token == tk_prop_word) {
            if (state == ST_WANT_OPERATOR)
                state = ST_DONE;
            else
                expression_property(parser, &state);
        }
        else if (lex->token == tk_tuple_open) {
            if (state == ST_WANT_OPERATOR)
                state = ST_DONE;
            else {
                loli_es_enter_tree(parser->expr, tree_tuple);
                state = ST_WANT_VALUE;
            }
        }
        else if (lex->token == tk_right_parenth ||
                 lex->token == tk_right_bracket ||
                 lex->token == tk_tuple_close) {
            if (state == ST_DEMAND_VALUE) {
                 
                if (parser->expr->save_depth &&
                    (lex->token == tk_right_bracket ||
                     lex->token == tk_tuple_close)) {
                    check_valid_close_tok(parser);
                    loli_es_leave_tree(parser->expr);
                    state = ST_WANT_OPERATOR;
                }
                else
                    state = ST_BAD_TOKEN;
            }
            else if (state == ST_WANT_OPERATOR &&
                     parser->expr->save_depth == 0)
                state = ST_DONE;
            else {
                check_valid_close_tok(parser);
                loli_es_leave_tree(parser->expr);
                state = ST_WANT_OPERATOR;
            }
        }
        else if (lex->token == tk_integer || lex->token == tk_double ||
                 lex->token == tk_double_quote || lex->token == tk_bytestring ||
                 lex->token == tk_byte)
            expression_literal(parser, &state);
        else if (lex->token == tk_dot)
            expression_dot(parser, &state);
        else if (lex->token == tk_minus ||
                 lex->token == tk_not ||
                 lex->token == tk_tilde)
            expression_unary(parser, &state);
        else if (lex->token == tk_lambda) {
             
            if (state == ST_WANT_OPERATOR)
                loli_es_enter_tree(parser->expr, tree_call);

            loli_expr_state *es = parser->expr;
            int spot = es->pile_current;
            loli_sp_insert(parser->expr_strings, lex->label, &es->pile_current);
            loli_es_push_text(parser->expr, tree_lambda, lex->expand_start_line,
                    spot);

            if (state == ST_WANT_OPERATOR)
                loli_es_leave_tree(parser->expr);

            state = ST_WANT_OPERATOR;
        }
         
        else if (parser_tok_table[lex->token].val_or_end &&
                 parser->expr->save_depth == 0 &&
                 state == ST_WANT_OPERATOR)
            state = ST_DONE;
        else if (lex->token == tk_comma || lex->token == tk_arrow)
            expression_comma_arrow(parser, &state);
        else if (lex->token == tk_keyword_arg)
            expression_named_arg(parser, &state);
        else
            state = ST_BAD_TOKEN;

        if (state == ST_DONE)
            break;
        else if (state == ST_BAD_TOKEN)
            loli_raise_syn(parser->raiser, "Unexpected token '%s'.",
                    tokname(lex->token));
        else if (state & ST_FUTURE)
            state &= ~ST_FUTURE;
        else
            loli_lexer(lex);
    }
}

static void expression(loli_parse_state *parser)
{
    loli_es_flush(parser->expr);
    expression_raw(parser);
}


static loli_var *get_named_var(loli_parse_state *, loli_type *);


static inline void handle_multiline(loli_parse_state *, int);

static loli_type *parse_lambda_body(loli_parse_state *parser,
        loli_type *expect_type)
{
    loli_lex_state *lex = parser->lex;
    int key_id = -1;
    loli_type *result_type = NULL;

    loli_lexer(parser->lex);
    while (1) {
        if (lex->token == tk_word)
            key_id = keyword_by_name(lex->label);

        if (key_id == -1) {
            expression(parser);
            if (lex->token != tk_end_lambda)
                 
                loli_emit_eval_expr(parser->emit, parser->expr);
            else {
                 
                loli_emit_eval_lambda_body(parser->emit, parser->expr,
                        expect_type);

                if (parser->expr->root->result)
                    result_type = parser->expr->root->result->type;

                break;
            }
        }
        else {
             
            loli_lexer(lex);
            handle_multiline(parser, key_id);

            key_id = -1;
            if (lex->token == tk_end_lambda)
                break;
        }
    }

    return result_type;
}

static int collect_lambda_args(loli_parse_state *parser,
        loli_type *expect_type)
{
    int infer_count = (expect_type) ? expect_type->subtype_count : -1;
    int num_args = 0;
    loli_lex_state *lex = parser->lex;

    while (1) {
        NEED_NEXT_TOK(tk_word)
        loli_var *arg_var = get_named_var(parser, NULL);
        loli_type *arg_type;

        if (lex->token == tk_colon) {
            loli_lexer(lex);
            arg_type = get_type(parser);
            arg_var->type = arg_type;
        }
        else {
            arg_type = NULL;
            if (num_args < infer_count)
                arg_type = expect_type->subtypes[num_args + 1];

            if (arg_type == NULL || arg_type->flags & TYPE_IS_INCOMPLETE)
                loli_raise_syn(parser->raiser, "Cannot infer type of '%s'.",
                        lex->label);

            arg_var->type = arg_type;
        }

        loli_tm_add(parser->tm, arg_type);
        num_args++;

        if (lex->token == tk_comma)
            continue;
        else if (lex->token == tk_bitwise_or)
            break;
        else
            loli_raise_syn(parser->raiser,
                    "Expected either ',' or '|', not '%s'.",
                    tokname(lex->token));
    }

    return num_args;
}

static void ensure_not_in_optargs(loli_parse_state *parser, int line)
{
    loli_block *block = parser->emit->block;

    if (block->block_type != block_define &&
        block->block_type != block_class)
        return;

    if (block->patch_start != loli_u16_pos(parser->emit->patches)) {
        parser->lex->line_num = line;
        loli_raise_syn(parser->raiser,
                "Optional arguments are not allowed to use lambdas.");
    }
}

loli_var *loli_parser_lambda_eval(loli_parse_state *parser,
        int lambda_start_line, const char *lambda_body, loli_type *expect_type)
{
    loli_lex_state *lex = parser->lex;
    int args_collected = 0, tm_return = parser->tm->pos;
    loli_type *root_result;

    ensure_not_in_optargs(parser, lambda_start_line);

     
    loli_lexer_load(lex, et_lambda, lambda_body);
    lex->line_num = lambda_start_line;

     
    loli_var *lambda_var = new_native_define_var(parser, NULL, "(lambda)",
            lex->line_num);

     
    loli_emit_enter_call_block(parser->emit, block_lambda, lambda_var);

    loli_tm_add(parser->tm, loli_unit_type);

    loli_lexer(lex);

     
    if (lex->token == tk_bitwise_or)
        args_collected = collect_lambda_args(parser, expect_type);

     
    loli_es_checkpoint_save(parser->expr);
    root_result = parse_lambda_body(parser, expect_type);
    loli_es_checkpoint_restore(parser->expr);

    if (root_result != NULL)
        loli_tm_insert(parser->tm, tm_return, root_result);

    int flags = 0;
    if (expect_type && expect_type->cls->id == LOLI_ID_FUNCTION &&
        expect_type->flags & TYPE_IS_VARARGS)
        flags = TYPE_IS_VARARGS;

    lambda_var->type = loli_tm_make_call(parser->tm, flags,
            parser->symtab->function_class, args_collected + 1);

    hide_block_vars(parser);
    loli_emit_leave_call_block(parser->emit, lex->line_num);
    loli_pop_lex_entry(lex);

    return lambda_var;
}



static void keyword_if(loli_parse_state *);
static void keyword_do(loli_parse_state *);
static void keyword_var(loli_parse_state *);
static void keyword_for(loli_parse_state *);
static void keyword_try(loli_parse_state *);
static void keyword_case(loli_parse_state *);
static void keyword_else(loli_parse_state *);
static void keyword_elif(loli_parse_state *);
static void keyword_enum(loli_parse_state *);
static void keyword_while(loli_parse_state *);
static void keyword_raise(loli_parse_state *);
static void keyword_match(loli_parse_state *);
static void keyword_break(loli_parse_state *);
static void keyword_class(loli_parse_state *);
static void keyword_public(loli_parse_state *);
static void keyword_static(loli_parse_state *);
static void keyword_scoped(loli_parse_state *);
static void keyword_fn(loli_parse_state *);
static void keyword_return(loli_parse_state *);
static void keyword_except(loli_parse_state *);
static void keyword_import(loli_parse_state *);
static void keyword_future(loli_parse_state *);
static void keyword_private(loli_parse_state *);
static void keyword_protected(loli_parse_state *);
static void keyword_continue(loli_parse_state *);

typedef void (keyword_handler)(loli_parse_state *);

static keyword_handler *handlers[] = {
    keyword_if,
    keyword_fn,
    keyword_do,
    keyword_static,
    keyword_public,
    keyword_private,
    keyword_protected,
    keyword_var,
    keyword_for,
    keyword_try,
    keyword_case,
    keyword_else,
    keyword_elif,
    keyword_enum,
    keyword_while,
    keyword_raise,
    keyword_match,
    keyword_break,
    keyword_class,
    keyword_scoped,
    keyword_future,
    keyword_return,
    keyword_except,
    keyword_import,
    keyword_continue
};

static inline void handle_multiline(loli_parse_state *parser, int key_id)
{
    handlers[key_id](parser);
}

static loli_var *get_named_var(loli_parse_state *parser, loli_type *var_type)
{
    loli_lex_state *lex = parser->lex;
    loli_var *var;

    var = loli_find_var(parser->symtab, NULL, lex->label);
    if (var != NULL)
        loli_raise_syn(parser->raiser, "%s has already been declared.",
                lex->label);

    var = new_scoped_var(parser, var_type, lex->label, lex->line_num);

    loli_lexer(lex);
    return var;
}

static loli_var *get_local_var(loli_parse_state *parser, loli_type *var_type)
{
    loli_lex_state *lex = parser->lex;
    loli_var *var;

    var = loli_find_var(parser->symtab, NULL, lex->label);
    if (var != NULL)
        loli_raise_syn(parser->raiser, "%s has already been declared.",
                lex->label);

    var = new_local_var(parser, var_type, lex->label, lex->line_num);

    loli_lexer(lex);
    return var;
}

static void ensure_unique_class_member(loli_parse_state *parser,
        const char *name)
{
    loli_class *current_class = parser->class_self_type->cls;
    loli_named_sym *sym = loli_find_member(current_class, name, NULL);

    if (sym) {
        if (sym->item_kind == ITEM_TYPE_VAR)
            loli_raise_syn(parser->raiser,
                    "A method in class '%s' already has the name '%s'.",
                    current_class->name, name);
        else
            loli_raise_syn(parser->raiser,
                    "A property in class %s already has the name @%s.",
                    current_class->name, name);
    }
}

static loli_prop_entry *get_named_property(loli_parse_state *parser, int flags)
{
    char *name = parser->lex->label;
    loli_class *current_class = parser->class_self_type->cls;

    ensure_unique_class_member(parser, name);

    loli_prop_entry *prop;
    prop = loli_add_class_property(parser->symtab, current_class, NULL, name,
            flags);

    loli_lexer(parser->lex);
    return prop;
}

static void bad_decl_token(loli_parse_state *parser)
{
    const char *message;

    if (parser->lex->token == tk_word)
        message = "Class properties must start with @.";
    else
        message = "Cannot use a class property outside of a constructor.";

    loli_raise_syn(parser->raiser, message);
}

static void add_unresolved_defines_to_msgbuf(loli_parse_state *parser,
        loli_msgbuf *msgbuf)
{
    int count = parser->emit->block->pending_future_decls;
    loli_module_entry *m = parser->symtab->active_module;
    loli_var *var_iter;

    if (parser->emit->block->block_type == block_file)
        var_iter = m->var_chain;
    else
        var_iter = (loli_var *)m->class_chain->members;

    while (var_iter) {
        if (var_iter->flags & VAR_IS_FUTURE) {
            loli_proto *p = loli_emit_proto_for_var(parser->emit, var_iter);

            loli_mb_add_fmt(msgbuf, "\n* %s at line %d", p->name,
                    var_iter->line_num);

            if (count == 1)
                break;
            else
                count--;
        }

        var_iter = var_iter->next;
    }
}

static void error_future_decl_keyword(loli_parse_state *parser, int key)
{
    loli_msgbuf *msgbuf = loli_mb_flush(parser->msgbuf);
    const char *action = "";

    if (key == KEY_VAR) {
        if (parser->emit->block->block_type == block_class)
            action = "declare a class property";
        else
            action = "declare a global var";
    }
    else
        action = "use 'import'";

    loli_mb_add_fmt(msgbuf, "Cannot %s when there are unresolved future(s):",
            action);

    add_unresolved_defines_to_msgbuf(parser, msgbuf);
    loli_raise_syn(parser->raiser, loli_mb_raw(msgbuf));
}

static void parse_var(loli_parse_state *parser, int modifiers)
{
    loli_lex_state *lex = parser->lex;
    loli_sym *sym = NULL;
    loli_block *block = parser->emit->block;

    loli_token want_token, other_token;
    if (block->block_type == block_class) {
        if (modifiers == 0)
            loli_raise_syn(parser->raiser,
                    "Class var declaration must start with a scope.");

        modifiers &= ~PUBLIC_SCOPE;

        want_token = tk_prop_word;
        other_token = tk_word;
    }
    else {
        want_token = tk_word;
        other_token = tk_prop_word;
    }

    if (block->pending_future_decls)
        error_future_decl_keyword(parser, KEY_VAR);

     
    int flags = SYM_NOT_INITIALIZED | modifiers;

    while (1) {
        loli_es_flush(parser->expr);

         
        if (lex->token == other_token)
            bad_decl_token(parser);

        NEED_CURRENT_TOK(want_token)

        if (lex->token == tk_word) {
            sym = (loli_sym *)get_named_var(parser, NULL);
            sym->flags |= SYM_NOT_INITIALIZED;
            if (sym->flags & VAR_IS_GLOBAL)
                loli_es_push_global_var(parser->expr, (loli_var *)sym);
            else
                loli_es_push_local_var(parser->expr, (loli_var *)sym);
        }
        else {
            sym = (loli_sym *)get_named_property(parser, flags);
            loli_es_push_property(parser->expr, (loli_prop_entry *)sym);
        }

        if (lex->token == tk_colon) {
            loli_lexer(lex);
            sym->type = get_type(parser);
        }

        if (lex->token != tk_equal) {
            loli_raise_syn(parser->raiser,
                    "An initialization expression is required here.");
        }

        loli_es_push_binary_op(parser->expr, expr_assign);
        loli_lexer(lex);
        expression_raw(parser);
        loli_emit_eval_expr(parser->emit, parser->expr);

        if (lex->token != tk_comma)
            break;

        loli_lexer(lex);
    }
}

static void keyword_var(loli_parse_state *parser)
{
    parse_var(parser, 0);
}

static void send_optargs_for(loli_parse_state *parser, loli_var *var)
{
    loli_type *type = var->type;
    loli_proto *proto = loli_emit_proto_for_var(parser->emit, var);
    void (*optarg_func)(loli_emit_state *, loli_ast *) = loli_emit_eval_optarg;
    int count = loli_func_type_num_optargs(type);

    if (proto->arg_names == NULL)
        loli_emit_write_keyless_optarg_header(parser->emit, type);
    else
        optarg_func = loli_emit_eval_optarg_keyed;

    loli_es_checkpoint_save(parser->expr);

     
    loli_es_checkpoint_reverse_n(parser->expr, count);

    int i;

    for (i = 0;i < count;i++) {
        loli_es_checkpoint_restore(parser->expr);
        optarg_func(parser->emit, parser->expr->root);
    }

     
    loli_es_checkpoint_restore(parser->expr);
}

static void verify_existing_decl(loli_parse_state *parser, loli_var *var,
        int modifiers)
{
    if ((var->flags & VAR_IS_FUTURE) == 0) {
        loli_proto *p = loli_emit_proto_for_var(parser->emit, var);
        loli_raise_syn(parser->raiser, "%s has already been declared.",
                p->name);
    }
    else if (modifiers & VAR_IS_FUTURE) {
        loli_raise_syn(parser->raiser,
                "A future declaration for %s already exists.", var->name);
    }
}

static loli_var *find_existing_define(loli_parse_state *parser,
        loli_class *parent, char *label, int modifiers)
{
    loli_var *var = loli_find_var(parser->symtab, NULL, label);

    if (var)
        verify_existing_decl(parser, var, modifiers);

    if (parent) {
        loli_named_sym *sym = loli_find_member(parent, label, NULL);

        if (sym) {
            if (sym->item_kind != ITEM_TYPE_VAR)
                loli_raise_syn(parser->raiser,
                        "A property in class %s already has the name @%s.",
                        parent->name, label);
            else {
                var = (loli_var *)sym;
                if ((var->flags & VAR_IS_FUTURE) == 0) {
                    loli_raise_syn(parser->raiser,
                            "A method in class '%s' already has the name '%s'.",
                            parent->name, label);
                }
                verify_existing_decl(parser, (loli_var *)sym, modifiers);
            }

            var = (loli_var *)sym;
        }
    }

    return var;
}

static void error_future_decl_pending(loli_parse_state *parser)
{
    loli_msgbuf *msgbuf = loli_mb_flush(parser->msgbuf);
    const char *what = "";

     
    if (parser->emit->block->block_type == block_file)
        what = "module";
    else
        what = "class";

    loli_mb_add_fmt(msgbuf,
            "Reached end of %s with unresolved future(s):", what);
    add_unresolved_defines_to_msgbuf(parser, msgbuf);
    loli_raise_syn(parser->raiser, loli_mb_raw(msgbuf));
}

#define ALL_MODIFIERS \
    (SYM_SCOPE_PRIVATE | SYM_SCOPE_PROTECTED | VAR_IS_STATIC)

static void error_future_decl_modifiers(loli_parse_state *parser,
        loli_var *define_var)
{
    loli_msgbuf *msgbuf = loli_mb_flush(parser->msgbuf);
    loli_proto *p = loli_emit_proto_for_var(parser->emit, define_var);
    int modifiers = define_var->flags;

    loli_mb_add_fmt(msgbuf, "Wrong qualifiers in resolution of %s (expected: ",
            p->name);

     
    if (define_var->flags & SYM_SCOPE_PRIVATE)
        loli_mb_add(msgbuf, "private");
    else if (define_var->flags & SYM_SCOPE_PROTECTED)
        loli_mb_add(msgbuf, "protected");
    else
        loli_mb_add(msgbuf, "public");

    if (modifiers & VAR_IS_STATIC)
        loli_mb_add(msgbuf, " static");

    loli_mb_add(msgbuf, ").");
    loli_raise_syn(parser->raiser, loli_mb_raw(msgbuf));
}

static void parse_define_header(loli_parse_state *parser, int modifiers)
{
    loli_lex_state *lex = parser->lex;
    NEED_CURRENT_TOK(tk_word)

    loli_class *parent = NULL;
    loli_block_type block_type = parser->emit->block->block_type;
    int collect_flag = F_COLLECT_DEFINE;

    if (block_type == block_class || block_type == block_enum)
        parent = parser->class_self_type->cls;

    loli_var *old_define = find_existing_define(parser, parent, lex->label,
            modifiers);

    loli_var *define_var;

    if (old_define) {
        if ((old_define->flags & ALL_MODIFIERS) !=
            (modifiers & ALL_MODIFIERS))
            error_future_decl_modifiers(parser, old_define);

        define_var = old_define;
        loli_emit_resolve_future_decl(parser->emit, define_var);
    }
    else {
        if (modifiers & VAR_IS_FUTURE)
            collect_flag = F_COLLECT_FUTURE;

        define_var = new_native_define_var(parser, parent, lex->label,
                lex->line_num);
    }

     
    define_var->flags |= SYM_NOT_INITIALIZED | modifiers;

     
    loli_tm_add(parser->tm, loli_unit_type);

    loli_lexer(lex);
    collect_generics(parser);
    loli_emit_enter_call_block(parser->emit, block_define, define_var);

    if (parent && (define_var->flags & VAR_IS_STATIC) == 0) {
         
        loli_tm_add(parser->tm, parser->class_self_type);

        loli_var *self_var = new_local_var(parser, parser->class_self_type,
                "(self)", lex->line_num);

        parser->emit->block->self = (loli_storage *)self_var;
    }

    collect_call_args(parser, define_var, collect_flag);

    NEED_CURRENT_TOK(tk_left_curly)

    if (define_var->type->flags & TYPE_HAS_OPTARGS &&
        collect_flag != F_COLLECT_FUTURE)
        send_optargs_for(parser, define_var);

    define_var->flags &= ~SYM_NOT_INITIALIZED;
}

#undef ALL_MODIFIERS

static loli_var *parse_for_range_value(loli_parse_state *parser,
        const char *name)
{
    loli_expr_state *es = parser->expr;
    expression(parser);

     
    if (es->root->tree_type == tree_binary &&
        es->root->op >= expr_assign) {
        loli_raise_syn(parser->raiser,
                   "For range value expression contains an assignment.");
    }

    loli_class *cls = parser->symtab->integer_class;

     
    loli_var *var = new_local_var(parser, cls->self_type, name,
            parser->lex->line_num);

    loli_emit_eval_expr_to_var(parser->emit, es, var);

    return var;
}

static void process_docstring(loli_parse_state *parser)
{
    loli_lex_state *lex = parser->lex;
    loli_lexer(lex);

    int key_id;
    if (lex->token == tk_word)
        key_id = keyword_by_name(lex->label);
    else
        key_id = -1;

    if (key_id == KEY_PRIVATE ||
        key_id == KEY_PROTECTED ||
        key_id == KEY_FN ||
        key_id == KEY_CLASS) {
        loli_lexer(lex);
        handlers[key_id](parser);
    }
    else
        loli_raise_syn(parser->raiser,
                "Docstring must be followed by a function or class definition.");
}

static void statement(loli_parse_state *parser, int multi)
{
    int key_id;
    loli_lex_state *lex = parser->lex;

    do {
        loli_token token = lex->token;

        if (token == tk_word) {
            key_id = keyword_by_name(lex->label);
            if (key_id != -1) {
                 
                loli_lexer(lex);
                handlers[key_id](parser);
            }
            else {
                expression(parser);
                loli_emit_eval_expr(parser->emit, parser->expr);
            }
        }
        else if (token == tk_integer || token == tk_double ||
                 token == tk_double_quote || token == tk_left_parenth ||
                 token == tk_left_bracket || token == tk_tuple_open ||
                 token == tk_prop_word || token == tk_bytestring ||
                 token == tk_byte) {
            expression(parser);
            loli_emit_eval_expr(parser->emit, parser->expr);
        }
        else if (token == tk_docstring)
            process_docstring(parser);
         
        else if (multi)
            break;
         
        else
            loli_raise_syn(parser->raiser, "Expected a value, not '%s'.",
                    tokname(token));
    } while (multi);
}

static void parse_block_body(loli_parse_state *parser)
{
    loli_lex_state *lex = parser->lex;

    loli_lexer(lex);
     
    if (lex->token != tk_right_curly)
        statement(parser, 1);
    NEED_CURRENT_TOK(tk_right_curly)
    loli_lexer(lex);
}

static void do_elif(loli_parse_state *parser)
{
    loli_lex_state *lex = parser->lex;
    hide_block_vars(parser);
    loli_emit_change_block_to(parser->emit, block_if_elif);
    expression(parser);
    loli_emit_eval_condition(parser->emit, parser->expr);

    NEED_CURRENT_TOK(tk_colon)

    loli_lexer(lex);
}

static void do_else(loli_parse_state *parser)
{
    loli_lex_state *lex = parser->lex;

    hide_block_vars(parser);
    loli_emit_change_block_to(parser->emit, block_if_else);

    NEED_CURRENT_TOK(tk_colon)
    loli_lexer(lex);
}

static void keyword_if(loli_parse_state *parser)
{
    loli_lex_state *lex = parser->lex;

    loli_emit_enter_block(parser->emit, block_if);
    expression(parser);
    loli_emit_eval_condition(parser->emit, parser->expr);
    NEED_CURRENT_TOK(tk_colon)
    NEED_NEXT_TOK(tk_left_curly)
    loli_lexer(lex);

    int have_else = 0;

    while (1) {
        if (lex->token == tk_word) {
            int key = keyword_by_name(lex->label);
            if (key == -1) {
                expression(parser);
                loli_emit_eval_expr(parser->emit, parser->expr);
            }
            else if (key != KEY_ELIF && key != KEY_ELSE) {
                loli_lexer(lex);
                handlers[key](parser);
            }
            else if (have_else == 1) {
                const char *what = "else";

                if (key == KEY_ELIF)
                    what = "elif";

                loli_raise_syn(parser->raiser,
                        "%s after else in multi-line if block.", what);
            }
        }
        else if (lex->token != tk_right_curly) {
            expression(parser);
            loli_emit_eval_expr(parser->emit, parser->expr);
        }

        if (lex->token == tk_word && have_else == 0) {
            int key = keyword_by_name(lex->label);

            if (key == KEY_ELIF || key == KEY_ELSE) {
                loli_lexer(lex);
                if (key == KEY_ELIF)
                    do_elif(parser);
                else {
                    do_else(parser);
                    have_else = 1;
                }

                continue;
            }
        }
        else if (lex->token == tk_right_curly)
            break;
    }

    loli_lexer(lex);
    hide_block_vars(parser);
    loli_emit_leave_block(parser->emit);
}

static void keyword_elif(loli_parse_state *parser)
{
    loli_raise_syn(parser->raiser, "'elif' without 'if'.");
}

static void keyword_else(loli_parse_state *parser)
{
    loli_raise_syn(parser->raiser, "'else' without 'if'.");
    }

static int code_is_after_exit(loli_parse_state *parser)
{
    loli_token token = parser->lex->token;

     
    if (token == tk_right_curly ||
        token == tk_eof ||
        token == tk_end_tag)
        return 0;

    if (token == tk_word) {
        int key_id = keyword_by_name(parser->lex->label);

         
        if (key_id == KEY_ELIF ||
            key_id == KEY_ELSE ||
            key_id == KEY_EXCEPT ||
            key_id == KEY_CASE)
            return 0;
    }

    return 1;
}

static void keyword_return(loli_parse_state *parser)
{
    loli_block *block = parser->emit->function_block;
    loli_type *return_type = NULL;

    if (block->block_type == block_class)
        loli_raise_syn(parser->raiser,
                "'return' not allowed in a class constructor.");
    else if (block->block_type == block_lambda)
        loli_raise_syn(parser->raiser, "'return' not allowed in a lambda.");
    else if (block->block_type == block_file)
        loli_raise_syn(parser->raiser, "'return' used outside of a function.");
    else
        return_type = block->function_var->type->subtypes[0];

    if (return_type != loli_unit_type)
        expression(parser);

    loli_emit_eval_return(parser->emit, parser->expr, return_type);

    if (code_is_after_exit(parser)) {
        const char *extra = ".";
        if (return_type == loli_unit_type)
            extra = " (no return type given).";

        loli_raise_syn(parser->raiser,
                "Statement(s) after 'return' will not execute%s", extra);
    }
}

static void keyword_while(loli_parse_state *parser)
{
    loli_lex_state *lex = parser->lex;

    loli_emit_enter_block(parser->emit, block_while);

    expression(parser);
    loli_emit_eval_condition(parser->emit, parser->expr);

    NEED_CURRENT_TOK(tk_colon)
    NEED_NEXT_TOK(tk_left_curly)
    parse_block_body(parser);

    hide_block_vars(parser);
    loli_emit_leave_block(parser->emit);
}

static void keyword_continue(loli_parse_state *parser)
{
    loli_emit_continue(parser->emit);

    if (code_is_after_exit(parser))
        loli_raise_syn(parser->raiser,
                "Statement(s) after 'continue' will not execute.");
}

static void keyword_break(loli_parse_state *parser)
{
    loli_emit_break(parser->emit);

    if (code_is_after_exit(parser))
        loli_raise_syn(parser->raiser,
                "Statement(s) after 'break' will not execute.");
}

static void keyword_for(loli_parse_state *parser)
{
    loli_lex_state *lex = parser->lex;
    loli_var *loop_var;

    NEED_CURRENT_TOK(tk_word)

    loli_emit_enter_block(parser->emit, block_for_in);

    loop_var = loli_find_var(parser->symtab, NULL, lex->label);
    if (loop_var == NULL) {
        loli_class *cls = parser->symtab->integer_class;
        loop_var = new_local_var(parser, cls->self_type, lex->label,
                lex->line_num);
    }
    else if (loop_var->type->cls->id != LOLI_ID_INTEGER) {
        loli_raise_syn(parser->raiser,
                   "Loop var must be type Integer, not type '^T'.",
                   loop_var->type);
    }

    NEED_NEXT_TOK(tk_word)
    if (strcmp(lex->label, "in") != 0)
        loli_raise_syn(parser->raiser, "Expected 'in', not '%s'.", lex->label);

    loli_lexer(lex);

    loli_var *for_start, *for_end;
    loli_sym *for_step;

    for_start = parse_for_range_value(parser, "(for start)");

    NEED_CURRENT_TOK(tk_three_dots)
    loli_lexer(lex);

    for_end = parse_for_range_value(parser, "(for end)");

    if (lex->token == tk_word) {
        if (strcmp(lex->label, "by") != 0)
            loli_raise_syn(parser->raiser, "Expected 'by', not '%s'.",
                    lex->label);

        loli_lexer(lex);
        for_step = (loli_sym *)parse_for_range_value(parser, "(for step)");
    }
    else {
        loli_var *step_var = new_local_var(parser,
                parser->symtab->integer_class->self_type, "(for step)",
                lex->line_num);
         
        loli_es_flush(parser->expr);
        loli_es_push_integer(parser->expr, 1);
        loli_emit_eval_expr_to_var(parser->emit, parser->expr, step_var);
        for_step = (loli_sym *)step_var;
    }

    loli_emit_finalize_for_in(parser->emit, loop_var, for_start, for_end,
                              for_step, parser->lex->line_num);

    NEED_CURRENT_TOK(tk_colon)
    NEED_NEXT_TOK(tk_left_curly)
    parse_block_body(parser);

    hide_block_vars(parser);
    loli_emit_leave_block(parser->emit);
}

static void keyword_do(loli_parse_state *parser)
{
    loli_lex_state *lex = parser->lex;

    loli_emit_enter_block(parser->emit, block_do_while);

    NEED_CURRENT_TOK(tk_colon)
    NEED_NEXT_TOK(tk_left_curly)
    parse_block_body(parser);

    NEED_CURRENT_TOK(tk_word)
     
    if (strcmp(lex->label, "while") != 0)
        loli_raise_syn(parser->raiser, "Expected 'while', not '%s'.",
                lex->label);

     
    loli_lexer(lex);

     
    hide_block_vars(parser);

    expression(parser);
    loli_emit_eval_condition(parser->emit, parser->expr);
    loli_emit_leave_block(parser->emit);
}

static void run_loaded_module(loli_parse_state *parser,
        loli_module_entry *module)
{
     
    module->flags &= ~MODULE_NOT_EXECUTED;
    module->flags |= MODULE_IN_EXECUTION;

    loli_module_entry *save_active = parser->symtab->active_module;
    loli_lex_state *lex = parser->lex;

    parser->symtab->active_module = module;

     
    loli_var *import_var = new_native_define_var(parser, NULL, "__module__",
            lex->line_num);

    import_var->type = parser->default_call_type;

    loli_emit_enter_call_block(parser->emit, block_file, import_var);

     
    loli_lexer(lex);
    statement(parser, 1);

     
    if (lex->token == tk_right_curly)
        loli_raise_syn(parser->raiser, "'}' outside of a block.");

    if (lex->token == tk_end_tag)
        loli_raise_syn(parser->raiser, "Unexpected token '?>'.");

    if (lex->token == tk_invalid)
        loli_raise_syn(parser->raiser, "Unexpected token '%s'.",
                tokname(lex->token));

    if (parser->emit->block->pending_future_decls)
        error_future_decl_pending(parser);

    loli_emit_leave_call_block(parser->emit, lex->line_num);
     
    loli_pop_lex_entry(parser->lex);

    loli_emit_write_import_call(parser->emit, import_var);

    parser->symtab->active_module = save_active;

    module->flags &= ~MODULE_IN_EXECUTION;
}

static loli_sym *find_existing_sym(loli_parse_state *parser,
        loli_module_entry *source, const char *search_name)
{
    loli_symtab *symtab = parser->symtab;
    loli_sym *sym;

    sym = (loli_sym *)loli_find_var(symtab, source, search_name);

    if (sym == NULL)
        sym = (loli_sym *)loli_find_class(symtab, source, search_name);

    if (sym == NULL)
        sym = (loli_sym *)loli_find_module(symtab, source, search_name);

    if (sym == NULL && source->info_table)
        sym = (loli_sym *)try_toplevel_dynaload(parser, source, search_name);

    return sym;
}

static void link_import_syms(loli_parse_state *parser,
        loli_module_entry *source, uint16_t start, int count)
{
    loli_symtab *symtab = parser->symtab;
    loli_module_entry *active = symtab->active_module;

    do {
        char *search_name = loli_sp_get(parser->import_ref_strings, start);
        loli_sym *sym = find_existing_sym(parser, active, search_name);

        if (sym)
            loli_raise_syn(parser->raiser, "'%s' has already been declared.",
                    search_name);

        sym = find_existing_sym(parser, source, search_name);

        if (sym == NULL)
            loli_raise_syn(parser->raiser,
                    "Cannot find symbol '%s' inside of module '%s'.",
                    search_name, source->loadname);
        else if (sym->item_kind == ITEM_TYPE_MODULE)
            loli_raise_syn(parser->raiser,
                    "Not allowed to directly import modules ('%s').",
                    search_name);

        loli_add_symbol_ref(active, sym);
        start += strlen(search_name) + 1;
        count--;
    } while (count);
}

static void collect_import_refs(loli_parse_state *parser, int *count)
{
    loli_lex_state *lex = parser->lex;
    uint16_t top = parser->import_pile_current;

    while (1) {
        NEED_NEXT_TOK(tk_word)
        loli_sp_insert(parser->import_ref_strings, lex->label, &top);
        parser->import_pile_current = top;
        (*count)++;

        loli_lexer(lex);

        if (lex->token == tk_right_parenth)
            break;
        else if (lex->token != tk_comma)
            loli_raise_syn(parser->raiser,
                    "Expected either ',' or ')', not '%s'.",
                    tokname(lex->token));
    }

    loli_lexer(lex);
}

static void parse_verify_import_path(loli_parse_state *parser)
{
    loli_lex_state *lex = parser->lex;

    if (lex->token == tk_double_quote) {
        loli_lexer_verify_path_string(lex);
        const char *pending_loadname = strrchr(lex->label, LOLI_PATH_CHAR);

        if (pending_loadname == NULL)
            pending_loadname = lex->label;
        else
            pending_loadname += 1;

        parser->ims->pending_loadname = pending_loadname;
    }
    else if (lex->token == tk_word)
        parser->ims->pending_loadname = lex->label;
    else {
        loli_raise_syn(parser->raiser,
                "'import' expected a path (identifier or string), not %s.",
                tokname(lex->token));
    }
}

static void keyword_import(loli_parse_state *parser)
{
    loli_lex_state *lex = parser->lex;
    loli_block *block = parser->emit->block;
    if (block->block_type != block_file)
        loli_raise_syn(parser->raiser, "Cannot import a file here.");

    if (block->pending_future_decls)
        error_future_decl_keyword(parser, KEY_IMPORT);

    loli_symtab *symtab = parser->symtab;
    loli_module_entry *active = symtab->active_module;
    uint32_t save_import_current = parser->import_pile_current;
    int import_sym_count = 0;

    while (1) {
        if (lex->token == tk_left_parenth)
            collect_import_refs(parser, &import_sym_count);

        parse_verify_import_path(parser);

        loli_module_entry *module = NULL;
        char *search_start = lex->label;
        char *path_tail = strrchr(search_start, LOLI_PATH_CHAR);
         
        if (path_tail != NULL)
            search_start = path_tail + 1;

        if (loli_find_module(symtab, active, search_start))
            loli_raise_syn(parser->raiser,
                    "A module named '%s' has already been imported here.",
                    search_start);

        if (path_tail == NULL)
            module = loli_find_registered_module(symtab, lex->label);

         
        if (module == NULL) {
            module = load_module(parser, lex->label);
             

             
            if (module->flags & MODULE_NOT_EXECUTED)
                run_loaded_module(parser, module);
        }

        loli_lexer(parser->lex);
        if (lex->token == tk_word && strcmp(lex->label, "as") == 0) {
            if (import_sym_count)
                loli_raise_syn(parser->raiser,
                        "Cannot use 'as' when only specific items are being imported.");

            NEED_NEXT_TOK(tk_word)
             
            link_module_to(active, module, lex->label);
            loli_lexer(lex);
        }
        else if (import_sym_count) {
            link_import_syms(parser, module, save_import_current,
                    import_sym_count);
            parser->import_pile_current = save_import_current;
            import_sym_count = 0;
        }
        else
            link_module_to(active, module, NULL);

        if (lex->token == tk_comma) {
            loli_lexer(parser->lex);
            continue;
        }
        else
            break;
    }
}

static void process_except(loli_parse_state *parser)
{
    loli_lex_state *lex = parser->lex;

    loli_class *except_cls = get_type(parser)->cls;
    loli_block_type new_type = block_try_except;

     
    if (except_cls->id == LOLI_ID_EXCEPTION)
        new_type = block_try_except_all;
    else if (loli_class_greater_eq_id(LOLI_ID_EXCEPTION, except_cls) == 0)
        loli_raise_syn(parser->raiser, "'%s' is not a valid exception class.",
                except_cls->name);
    else if (except_cls->generic_count != 0)
        loli_raise_syn(parser->raiser, "'except' type cannot have subtypes.");

    hide_block_vars(parser);
    loli_emit_change_block_to(parser->emit, new_type);

    loli_var *exception_var = NULL;
    if (lex->token == tk_word) {
        if (strcmp(parser->lex->label, "as") != 0)
            loli_raise_syn(parser->raiser, "Expected 'as', not '%s'.",
                    lex->label);

        NEED_NEXT_TOK(tk_word)
        exception_var = loli_find_var(parser->symtab, NULL, lex->label);
        if (exception_var != NULL)
            loli_raise_syn(parser->raiser, "%s has already been declared.",
                    exception_var->name);

        exception_var = new_local_var(parser, except_cls->self_type,
                lex->label, lex->line_num);

        loli_lexer(lex);
    }

    NEED_CURRENT_TOK(tk_colon)
    loli_emit_except(parser->emit, except_cls->self_type, exception_var,
            lex->line_num);

    loli_lexer(lex);
}

static void keyword_try(loli_parse_state *parser)
{
    loli_lex_state *lex = parser->lex;

    loli_emit_enter_block(parser->emit, block_try);
    loli_emit_try(parser->emit, parser->lex->line_num);

    NEED_CURRENT_TOK(tk_colon)
    NEED_NEXT_TOK(tk_left_curly)
    loli_lexer(lex);

    while (1) {
        if (lex->token == tk_word) {
            int key = keyword_by_name(lex->label);
            if (key == -1) {
                expression(parser);
                loli_emit_eval_expr(parser->emit, parser->expr);
            }
            else if (key != KEY_EXCEPT) {
                loli_lexer(lex);
                handlers[key](parser);
            }
        }
        else if (lex->token != tk_right_curly)
            statement(parser, 0);

        if (lex->token == tk_word) {
            int key = keyword_by_name(lex->label);

            if (key == KEY_EXCEPT) {
                loli_lexer(lex);
                process_except(parser);
                continue;
            }
        }
        else if (lex->token == tk_right_curly)
            break;
    }

    loli_lexer(lex);
    hide_block_vars(parser);
    loli_emit_leave_block(parser->emit);
}

static void keyword_except(loli_parse_state *parser)
{
    loli_raise_syn(parser->raiser, "'except' outside 'try'.");
}

static void keyword_raise(loli_parse_state *parser)
{
    if (parser->emit->function_block->block_type == block_lambda)
        loli_raise_syn(parser->raiser, "'raise' not allowed in a lambda.");

    expression(parser);
    loli_emit_raise(parser->emit, parser->expr);

    if (code_is_after_exit(parser))
        loli_raise_syn(parser->raiser,
                "Statement(s) after 'raise' will not execute.");
}

static void ensure_valid_class(loli_parse_state *parser, const char *name)
{
    if (name[1] == '\0')
        loli_raise_syn(parser->raiser,
                "'%s' is not a valid class name (too short).", name);

    loli_class *lookup_class = loli_find_class(parser->symtab, NULL, name);
    if (lookup_class != NULL) {
        loli_raise_syn(parser->raiser, "Class '%s' has already been declared.",
                name);
    }

    loli_item *item = try_toplevel_dynaload(parser,
            parser->symtab->builtin_module, name);
    if (item && item->item_kind != ITEM_TYPE_VAR)
        loli_raise_syn(parser->raiser,
                "A built-in class named '%s' already exists.", name);
}

static loli_class *parse_and_verify_super(loli_parse_state *parser,
        loli_class *cls)
{
    loli_lex_state *lex = parser->lex;
    NEED_NEXT_TOK(tk_word)

    loli_class *super_class = resolve_class_name(parser);

    if (super_class == cls)
        loli_raise_syn(parser->raiser, "A class cannot inherit from itself!");
    else if (super_class->item_kind == ITEM_TYPE_VARIANT ||
             super_class->flags & (CLS_IS_ENUM | CLS_IS_BUILTIN))
        loli_raise_syn(parser->raiser, "'%s' cannot be inherited from.",
                lex->label);

    int adjust = super_class->prop_count;

     
    cls->parent = super_class;
    cls->prop_count += super_class->prop_count;
    cls->inherit_depth = super_class->inherit_depth + 1;

    if (cls->prop_count && cls->members) {
         
        loli_named_sym *sym = cls->members;
        while (sym) {
            if (sym->item_kind == ITEM_TYPE_PROPERTY)
                sym->reg_spot += adjust;

            sym = sym->next;
        }
    }

    return super_class;
}

static void run_super_ctor(loli_parse_state *parser, loli_class *cls,
        loli_class *super_class)
{
    loli_lex_state *lex = parser->lex;
    loli_var *class_new = loli_find_method(super_class, "<new>");

     

    loli_expr_state *es = parser->expr;
    loli_es_flush(es);
    loli_es_push_inherited_new(es, class_new);
    loli_es_enter_tree(es, tree_call);
     
    es->save_depth = 0;

    loli_lexer(parser->lex);

    if (lex->token == tk_left_parenth) {
         
        loli_lexer(lex);
        if (lex->token == tk_right_parenth)
            loli_raise_syn(parser->raiser,
                    "Empty () not needed here for inherited new.");

        while (1) {
            expression_raw(parser);
            loli_es_collect_arg(parser->expr);
            if (lex->token == tk_comma) {
                loli_lexer(lex);
                continue;
            }
            else if (lex->token == tk_right_parenth) {
                loli_lexer(lex);
                break;
            }
            else
                loli_raise_syn(parser->raiser,
                        "Expected either ',' or ')', not '%s'.",
                        tokname(lex->token));
        }
    }

     
    parser->expr->save_depth = 1;
    loli_es_leave_tree(parser->expr);
    loli_emit_eval_expr(parser->emit, es);
}

static void parse_class_header(loli_parse_state *parser, loli_class *cls)
{
    loli_lex_state *lex = parser->lex;
     
    loli_var *call_var = new_native_define_var(parser, cls, "<new>",
            lex->line_num);

     
    call_var->flags |= SYM_NOT_INITIALIZED;

    loli_lexer(lex);
    collect_generics(parser);
    cls->generic_count = loli_gp_num_in_scope(parser->generics);

    loli_emit_enter_call_block(parser->emit, block_class, call_var);

    parser->class_self_type = build_self_type(parser, cls);

    loli_tm_add(parser->tm, parser->class_self_type);
    collect_call_args(parser, call_var, F_COLLECT_CLASS);

    loli_class *super_cls = NULL;

    if (lex->token == tk_lt)
        super_cls = parse_and_verify_super(parser, cls);

    loli_emit_write_class_header(parser->emit, parser->class_self_type,
            lex->line_num);

    if (call_var->type->flags & TYPE_HAS_OPTARGS)
        send_optargs_for(parser, call_var);

    call_var->flags &= ~SYM_NOT_INITIALIZED;

    if (cls->members->item_kind == ITEM_TYPE_PROPERTY)
        loli_emit_write_shorthand_ctor(parser->emit, cls,
                parser->symtab->active_module->var_chain, lex->line_num);

    if (super_cls)
        run_super_ctor(parser, cls, super_cls);
}

static int get_gc_flags_for(loli_class *top_class, loli_type *target)
{
    int result_flag = 0;

    if (target->cls->flags & (CLS_GC_TAGGED | CLS_VISITED))
        result_flag = CLS_GC_TAGGED;
    else {
        result_flag = target->cls->flags & (CLS_GC_TAGGED | CLS_GC_SPECULATIVE);

        if (target->subtype_count) {
            int i;
            for (i = 0;i < target->subtype_count;i++)
                result_flag |= get_gc_flags_for(top_class, target->subtypes[i]);
        }
    }

    return result_flag;
}

static void determine_class_gc_flag(loli_parse_state *parser,
        loli_class *target)
{
    loli_class *parent_iter = target->parent;
    int mark = 0;

    if (parent_iter) {
         
        mark = parent_iter->flags & (CLS_GC_TAGGED | CLS_GC_SPECULATIVE);
        if (mark == CLS_GC_TAGGED) {
            target->flags |= CLS_GC_TAGGED;
            return;
        }

        while (parent_iter) {
            parent_iter->flags |= CLS_VISITED;
            parent_iter = parent_iter->next;
        }
    }

     
    if (target->generic_count)
        mark |= CLS_GC_SPECULATIVE;

    loli_named_sym *member_iter = target->members;

    while (member_iter) {
        if (member_iter->item_kind == ITEM_TYPE_PROPERTY)
            mark |= get_gc_flags_for(target, member_iter->type);

        member_iter = member_iter->next;
    }

     
    if (mark & CLS_GC_TAGGED)
        mark &= ~CLS_GC_SPECULATIVE;

    parent_iter = target->parent;
    while (parent_iter) {
        parent_iter->flags &= ~CLS_VISITED;
        parent_iter = parent_iter->next;
    }

    target->flags &= ~CLS_VISITED;
    target->flags |= mark;
}

static void parse_class_body(loli_parse_state *parser, loli_class *cls)
{
    loli_lex_state *lex = parser->lex;
    loli_type *save_class_self_type = parser->class_self_type;
    uint16_t save_generic_start = loli_gp_save_and_hide(parser->generics);

    parse_class_header(parser, cls);

    NEED_CURRENT_TOK(tk_left_curly)
    parse_block_body(parser);

    if (parser->emit->block->pending_future_decls)
        error_future_decl_pending(parser);

    determine_class_gc_flag(parser, parser->class_self_type->cls);

    parser->class_self_type = save_class_self_type;
    hide_block_vars(parser);
    loli_emit_leave_call_block(parser->emit, lex->line_num);

    loli_gp_restore_and_unhide(parser->generics, save_generic_start);
}

static void keyword_class(loli_parse_state *parser)
{
    loli_block *block = parser->emit->block;
    if (block->block_type != block_file)
        loli_raise_syn(parser->raiser, "Cannot define a class here.");

    loli_lex_state *lex = parser->lex;
    NEED_CURRENT_TOK(tk_word);

    ensure_valid_class(parser, lex->label);

    parse_class_body(parser, loli_new_class(parser->symtab, lex->label));
}

static void parse_variant_header(loli_parse_state *parser,
        loli_variant_class *variant_cls)
{
     
    loli_tm_add(parser->tm, variant_cls->parent->self_type);
    collect_call_args(parser, variant_cls, F_COLLECT_VARIANT);

    variant_cls->flags &= ~CLS_EMPTY_VARIANT;
}

static loli_class *parse_enum(loli_parse_state *parser, int is_scoped)
{
    loli_block *block = parser->emit->block;
    if (block->block_type != block_file)
        loli_raise_syn(parser->raiser, "Cannot define an enum here.");

    loli_lex_state *lex = parser->lex;
    NEED_CURRENT_TOK(tk_word)

    if (is_scoped == 1) {
        if (strcmp(lex->label, "enum") != 0)
            loli_raise_syn(parser->raiser, "Expected 'enum' after 'scoped'.");

        NEED_NEXT_TOK(tk_word)
    }

    ensure_valid_class(parser, lex->label);

    loli_class *enum_cls = loli_new_enum_class(parser->symtab, lex->label);

    if (is_scoped)
        enum_cls->flags |= CLS_ENUM_IS_SCOPED;

    uint16_t save_generic_start = loli_gp_save_and_hide(parser->generics);

    loli_lexer(lex);
    collect_generics(parser);

    enum_cls->generic_count = loli_gp_num_in_scope(parser->generics);

    loli_emit_enter_block(parser->emit, block_enum);

    parser->class_self_type = build_self_type(parser, enum_cls);

    NEED_CURRENT_TOK(tk_left_curly)
    loli_lexer(lex);

    int variant_count = 0;

    while (1) {
        NEED_CURRENT_TOK(tk_word)
        if (variant_count) {
            loli_class *cls = (loli_class *)loli_find_variant(enum_cls,
                    lex->label);

            if (cls == NULL && is_scoped == 0)
                cls = loli_find_class(parser->symtab, NULL, lex->label);

            if (cls) {
                loli_raise_syn(parser->raiser,
                        "A class with the name '%s' already exists.",
                        lex->label);
            }
        }

        loli_variant_class *variant_cls = loli_new_variant_class(parser->symtab,
                enum_cls, lex->label);
        variant_count++;

        loli_lexer(lex);
        if (lex->token == tk_left_parenth)
            parse_variant_header(parser, variant_cls);

        if (lex->token == tk_right_curly)
            break;
        else if (lex->token == tk_word && lex->label[0] == 'f' &&
                 keyword_by_name(lex->label) == KEY_FN)
            break;
        else {
            NEED_CURRENT_TOK(tk_comma)
            loli_lexer(lex);
        }
    }

    if (variant_count < 2) {
        loli_raise_syn(parser->raiser,
                "An enum must have at least two variants.");
    }

     
    enum_cls->variant_size = variant_count;

    loli_fix_enum_variant_ids(parser->symtab, enum_cls);

    if (lex->token == tk_word) {
        while (1) {
            loli_lexer(lex);
            keyword_fn(parser);
            if (lex->token == tk_right_curly)
                break;
            else if (lex->token != tk_word ||
                keyword_by_name(lex->label) != KEY_FN)
                loli_raise_syn(parser->raiser,
                        "Expected '}' or 'fn', not '%s'.",
                        tokname(lex->token));
        }
    }

     
    loli_emit_leave_block(parser->emit);
    parser->class_self_type = NULL;

    loli_gp_restore_and_unhide(parser->generics, save_generic_start);
    loli_lexer(lex);

    return enum_cls;
}

static void keyword_enum(loli_parse_state *parser)
{
    parse_enum(parser, 0);
}

static void keyword_scoped(loli_parse_state *parser)
{
    parse_enum(parser, 1);
}

static void match_case_enum(loli_parse_state *parser, loli_sym *match_sym)
{
    loli_type *match_input_type = match_sym->type;
    loli_class *match_class = match_input_type->cls;
    loli_lex_state *lex = parser->lex;

    NEED_CURRENT_TOK(tk_word)
    if (match_class->flags & CLS_ENUM_IS_SCOPED) {
        if (strcmp(match_class->name, lex->label) != 0)
            loli_raise_syn(parser->raiser,
                    "Expected '%s.<variant>', not '%s' because '%s' is a scoped enum.",
                    match_class->name, lex->label, match_class->name);

        NEED_NEXT_TOK(tk_dot)
        NEED_NEXT_TOK(tk_word)
    }

    loli_variant_class *variant_case = loli_find_variant(match_class,
            lex->label);

    if (variant_case == NULL)
        loli_raise_syn(parser->raiser, "%s is not a member of enum %s.",
                lex->label, match_class->name);

    if (loli_emit_is_duplicate_case(parser->emit, (loli_class *)variant_case))
        loli_raise_syn(parser->raiser, "Already have a case for variant %s.",
                lex->label);

    hide_block_vars(parser);
    loli_emit_change_match_branch(parser->emit);
    loli_emit_write_match_case(parser->emit, match_sym,
            (loli_class *)variant_case);

    if ((variant_case->flags & CLS_EMPTY_VARIANT) == 0) {
        loli_type *build_type = variant_case->build_type;
        loli_type_system *ts = parser->emit->ts;

        NEED_NEXT_TOK(tk_left_parenth)
         
        NEED_NEXT_TOK(tk_word)

        int i;
        for (i = 1;i < build_type->subtype_count;i++) {
            loli_type *var_type = loli_ts_resolve_by_second(ts,
                    match_input_type, build_type->subtypes[i]);

            if (strcmp(lex->label, "_") == 0)
                loli_lexer(lex);
            else {
                loli_var *var = get_local_var(parser, var_type);
                loli_emit_decompose(parser->emit, match_sym, i - 1,
                        var->reg_spot);
            }

            if (i != build_type->subtype_count - 1) {
                NEED_CURRENT_TOK(tk_comma)
                NEED_NEXT_TOK(tk_word)
            }
        }
        NEED_CURRENT_TOK(tk_right_parenth)
    }
     

    NEED_NEXT_TOK(tk_colon)
    loli_lexer(lex);
}

static void error_incomplete_match(loli_parse_state *parser,
        loli_sym *match_sym)
{
    loli_class *match_class = match_sym->type->cls;
    int match_case_start = parser->emit->block->match_case_start;

    int i;
    loli_msgbuf *msgbuf = parser->raiser->aux_msgbuf;
    loli_named_sym *sym_iter = match_class->members;
    int *match_cases = parser->emit->match_cases + match_case_start;

    loli_mb_add(msgbuf,
            "Match pattern not exhaustive. The following case(s) are missing:");

    while (sym_iter) {
        if (sym_iter->item_kind == ITEM_TYPE_VARIANT) {
            for (i = match_case_start;i < parser->emit->match_case_pos;i++) {
                if (sym_iter->id == match_cases[i])
                    break;
            }

            if (i == parser->emit->match_case_pos)
                loli_mb_add_fmt(msgbuf, "\n* %s", sym_iter->name);
        }

        sym_iter = sym_iter->next;
    }

    loli_raise_syn(parser->raiser, loli_mb_raw(msgbuf));
}

static void match_case_class(loli_parse_state *parser,
        loli_sym *match_sym)
{
    loli_lex_state *lex = parser->lex;
    NEED_CURRENT_TOK(tk_word)
    loli_class *cls = resolve_class_name(parser);

    if (loli_class_greater_eq(match_sym->type->cls, cls) == 0) {
        loli_raise_syn(parser->raiser,
                "Class %s does not inherit from matching class %s.", cls->name,
                match_sym->type->cls->name);
    }

    if (loli_emit_is_duplicate_case(parser->emit, cls))
        loli_raise_syn(parser->raiser, "Already have a case for class %s.",
                cls->name);

    hide_block_vars(parser);
    loli_emit_change_match_branch(parser->emit);
    loli_emit_write_match_case(parser->emit, match_sym, cls);

     
    if (cls->generic_count != 0) {
        loli_raise_syn(parser->raiser,
                "Class matching only works for types without generics.",
                cls->name);
    }

    NEED_NEXT_TOK(tk_left_parenth)
    NEED_NEXT_TOK(tk_word)

    if (strcmp(lex->label, "_") == 0)
        loli_lexer(lex);
    else {
        loli_var *var = get_local_var(parser, cls->self_type);
        loli_emit_decompose(parser->emit, match_sym, 0, var->reg_spot);
    }

    NEED_CURRENT_TOK(tk_right_parenth)
    NEED_NEXT_TOK(tk_colon)
    loli_lexer(lex);
}

static void keyword_match(loli_parse_state *parser)
{
    loli_lex_state *lex = parser->lex;

    loli_emit_enter_block(parser->emit, block_match);

    expression(parser);
    loli_emit_eval_match_expr(parser->emit, parser->expr);

    NEED_CURRENT_TOK(tk_colon)
    NEED_NEXT_TOK(tk_left_curly)
    NEED_NEXT_TOK(tk_word)
    if (keyword_by_name(lex->label) != KEY_CASE)
        loli_raise_syn(parser->raiser, "'match' must start with a case.");

    loli_sym *match_sym = parser->expr->root->result;
    int is_enum = match_sym->type->cls->flags & CLS_IS_ENUM;
    int have_else = 0, case_count = 0, enum_case_max = 0;

    if (is_enum)
        enum_case_max = match_sym->type->cls->variant_size;

    while (1) {
        if (lex->token == tk_word) {
            int key = keyword_by_name(lex->label);
            if (key == KEY_CASE) {
                if (have_else)
                    loli_raise_syn(parser->raiser,
                            "'case' in exhaustive match.");

                loli_lexer(lex);
                if (is_enum)
                    match_case_enum(parser, match_sym);
                else
                    match_case_class(parser, match_sym);

                case_count++;
            }
            else if (key == KEY_ELSE) {
                if (have_else ||
                    (is_enum && case_count == enum_case_max))
                    loli_raise_syn(parser->raiser,
                            "'else' in exhaustive match.");

                NEED_NEXT_TOK(tk_colon)
                loli_emit_change_match_branch(parser->emit);
                loli_lexer(lex);
                have_else = 1;
            }
            else if (key != -1) {
                loli_lexer(lex);
                handlers[key](parser);
            }
            else {
                expression(parser);
                loli_emit_eval_expr(parser->emit, parser->expr);
            }
        }
        else if (lex->token != tk_right_curly)
            statement(parser, 0);
        else
            break;
    }

    if (have_else == 0) {
        if (is_enum == 0)
            loli_raise_syn(parser->raiser,
                    "Match against a class must have an 'else' case.");
        else if (case_count != enum_case_max)
            error_incomplete_match(parser, match_sym);
    }

    hide_block_vars(parser);
    loli_lexer(lex);
    loli_emit_leave_block(parser->emit);
}

static void keyword_case(loli_parse_state *parser)
{
    loli_raise_syn(parser->raiser, "'case' not allowed outside of 'match'.");
}

#define ANY_SCOPE (SYM_SCOPE_PRIVATE | SYM_SCOPE_PROTECTED | PUBLIC_SCOPE)

static void parse_define(loli_parse_state *parser, int modifiers)
{
    loli_block *block = parser->emit->block;
    if (block->block_type != block_file &&
        block->block_type != block_define &&
        block->block_type != block_class &&
        block->block_type != block_enum)
        loli_raise_syn(parser->raiser, "Cannot define a function here.");

    if (block->block_type == block_class &&
        (modifiers & ANY_SCOPE) == 0)
        loli_raise_syn(parser->raiser,
                "Class method declaration must start with a scope.");

    modifiers &= ~PUBLIC_SCOPE;

    loli_lex_state *lex = parser->lex;
    uint16_t save_generic_start = loli_gp_save(parser->generics);

    parse_define_header(parser, modifiers);

    NEED_CURRENT_TOK(tk_left_curly)

    if ((modifiers & VAR_IS_FUTURE) == 0) {
        parse_block_body(parser);
        hide_block_vars(parser);
        loli_emit_leave_call_block(parser->emit, lex->line_num);
    }
    else {
        NEED_NEXT_TOK(tk_three_dots)
        NEED_NEXT_TOK(tk_right_curly)
        loli_lexer(lex);
        hide_block_vars(parser);
        loli_emit_leave_future_call(parser->emit);
    }

    loli_gp_restore(parser->generics, save_generic_start);
}

#undef ANY_SCOPE

static void keyword_fn(loli_parse_state *parser)
{
    parse_define(parser, 0);
}

static void parse_modifier(loli_parse_state *parser, int key)
{
    loli_lex_state *lex = parser->lex;
    int modifiers = 0;

    if (key == KEY_FUTURE) {
        loli_block_type block_type = parser->emit->block->block_type;
        if (block_type != block_file && block_type != block_class)
            loli_raise_syn(parser->raiser,
                    "'future' qualifier is only for toplevel functions and methods.");

        modifiers |= VAR_IS_FUTURE;
        NEED_CURRENT_TOK(tk_word)
        key = keyword_by_name(lex->label);
    }

    if (key == KEY_PUBLIC ||
        key == KEY_PROTECTED ||
        key == KEY_PRIVATE) {

        if (parser->emit->block->block_type != block_class) {
            const char *name = "public";
            if (key == KEY_PROTECTED)
                name = "protected";
            else if (key == KEY_PRIVATE)
                name = "private";

            loli_raise_syn(parser->raiser, "'%s' is not allowed here.", name);
        }

        if (key == KEY_PUBLIC)
            modifiers |= PUBLIC_SCOPE;
        else if (key == KEY_PROTECTED)
            modifiers |= SYM_SCOPE_PROTECTED;
        else
            modifiers |= SYM_SCOPE_PRIVATE;

        if (modifiers & VAR_IS_FUTURE)
            loli_lexer(lex);

        NEED_CURRENT_TOK(tk_word)
        key = keyword_by_name(lex->label);
    }
    else if (modifiers & VAR_IS_FUTURE &&
             parser->emit->block->block_type == block_class) {
        loli_raise_syn(parser->raiser,
                "'future' must be followed by a class scope here.");
    }

    if (key == KEY_STATIC) {
        modifiers |= VAR_IS_STATIC;
        loli_lexer(lex);
        NEED_CURRENT_TOK(tk_word)
        key = keyword_by_name(lex->label);

        if (key != KEY_FN)
            loli_raise_syn(parser->raiser,
                    "'sta' must be followed by 'fn', not '%s'.",
                    lex->label);
    }

    if (key == KEY_VAR) {
        if (modifiers & VAR_IS_FUTURE)
            loli_raise_syn(parser->raiser, "Cannot use 'future' with 'var'.");

        loli_lexer(lex);
        parse_var(parser, modifiers);
    }
    else if (key == KEY_FN) {
        loli_lexer(lex);
        parse_define(parser, modifiers);
    }
    else {
        const char *what = "either 'var' or 'fn'";

        if (modifiers & VAR_IS_FUTURE)
            what = "'fn'";

        loli_raise_syn(parser->raiser, "Expected %s, but got '%s'.", what,
                lex->label);
    }
}

static void keyword_public(loli_parse_state *parser)
{
    parse_modifier(parser, KEY_PUBLIC);
}

static void keyword_static(loli_parse_state *parser)
{
    loli_raise_syn(parser->raiser,
            "'sta' must follow a scope (pub, pro, or pri).");
}

static void keyword_future(loli_parse_state *parser)
{
    parse_modifier(parser, KEY_FUTURE);
}

static void keyword_private(loli_parse_state *parser)
{
    parse_modifier(parser, KEY_PRIVATE);
}

static void keyword_protected(loli_parse_state *parser)
{
    parse_modifier(parser, KEY_PROTECTED);
}

static void maybe_fix_print(loli_parse_state *parser)
{
    loli_symtab *symtab = parser->symtab;
    loli_module_entry *builtin = symtab->builtin_module;
    loli_var *stdout_var = loli_find_var(symtab, builtin, "stdout");
    loli_vm_state *vm = parser->vm;

    if (stdout_var) {
        loli_var *print_var = loli_find_var(symtab, builtin, "print");
        if (print_var) {
             
            loli_value *print_value = vm->gs->readonly_table[print_var->reg_spot];
            loli_function_val *print_func = print_value->value.function;

            print_func->foreign_func = loli_stdout_print;
            print_func->cid_table = &stdout_var->reg_spot;
        }
    }
}

static void setup_and_exec_vm(loli_parse_state *parser)
{
     
    loli_register_classes(parser->symtab, parser->vm);
    loli_prepare_main(parser->emit, parser->toplevel_func);

    parser->vm->gs->readonly_table = parser->symtab->literals->data;

    maybe_fix_print(parser);
    update_all_cid_tables(parser);

    parser->executing = 1;
    loli_call_prepare(parser->vm, parser->toplevel_func);
     
    loli_stack_drop_top(parser->vm);
    loli_call(parser->vm, 0);

     
    parser->vm->call_chain = parser->vm->call_chain->prev;
    parser->vm->call_depth = 1;
    parser->executing = 0;

     
    loli_reset_main(parser->emit);
}

static void template_read_loop(loli_parse_state *parser, loli_lex_state *lex)
{
    loli_config *config = parser->config;
    int result = 0;

    do {
        char *buffer;
        result = loli_lexer_read_content(lex, &buffer);
        if (buffer[0])
            config->render_func(buffer, config->data);
    } while (result);
}

static void parser_loop(loli_parse_state *parser)
{
    loli_lex_state *lex = parser->lex;

    loli_lexer(lex);

    while (1) {
        if (lex->token == tk_word)
            statement(parser, 1);
        else if (lex->token == tk_right_curly)
             
            loli_emit_leave_block(parser->emit);
        else if (lex->token == tk_end_tag || lex->token == tk_eof) {
             
            if (parser->rendering == 0 && lex->token == tk_end_tag)
                loli_raise_syn(parser->raiser, "Unexpected token '%s'.",
                        tokname(lex->token));

            if (parser->emit->block->pending_future_decls)
                error_future_decl_pending(parser);

            setup_and_exec_vm(parser);

            if (lex->token == tk_end_tag)
                template_read_loop(parser, lex);

            if (lex->token == tk_eof)
                break;

            loli_lexer(lex);
        }
        else if (lex->token == tk_docstring) {
            process_docstring(parser);
        }
        else {
            expression(parser);
            loli_emit_eval_expr(parser->emit, parser->expr);
        }
    }
}

static void fix_first_file_name(loli_parse_state *parser,
        const char *filename)
{
    loli_module_entry *module = parser->main_module;

    if (module->const_path &&
        strcmp(module->const_path, filename) == 0)
        return;

    loli_free(module->dirname);

    module->const_path = filename;
    module->dirname = dir_from_path(filename);
    module->cmp_len = strlen(filename);
    module->root_dirname = module->dirname;
     

    parser->emit->protos->data[0]->module_path = filename;
}

static void build_error(loli_parse_state *parser)
{
    loli_raiser *raiser = parser->raiser;
    loli_msgbuf *msgbuf = loli_mb_flush(parser->msgbuf);
    loli_vm_state *vm = parser->vm;
    const char *msg = loli_mb_raw(raiser->msgbuf);

    if (vm->exception_cls)
        loli_mb_add(msgbuf, vm->exception_cls->name);
    else
        loli_mb_add(msgbuf, loli_name_for_error(raiser));

    if (msg[0] != '\0')
        loli_mb_add_fmt(msgbuf, ": %s\n", msg);
    else
        loli_mb_add_char(msgbuf, '\n');

    if (parser->executing == 0) {
        loli_lex_entry *iter = parser->lex->entry;
        if (iter) {
            int fixed_line_num = (raiser->line_adjust == 0 ?
                    parser->lex->line_num : raiser->line_adjust);

            loli_mb_add_fmt(msgbuf, "    from %s:%d:\n",
                    parser->symtab->active_module->path, fixed_line_num);
        }
    }
    else {
        loli_call_frame *frame = parser->vm->call_chain;

        loli_mb_add(msgbuf, "Traceback:\n");

        while (frame->prev) {
            loli_proto *proto = frame->function->proto;

            if (frame->function->code == NULL)
                loli_mb_add_fmt(msgbuf, "    from %s: in %s\n",
                        proto->module_path, proto->name);
            else
                loli_mb_add_fmt(msgbuf,
                        "    from %s:%d: in %s\n",
                        proto->module_path, frame->code[-1], proto->name);

            frame = frame->prev;
        }
    }
}

static FILE *load_file_to_parse(loli_parse_state *parser, const char *path)
{
    FILE *load_file = fopen(path, "r");
    if (load_file == NULL) {
         
        char buffer[128];
#ifdef _WIN32
        strerror_s(buffer, sizeof(buffer), errno);
#else
        strerror_r(errno, buffer, sizeof(buffer));
#endif
        loli_raise_err(parser->raiser, "Failed to open %s: (%s).", path,
                buffer);
    }

    return load_file;
}

static int open_first_content(loli_state *s, const char *filename,
        char *content)
{
    loli_parse_state *parser = s->gs->parser;

    if (parser->content_to_parse)
        return 0;

     
    if (setjmp(parser->raiser->all_jumps->jump) == 0) {
        loli_lex_entry_type load_type;
        void *load_content;

        if (content == NULL) {
            char *suffix = strrchr(filename, '.');
            if (suffix == NULL || strcmp(suffix, ".li") != 0)
                loli_raise_err(parser->raiser,
                        "File name must end with '.li'.");

            load_type = et_file;
            load_content = load_file_to_parse(parser, filename);
        }
        else {
            load_type = string_input_mode(parser);
            load_content = content;
        }

         
        handle_rewind(parser);
        loli_lexer_load(parser->lex, load_type, load_content);
         
        fix_first_file_name(parser, filename);

        parser->content_to_parse = 1;
        return 1;
    }

    return 0;
}

int loli_load_file(loli_state *s, const char *filename)
{
    return open_first_content(s, filename, NULL);
}

int loli_load_string(loli_state *s, const char *context,
        const char *str)
{
    return open_first_content(s, context, (char *)str);
}

int loli_parse_content(loli_state *s)
{
    loli_parse_state *parser = s->gs->parser;

    if (parser->content_to_parse == 0)
        return 0;

    parser->content_to_parse = 0;
    parser->rendering = 0;

    if (setjmp(parser->raiser->all_jumps->jump) == 0) {
        parser_loop(parser);
        loli_pop_lex_entry(parser->lex);
        loli_mb_flush(parser->msgbuf);

        return 1;
    }
    else
        parser->rs->pending = 1;

    return 0;
}

int loli_render_content(loli_state *s)
{
    loli_parse_state *parser = s->gs->parser;

    if (parser->content_to_parse == 0)
        return 0;

    parser->content_to_parse = 0;
    parser->rendering = 1;

    if (setjmp(parser->raiser->all_jumps->jump) == 0) {
        loli_verify_template(parser->lex);
         
        parser_loop(parser);
        loli_pop_lex_entry(parser->lex);
        loli_mb_flush(parser->msgbuf);
        return 1;
    }
    else
        parser->rs->pending = 1;

    return 0;
}

int loli_parse_expr(loli_state *s, const char **text)
{
    if (text)
        *text = NULL;

    loli_parse_state *parser = s->gs->parser;

    if (parser->content_to_parse == 0)
        return 0;

    parser->content_to_parse = 0;
    parser->rendering = 0;

    if (setjmp(parser->raiser->all_jumps->jump) == 0) {
        loli_lex_state *lex = parser->lex;

        loli_lexer(lex);
        expression(parser);
        loli_emit_eval_expr(parser->emit, parser->expr);
        NEED_CURRENT_TOK(tk_eof);

        loli_sym *sym = parser->expr->root->result;

        setup_and_exec_vm(parser);
        loli_pop_lex_entry(parser->lex);

        if (sym && text) {
             
            loli_value *reg = s->call_chain->next->start[sym->reg_spot];
            loli_msgbuf *msgbuf = loli_mb_flush(parser->msgbuf);

            loli_mb_add_fmt(msgbuf, "(^T): ", sym->type);

             
            if (reg->flags & V_STRING_FLAG && reg->value.string)
                loli_mb_add_fmt(msgbuf, "\"%s\"", reg->value.string->string);
            else
                loli_mb_add_value(msgbuf, s, reg);


            *text = loli_mb_raw(msgbuf);
        }

        return 1;
    }
    else {
        parser->rs->pending = 1;
    }

    return 0;
}

loli_function_val *loli_find_function(loli_vm_state *vm, const char *name)
{
     
    loli_var *v = loli_find_var(vm->gs->parser->symtab, NULL, name);
    loli_function_val *result;

    if (v)
        result = vm->gs->readonly_table[v->reg_spot]->value.function;
    else
        result = NULL;

    return result;
}

const char *loli_error_message(loli_state *s)
{
    build_error(s->gs->parser);
    return loli_mb_raw(s->gs->parser->msgbuf);
}

const char *loli_error_message_no_trace(loli_state *s)
{
    return loli_mb_raw(s->raiser->msgbuf);
}

loli_config *loli_config_get(loli_state *s)
{
    return s->gs->parser->config;
}
