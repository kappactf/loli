#include <string.h>
#include <stdint.h>
#include <limits.h>

#include "loli_alloc.h"
#include "loli_expr.h"
#include "loli_emitter.h"
#include "loli_parser.h"

#include "loli_int_opcode.h"
#include "loli_int_code_iter.h"

# define loli_raise_adjusted(r, adjust, message, ...) \
{ \
    r->line_adjust = adjust; \
    loli_raise_syn(r, message, __VA_ARGS__); \
}

extern loli_type *loli_question_type;
extern loli_class *loli_self_class;
extern loli_type *loli_unit_type;
extern loli_type *loli_unset_type;


static loli_proto_stack *new_proto_stack(int);
static void free_proto_stack(loli_proto_stack *);
static loli_storage_stack *new_storage_stack(int);
static void free_storage_stack(loli_storage_stack *);

loli_emit_state *loli_new_emit_state(loli_symtab *symtab, loli_raiser *raiser)
{
    loli_emit_state *emit = loli_malloc(sizeof(*emit));

    emit->patches = loli_new_buffer_u16(4);
    emit->match_cases = loli_malloc(sizeof(*emit->match_cases) * 4);
    emit->tm = loli_new_type_maker();
    emit->ts = loli_new_type_system(emit->tm);
    emit->code = loli_new_buffer_u16(32);
    emit->closure_aux_code = NULL;

    emit->closure_spots = loli_new_buffer_u16(4);

    emit->storages = new_storage_stack(4);
    emit->protos = new_proto_stack(4);

    emit->transform_table = NULL;
    emit->transform_size = 0;

    emit->expr_strings = loli_new_string_pile();
    emit->match_case_pos = 0;
    emit->match_case_size = 4;

    emit->block = NULL;

    emit->function_depth = 0;

    emit->raiser = raiser;
    emit->expr_num = 1;

    loli_block *main_block = loli_malloc(sizeof(*main_block));

    main_block->prev = NULL;
    main_block->next = NULL;
    main_block->block_type = block_file;
    main_block->class_entry = NULL;
    main_block->self = NULL;
    main_block->code_start = 0;
    main_block->next_reg_spot = 0;
    main_block->storage_start = 0;
    main_block->var_count = 0;
    main_block->flags = 0;
    main_block->pending_future_decls = 0;
    emit->block = main_block;
    emit->function_depth++;
    emit->main_block = main_block;
    emit->function_block = main_block;
    emit->class_block_depth = 0;

    return emit;
}

void loli_free_emit_state(loli_emit_state *emit)
{
    loli_block *current, *temp;
    current = emit->block;
    while (current && current->prev)
        current = current->prev;

    while (current) {
        temp = current->next;
        loli_free(current);
        current = temp;
    }

    free_storage_stack(emit->storages);
    free_proto_stack(emit->protos);

    loli_free_string_pile(emit->expr_strings);
    loli_free_type_maker(emit->tm);
    loli_free(emit->transform_table);
    loli_free_type_system(emit->ts);
    loli_free(emit->match_cases);
    if (emit->closure_aux_code)
        loli_free_buffer_u16(emit->closure_aux_code);
    loli_free_buffer_u16(emit->closure_spots);
    loli_free_buffer_u16(emit->patches);
    loli_free_buffer_u16(emit->code);
    loli_free(emit);
}


static loli_storage *get_storage(loli_emit_state *, loli_type *);
static loli_block *find_deepest_loop(loli_emit_state *);
static void inject_patch_into_block(loli_emit_state *, loli_block *, uint16_t);
static void eval_tree(loli_emit_state *, loli_ast *, loli_type *);

void loli_emit_write_import_call(loli_emit_state *emit, loli_var *var)
{
    loli_storage *s = get_storage(emit, loli_unit_type);

    loli_u16_write_5(emit->code, o_call_native, var->reg_spot, 0, s->reg_spot,
            *emit->lex_linenum);
}

void loli_emit_eval_optarg(loli_emit_state *emit, loli_ast *ast)
{
    eval_tree(emit, ast, NULL);
    emit->expr_num++;

    uint16_t patch_spot = loli_u16_pop(emit->patches);

     
    loli_u16_set_at(emit->code, patch_spot,
            loli_u16_pos(emit->code) - patch_spot + 1);
}

void loli_emit_eval_optarg_keyed(loli_emit_state *emit, loli_ast *ast)
{
     
    uint16_t target_reg = ast->left->sym->reg_spot;

    loli_u16_write_3(emit->code, o_jump_if_set, target_reg, 2);
    loli_u16_write_1(emit->patches, loli_u16_pos(emit->code) - 1);

    eval_tree(emit, ast, NULL);
    emit->expr_num++;

    uint16_t patch_spot = loli_u16_pop(emit->patches);

     
    loli_u16_set_at(emit->code, patch_spot,
            loli_u16_pos(emit->code) - patch_spot + 2);
}

void loli_emit_write_keyless_optarg_header(loli_emit_state *emit,
        loli_type *type)
{
     
    int i;
    for (i = type->subtype_count - 1;i > 0;i--) {
        loli_type *inner = type->subtypes[i];
        if (inner->cls->id != LOLI_ID_OPTARG)
            break;
    }

    int patch_start = loli_u16_pos(emit->patches);
    uint16_t first_reg = (uint16_t)i;

    i = type->subtype_count - i - 1;

    for (;i > 0;i--, first_reg++) {
         
        loli_u16_write_3(emit->code, o_jump_if_set, first_reg, 5);

         
        loli_u16_write_2(emit->code, o_jump, 1);
        loli_u16_inject(emit->patches, patch_start,
                loli_u16_pos(emit->code) - 1);
    }

     
    loli_u16_write_2(emit->code, o_jump, 1);
    loli_u16_inject(emit->patches, patch_start,
            loli_u16_pos(emit->code) - 1);

     

    uint16_t patch_spot = loli_u16_pop(emit->patches);
    loli_u16_set_at(emit->code, patch_spot,
            loli_u16_pos(emit->code) - patch_spot + 1);
}

void loli_emit_write_class_header(loli_emit_state *emit, loli_type *self_type,
        uint16_t line_num)
{
    loli_storage *self = get_storage(emit, self_type);

    emit->function_block->self = self;
    loli_u16_write_4(emit->code, o_instance_new, self_type->cls->id,
            self->reg_spot, line_num);
}

void loli_emit_write_shorthand_ctor(loli_emit_state *emit, loli_class *cls,
        loli_var *var_iter, uint16_t line_num)
{
    loli_named_sym *prop_iter = cls->members;
    uint16_t self_reg_spot = emit->function_block->self->reg_spot;

     

    while (prop_iter->item_kind == ITEM_TYPE_PROPERTY) {
        while (strcmp(var_iter->name, "") != 0)
            var_iter = var_iter->next;

        loli_u16_write_5(emit->code, o_property_set, prop_iter->reg_spot,
                self_reg_spot, var_iter->reg_spot, *emit->lex_linenum);

        prop_iter->flags &= ~SYM_NOT_INITIALIZED;
        var_iter = var_iter->next;
        prop_iter = prop_iter->next;
    }
}

void loli_emit_finalize_for_in(loli_emit_state *emit, loli_var *user_loop_var,
        loli_var *for_start, loli_var *for_end, loli_sym *for_step,
        int line_num)
{
    loli_sym *target;
    int need_sync = user_loop_var->flags & VAR_IS_GLOBAL;

    if (need_sync) {
        loli_class *cls = emit->symtab->integer_class;
         
        target = (loli_sym *)get_storage(emit, cls->self_type);
    }
    else
        target = (loli_sym *)user_loop_var;

    loli_u16_write_6(emit->code, o_for_setup, for_start->reg_spot,
            for_end->reg_spot, for_step->reg_spot, target->reg_spot, line_num);

    if (need_sync) {
        loli_u16_write_4(emit->code, o_global_set, target->reg_spot,
                user_loop_var->reg_spot, line_num);
    }

     
    emit->block->code_start = loli_u16_pos(emit->code);

    loli_u16_write_5(emit->code, o_for_integer, for_start->reg_spot,
            for_end->reg_spot, for_step->reg_spot, target->reg_spot);

    loli_u16_write_2(emit->code, 5, line_num);

    loli_u16_write_1(emit->patches, loli_u16_pos(emit->code) - 2);

    if (need_sync) {
        loli_u16_write_4(emit->code, o_global_set, target->reg_spot,
                user_loop_var->reg_spot, line_num);
    }
}

static void write_pop_try_blocks_up_to(loli_emit_state *emit,
        loli_block *stop_block)
{
    loli_block *block_iter = emit->block;
    int try_count = 0;

    while (block_iter != stop_block) {
        if (block_iter->block_type == block_try)
            try_count++;

        block_iter = block_iter->prev;
    }

    if (try_count) {
        int i;
        for (i = 0;i < try_count;i++)
            loli_u16_write_1(emit->code, o_catch_pop);
    }
}

void loli_emit_break(loli_emit_state *emit)
{
    loli_block *loop_block = find_deepest_loop(emit);

    if (loop_block == NULL)
        loli_raise_syn(emit->raiser, "'break' used outside of a loop.");

    write_pop_try_blocks_up_to(emit, loop_block);

     
    loli_u16_write_2(emit->code, o_jump, 1);

    inject_patch_into_block(emit, loop_block, loli_u16_pos(emit->code) - 1);
}

void loli_emit_continue(loli_emit_state *emit)
{
    loli_block *loop_block = find_deepest_loop(emit);

    if (loop_block == NULL)
        loli_raise_syn(emit->raiser, "'continue' used outside of a loop.");

    write_pop_try_blocks_up_to(emit, loop_block);

    int where = loop_block->code_start - loli_u16_pos(emit->code);
    loli_u16_write_2(emit->code, o_jump, (uint16_t)where);
}

void loli_emit_try(loli_emit_state *emit, int line_num)
{
    loli_u16_write_3(emit->code, o_catch_push, 1, line_num);

    loli_u16_write_1(emit->patches, loli_u16_pos(emit->code) - 2);
}

void loli_emit_except(loli_emit_state *emit, loli_type *except_type,
        loli_var *except_var, int line_num)
{
    loli_u16_write_4(emit->code, o_exception_catch, except_type->cls->id, 2,
            line_num);
    loli_u16_write_1(emit->patches, loli_u16_pos(emit->code) - 2);

    if (except_var)
        loli_u16_write_2(emit->code, o_exception_store, except_var->reg_spot);
}

static void emit_jump_if(loli_emit_state *emit, loli_ast *ast, int jump_on)
{
    loli_u16_write_4(emit->code, o_jump_if, jump_on, ast->result->reg_spot, 3);

    loli_u16_write_1(emit->patches, loli_u16_pos(emit->code) - 1);
}

static void write_patches_since(loli_emit_state *emit, int to)
{
    int from = emit->patches->pos - 1;
    int pos = loli_u16_pos(emit->code);

    for (;from >= to;from--) {
        uint16_t patch = loli_u16_pop(emit->patches);

         
        if (patch != 0) {
            int adjust = loli_u16_get(emit->code, patch);
            loli_u16_set_at(emit->code, patch, pos + adjust - patch);
        }
    }
}


static loli_storage *new_storage(void)
{
    loli_storage *result = loli_malloc(sizeof(*result));

    result->type = NULL;
    result->expr_num = 0;
    result->flags = 0;
    result->item_kind = ITEM_TYPE_STORAGE;

    return result;
}

static loli_storage_stack *new_storage_stack(int initial)
{
    loli_storage_stack *result = loli_malloc(sizeof(*result));
    result->data = loli_malloc(initial * sizeof(*result->data));
    int i;
    for (i = 0;i < initial;i++) {
        loli_storage *s = new_storage();

        result->data[i] = s;
    }

    result->scope_end = 0;
    result->size = initial;

    return result;
}

static void free_storage_stack(loli_storage_stack *stack)
{
    int i;
    for (i = 0;i < stack->size;i++) {
        loli_free(stack->data[i]);
    }

    loli_free(stack->data);
    loli_free(stack);
}

static void grow_storages(loli_storage_stack *stack)
{
    int i;
    int new_size = stack->size * 2;
    loli_storage **new_data = loli_realloc(stack->data,
            sizeof(*new_data) * stack->size * 2);

     
    for (i = stack->size;i < new_size;i++)
        new_data[i] = new_storage();

    stack->data = new_data;
    stack->size = new_size;
}

static loli_storage *get_storage(loli_emit_state *emit, loli_type *type)
{
    loli_storage_stack *stack = emit->storages;
    int expr_num = emit->expr_num;
    int i;
    loli_storage *s = NULL;

    for (i = emit->function_block->storage_start;
         i < stack->size;
         i++) {
        s = stack->data[i];

         
        if (s->type == NULL) {
            s->type = type;

            s->reg_spot = emit->function_block->next_reg_spot;
            emit->function_block->next_reg_spot++;

            i++;
            if (i == stack->size)
                grow_storages(emit->storages);

             
            stack->scope_end = i;

            break;
        }
        else if (s->type == type &&
                 s->expr_num != expr_num) {
            s->expr_num = expr_num;
            break;
        }
    }

    s->expr_num = expr_num;
    s->flags &= ~SYM_NOT_ASSIGNABLE;

    return s;
}

loli_storage *get_unique_storage(loli_emit_state *emit, loli_type *type)
{
    int next_spot = emit->function_block->next_reg_spot;
    loli_storage *s = NULL;

    do {
        s = get_storage(emit, type);
    } while (emit->function_block->next_reg_spot == next_spot);

    return s;
}


static void inject_patch_into_block(loli_emit_state *, loli_block *, uint16_t);
static void write_final_code_for_block(loli_emit_state *, loli_block *);


static loli_block *block_enter_common(loli_emit_state *emit)
{
    loli_block *new_block;
    if (emit->block->next == NULL) {
        new_block = loli_malloc(sizeof(*new_block));

        emit->block->next = new_block;
        new_block->prev = emit->block;
        new_block->next = NULL;
    }
    else
        new_block = emit->block->next;

    new_block->class_entry = emit->block->class_entry;
    new_block->self = NULL;
    new_block->patch_start = emit->patches->pos;
    new_block->last_exit = -1;
    new_block->flags = 0;
    new_block->var_count = 0;
    new_block->code_start = loli_u16_pos(emit->code);
    new_block->pending_future_decls = 0;

    return new_block;
}

void loli_emit_enter_block(loli_emit_state *emit, loli_block_type block_type)
{
    loli_block *new_block = block_enter_common(emit);
    new_block->block_type = block_type;
    new_block->flags |= BLOCK_ALWAYS_EXITS;

    if (block_type == block_enum) {
         
        new_block->class_entry = emit->symtab->active_module->class_chain;
         
        emit->function_depth++;
    }

    emit->block = new_block;
}

void loli_emit_enter_call_block(loli_emit_state *emit,
        loli_block_type block_type, loli_var *call_var)
{
    loli_block *new_block = block_enter_common(emit);
    new_block->block_type = block_type;

    if (block_type == block_class) {
        new_block->class_entry = emit->symtab->active_module->class_chain;
        emit->class_block_depth = emit->function_depth + 1;
    }

     
    if (emit->block->block_type == block_define)
        call_var->flags |= VAR_NEEDS_CLOSURE;

     
    if (block_type != block_file)
        emit->function_depth++;

    new_block->prev_function_block = emit->function_block;

    emit->function_block = new_block;

    new_block->next_reg_spot = 0;
    new_block->storage_start = emit->storages->scope_end;
    new_block->function_var = call_var;
    new_block->code_start = loli_u16_pos(emit->code);

    emit->block = new_block;
}

void loli_emit_leave_future_call(loli_emit_state *emit)
{
     
    emit->block = emit->block->prev;
    emit->function_block = emit->block;
    emit->function_depth--;
    emit->block->pending_future_decls++;
}

void loli_emit_resolve_future_decl(loli_emit_state *emit, loli_var *var)
{
    var->flags &= ~VAR_IS_FUTURE;
    emit->block->pending_future_decls--;
}

void loli_emit_leave_call_block(loli_emit_state *emit, uint16_t line_num)
{
    loli_block *block = emit->block;

    if (block->block_type == block_class)
        loli_u16_write_3(emit->code, o_return_value, block->self->reg_spot,
                line_num);
    else if (block->last_exit != loli_u16_pos(emit->code)) {
        loli_type *type = block->function_var->type->subtypes[0];

        if (type == loli_unit_type)
            loli_u16_write_2(emit->code, o_return_unit, line_num);
        else if (type == loli_self_class->self_type)
             
            loli_u16_write_3(emit->code, o_return_value, 0, line_num);
        else
            loli_raise_syn(emit->raiser,
                    "Missing return statement at end of function.");
    }

    write_final_code_for_block(emit, block);

    int i;
    for (i = block->storage_start;i < emit->storages->scope_end;i++)
        emit->storages->data[i]->type = NULL;

    emit->storages->scope_end = block->storage_start;

    if (emit->block->block_type == block_class)
        emit->class_block_depth = 0;

    emit->function_block = block->prev_function_block;

     
    if (block->block_type != block_file)
        emit->function_depth--;

    emit->block = emit->block->prev;
}

void loli_emit_leave_block(loli_emit_state *emit)
{
    loli_block *block;
    int block_type;

    if (emit->block->prev == NULL)
        loli_raise_syn(emit->raiser, "'}' outside of a block.");

    block = emit->block;
    block_type = block->block_type;

     
    if (block_type == block_while || block_type == block_for_in) {
        int x = block->code_start - loli_u16_pos(emit->code);
        loli_u16_write_2(emit->code, o_jump, (uint16_t)x);
    }
    else if (block_type == block_match)
        emit->match_case_pos = emit->block->match_case_start;
    else if (block_type == block_try ||
             block_type == block_try_except ||
             block_type == block_try_except_all) {
         
        loli_u16_set_at(emit->code, loli_u16_pop(emit->patches), 0);
    }
    else if (block_type == block_enum)
         
        emit->function_depth--;

    if ((block_type == block_if_else ||
         block_type == block_match ||
         block_type == block_try_except_all) &&
        block->flags & BLOCK_ALWAYS_EXITS &&
        block->last_exit == loli_u16_pos(emit->code)) {
        emit->block->prev->last_exit = loli_u16_pos(emit->code);
    }

    write_patches_since(emit, block->patch_start);
    emit->block = emit->block->prev;
}

static loli_block *find_deepest_loop(loli_emit_state *emit)
{
    loli_block *block, *ret;
    ret = NULL;

    for (block = emit->block; block; block = block->prev) {
        if (block->block_type == block_while ||
            block->block_type == block_do_while ||
            block->block_type == block_for_in) {
            ret = block;
            break;
        }
        else if (block->block_type >= block_define) {
            ret = NULL;
            break;
        }
    }

    return ret;
}

static void inject_patch_into_block(loli_emit_state *emit, loli_block *block,
        uint16_t patch)
{
     
    if (emit->block == block)
        loli_u16_write_1(emit->patches, patch);
    else {
        loli_u16_inject(emit->patches, block->next->patch_start, patch);

         
        for (block = block->next; block; block = block->next)
            block->patch_start++;
    }
}


void loli_emit_change_block_to(loli_emit_state *emit, int new_type)
{
    loli_block *block = emit->block;
    loli_block_type current_type = block->block_type;

    if (block->last_exit != loli_u16_pos(emit->code))
        block->flags &= ~BLOCK_ALWAYS_EXITS;

    if (new_type == block_try_except || new_type == block_try_except_all) {
        if (current_type == block_try_except_all)
            loli_raise_syn(emit->raiser, "'except' clause is unreachable.");

         
        if (current_type == block_try)
            loli_u16_write_1(emit->code, o_catch_pop);
    }

    int save_jump;

    if (block->last_exit != loli_u16_pos(emit->code)) {
         
        loli_u16_write_2(emit->code, o_jump, 1);
        save_jump = loli_u16_pos(emit->code) - 1;
    }
    else
         
        save_jump = -1;

     
    uint16_t patch = loli_u16_pop(emit->patches);

    if (patch != 0) {
        int patch_adjust = loli_u16_get(emit->code, patch);
        loli_u16_set_at(emit->code, patch,
                loli_u16_pos(emit->code) + patch_adjust - patch);
    }
     

    if (save_jump != -1)
        loli_u16_write_1(emit->patches, save_jump);

    emit->block->block_type = new_type;
}



static void close_over_sym(loli_emit_state *emit, uint16_t depth, loli_sym *sym)
{
    loli_u16_write_2(emit->closure_spots, sym->reg_spot, depth);
    emit->function_block->flags |= BLOCK_MAKE_CLOSURE;
}

static void emit_create_function(loli_emit_state *emit, loli_sym *func_sym,
        loli_storage *target)
{
    loli_u16_write_4(emit->code, o_closure_function, func_sym->reg_spot,
            target->reg_spot, *emit->lex_linenum);
    emit->function_block->flags |= BLOCK_MAKE_CLOSURE;
}

static uint16_t checked_close_over_var(loli_emit_state *emit, loli_var *var)
{
    if (emit->function_block->block_type == block_define &&
        emit->function_block->prev->block_type == block_define &&
        var->type->flags & TYPE_IS_UNRESOLVED)
        loli_raise_syn(emit->raiser,
                "Cannot close over a var of an incomplete type in this scope.");

    if (var->function_depth == emit->class_block_depth)
        loli_raise_syn(emit->raiser,
                "Not allowed to close over variables from a class constructor.",
                "");

    close_over_sym(emit, var->function_depth, (loli_sym *)var);
    return (loli_u16_pos(emit->closure_spots) - 1) / 2;
}

static int find_closed_sym_spot_raw(loli_emit_state *emit, uint16_t depth,
        uint16_t spot)
{
    int result = -1, i;

    for (i = 0;
         i < loli_u16_pos(emit->closure_spots);
         i += 2) {
        if (loli_u16_get(emit->closure_spots, i) == spot &&
            loli_u16_get(emit->closure_spots, i + 1) == depth) {
            result = i / 2;
            break;
        }
    }

    return result;
}

#define find_closed_sym_spot(emit, depth, sym) \
find_closed_sym_spot_raw(emit, depth, (sym)->reg_spot)

static void close_over_class_self(loli_emit_state *emit, loli_ast *ast)
{
     
    uint16_t depth = 3;
    loli_block *block = emit->function_block->prev;

    while (block->block_type != block_class &&
           block->block_type != block_enum)
        block = block->prev;

    block = block->next;

    if (block->block_type != block_define) {
        loli_raise_adjusted(emit->raiser, ast->line_num,
                "Not allowed to close over self in a class constructor.",
                "");
    }

    if (block->self == NULL) {
        loli_raise_adjusted(emit->raiser, ast->line_num,
                "Static methods do not have access to self.", "");
    }

    loli_sym *upper_self = (loli_sym *)block->self;

    if (find_closed_sym_spot(emit, depth, upper_self) == -1)
        close_over_sym(emit, depth, upper_self);

    emit->function_block->self = get_storage(emit, upper_self->type);
    emit->function_block->flags |= BLOCK_MAKE_CLOSURE;
}

static void setup_for_transform(loli_emit_state *emit,
        loli_function_val *f, int is_backing)
{
    int next_reg_spot = emit->function_block->next_reg_spot;

    if (emit->transform_size < emit->function_block->next_reg_spot) {
        emit->transform_table = loli_realloc(emit->transform_table,
                next_reg_spot * sizeof(*emit->transform_table));
        emit->transform_size = emit->function_block->next_reg_spot;
    }

    memset(emit->transform_table, (uint16_t)-1,
            next_reg_spot * sizeof(*emit->transform_table));

    loli_var *func_var = emit->function_block->function_var;
    uint16_t line_num = func_var->line_num;
    uint16_t local_count = func_var->type->subtype_count - 1;
    int i, count = 0;

    for (i = 0;
         i < loli_u16_pos(emit->closure_spots);
         i += 2) {
        if (loli_u16_get(emit->closure_spots, i + 1) == emit->function_depth) {
            uint16_t spot = loli_u16_get(emit->closure_spots, i);
            if (spot < local_count) {
                 
                loli_u16_write_4(emit->closure_aux_code, o_closure_set, i / 2,
                        spot, line_num);
            }

            emit->transform_table[spot] = i / 2;
            count++;
             
            loli_u16_set_at(emit->closure_spots, i + 1, (uint16_t)-1);
        }
    }
     
    if (is_backing == 0 && count) {
        uint16_t *locals = loli_malloc((count + 1) * sizeof(*locals));

        locals[0] = count + 1;

        int pos = 1;
        for (i = 0;i < next_reg_spot;i++) {
            if (emit->transform_table[i] != (uint16_t) -1) {
                locals[pos] = i;
                pos++;
            }
        }

        f->proto->locals = locals;
    }
}

static void maybe_add_jump(loli_buffer_u16 *buffer, int i, int dest)
{
    int end = loli_u16_pos(buffer);

    for (;i < end;i += 2) {
        int jump = loli_u16_get(buffer, i);

         
        if (jump > dest) {
            loli_u16_inject(buffer, i, 0);
            loli_u16_inject(buffer, i, dest);
            return;
        }
        else if (jump == dest)
            return;
    }

    loli_u16_write_2(buffer, dest, 0);
}

static int count_transforms(loli_emit_state *emit, int start)
{
    loli_code_iter ci;
    loli_ci_init(&ci, emit->code->data, start, loli_u16_pos(emit->code));
    loli_ci_next(&ci);
    uint16_t *buffer = ci.buffer;
    uint16_t *transform_table = emit->transform_table;
    loli_opcode op = buffer[ci.offset];
    int pos = ci.offset + 1;
    int count = 0;

    if (op == o_call_register &&
        transform_table[buffer[pos]] != (uint16_t)-1)
        count++;

    pos += ci.special_1 + ci.counter_2;

    if (ci.inputs_3) {
        int i;
        for (i = 0;i < ci.inputs_3;i++) {
            if (transform_table[buffer[pos + i]] != (uint16_t)-1)
                count++;
        }
    }

    return count;
}

uint16_t iter_for_first_line(loli_emit_state *emit, int pos)
{
    uint16_t result = 0;
    loli_code_iter ci;
    loli_ci_init(&ci, emit->code->data, pos, loli_u16_pos(emit->code));

    while (loli_ci_next(&ci)) {
        if (ci.line_6) {
            pos = ci.offset + ci.round_total - ci.line_6;
            result = ci.buffer[pos];
            break;
        }
    }

    return result;
}

static void perform_closure_transform(loli_emit_state *emit,
        loli_block *function_block, loli_function_val *f)
{
    if (emit->closure_aux_code == NULL)
        emit->closure_aux_code = loli_new_buffer_u16(8);
    else
        loli_u16_set_pos(emit->closure_aux_code, 0);

    int iter_start = emit->block->code_start;
    uint16_t first_line = iter_for_first_line(emit, iter_start);
    loli_block *prev_block = function_block->prev_function_block;
    int is_backing = (prev_block->block_type == block_class ||
                      prev_block->block_type == block_file ||
                      function_block->prev->block_type == block_enum);

    if (is_backing) {
         
        loli_storage *s = get_unique_storage(emit,
                function_block->function_var->type);

        loli_u16_write_4(emit->closure_aux_code, o_closure_new,
                loli_u16_pos(emit->closure_spots) / 2, s->reg_spot,
                first_line);

         
        if (function_block->self && emit->function_depth == 3) {
             
            uint16_t self_spot = find_closed_sym_spot_raw(emit, 3, 0);
             
            if (self_spot != (uint16_t)-1) {
                loli_u16_write_4(emit->closure_aux_code, o_closure_set,
                        self_spot, 0, first_line);
            }
        }
    }
    else if (emit->block->self) {
        loli_storage *block_self = emit->block->self;

        while (prev_block->block_type != block_class &&
               prev_block->block_type != block_enum)
             
            prev_block = prev_block->prev;

        prev_block = prev_block->next;

         

         
        uint16_t self_spot = find_closed_sym_spot(emit, 3,
                (loli_sym *)prev_block->self);

        loli_u16_write_4(emit->closure_aux_code, o_closure_get,
                self_spot, block_self->reg_spot, first_line);
    }

    setup_for_transform(emit, f, is_backing);

    if (is_backing)
        loli_u16_set_pos(emit->closure_spots, 0);

    loli_code_iter ci;
    loli_ci_init(&ci, emit->code->data, iter_start, loli_u16_pos(emit->code));
    uint16_t *transform_table = emit->transform_table;
    int jump_adjust = 0;

#define MAYBE_TRANSFORM_INPUT(x, z) \
{ \
    uint16_t id = transform_table[buffer[x]]; \
    if (id != (uint16_t)-1) { \
        loli_u16_write_4(emit->closure_aux_code, z, id, \
                buffer[x], first_line); \
        jump_adjust += 4; \
    } \
}

    uint16_t *buffer = ci.buffer;
    uint16_t patch_start = loli_u16_pos(emit->patches);
    int i, pos;

     
    while (loli_ci_next(&ci)) {
        if (ci.jumps_5) {
            int stop = ci.offset + ci.round_total - ci.line_6;

            for (i = stop - ci.jumps_5;i < stop;i++) {
                int jump = (int16_t)buffer[i];
                 
                if (jump == 0)
                    continue;

                maybe_add_jump(emit->patches, patch_start, ci.offset + jump);
            }
        }
    }

     
    loli_u16_write_2(emit->patches, UINT16_MAX, 0);

    uint16_t patch_stop = loli_u16_pos(emit->patches);
    uint16_t patch_iter = patch_start;
    uint16_t next_jump = loli_u16_get(emit->patches, patch_iter);

    loli_ci_init(&ci, emit->code->data, iter_start, loli_u16_pos(emit->code));
    while (loli_ci_next(&ci)) {
        int output_start = 0;

        loli_opcode op = buffer[ci.offset];
         
        pos = ci.offset + 1;

        if (ci.special_1) {
            switch (op) {
                case o_call_register:
                    MAYBE_TRANSFORM_INPUT(pos, o_closure_get)
                default:
                    pos += ci.special_1;
                    break;
            }
        }

        pos += ci.counter_2;

        if (ci.inputs_3) {
            for (i = 0;i < ci.inputs_3;i++) {
                MAYBE_TRANSFORM_INPUT(pos + i, o_closure_get)
            }

            pos += ci.inputs_3;
        }

        if (ci.outputs_4) {
            output_start = pos;
            pos += ci.outputs_4;
        }

        i = ci.offset;
        if (i == next_jump) {
             
            loli_u16_set_at(emit->patches, patch_iter + 1,
                    loli_u16_pos(emit->closure_aux_code));
            patch_iter += 2;
            next_jump = loli_u16_get(emit->patches, patch_iter);
        }

        int stop = ci.offset + ci.round_total - ci.jumps_5 - ci.line_6;
        for (;i < stop;i++)
            loli_u16_write_1(emit->closure_aux_code, buffer[i]);

        if (ci.jumps_5) {
            for (i = 0;i < ci.jumps_5;i++) {
                 
                int distance = (int16_t)buffer[stop + i];

                 
                if (distance) {
                    int destination = ci.offset + distance;

                     
                    loli_u16_write_2(emit->patches,
                            loli_u16_pos(emit->closure_aux_code),
                            ci.round_total - ci.jumps_5 - ci.line_6 + i);

                    loli_u16_write_1(emit->closure_aux_code, destination);
                }
                else
                    loli_u16_write_1(emit->closure_aux_code, 0);
            }
        }

        if (ci.line_6)
            loli_u16_write_1(emit->closure_aux_code, buffer[pos]);

        if (ci.outputs_4) {
            int stop = output_start + ci.outputs_4;

            for (i = output_start;i < stop;i++) {
                MAYBE_TRANSFORM_INPUT(i, o_closure_set)
            }
        }
    }

     
    int j;
    for (j = patch_stop;j < loli_u16_pos(emit->patches);j += 2) {
         
        int aux_pos = loli_u16_get(emit->patches, j);
         
        int original = loli_u16_get(emit->closure_aux_code, aux_pos);
        int k;

        for (k = patch_start;k < patch_stop;k += 2) {
            if (original == loli_u16_get(emit->patches, k)) {
                int tx_offset = count_transforms(emit, original) * 4;

                 
                int new_jump =
                         
                        loli_u16_get(emit->patches, k + 1)
                         
                        - aux_pos
                         
                        + loli_u16_get(emit->patches, j + 1)
                         
                        - tx_offset;

                loli_u16_set_at(emit->closure_aux_code, aux_pos,
                        (int16_t)new_jump);
                break;
            }
        }
    }

    loli_u16_set_pos(emit->patches, patch_start);
}

static void write_final_code_for_block(loli_emit_state *emit,
        loli_block *function_block)
{
    loli_var *var = function_block->function_var;
    loli_value *v = loli_vs_nth(emit->symtab->literals, var->reg_spot);
    loli_function_val *f = v->value.function;

    int code_start, code_size;
    uint16_t *source, *code;

    if ((function_block->flags & BLOCK_MAKE_CLOSURE) == 0) {
        code_start = emit->block->code_start;
        code_size = loli_u16_pos(emit->code) - emit->block->code_start;

        source = emit->code->data;
    }
    else {
        loli_block *prev = function_block->prev_function_block;

        perform_closure_transform(emit, function_block, f);

        if (prev->block_type != block_file)
            prev->flags |= BLOCK_MAKE_CLOSURE;

        code_start = 0;
        code_size = loli_u16_pos(emit->closure_aux_code);
        source = emit->closure_aux_code->data;
    }

    code = loli_malloc((code_size + 1) * sizeof(*code));
    memcpy(code, source + code_start, sizeof(*code) * code_size);

    f->code_len = code_size;
    f->code = code;
    f->proto->code = code;
    f->reg_count = function_block->next_reg_spot;

    loli_u16_set_pos(emit->code, function_block->code_start);
}


static void eval_enforce_value(loli_emit_state *, loli_ast *, loli_type *,
        const char *);


static void grow_match_cases(loli_emit_state *emit)
{
    emit->match_case_size *= 2;
    emit->match_cases = loli_realloc(emit->match_cases,
        sizeof(*emit->match_cases) * emit->match_case_size);
}

void loli_emit_decompose(loli_emit_state *emit, loli_sym *match_sym, int index,
        uint16_t pos)
{
     

    if (match_sym->type->cls->flags & CLS_IS_ENUM)
        loli_u16_write_5(emit->code, o_property_get, index, match_sym->reg_spot,
                pos, *emit->lex_linenum);
    else
        loli_u16_write_4(emit->code, o_assign, match_sym->reg_spot, pos,
                *emit->lex_linenum);
}

int loli_emit_is_duplicate_case(loli_emit_state *emit, loli_class *cls)
{
    if (emit->match_case_pos >= emit->match_case_size)
        grow_match_cases(emit);

    loli_block *block = emit->block;
    int cls_id = cls->id, ret = 0;
    int i;

    for (i = block->match_case_start;i < emit->match_case_pos;i++) {
        if (emit->match_cases[i] == cls_id) {
            ret = 1;
            break;
        }
    }

    return ret;
}

void loli_emit_write_match_case(loli_emit_state *emit, loli_sym *match_sym,
        loli_class *cls)
{
    emit->match_cases[emit->match_case_pos] = cls->id;
    emit->match_case_pos++;

    loli_u16_write_4(emit->code, o_jump_if_not_class, cls->id,
            match_sym->reg_spot, 3);

    loli_u16_write_1(emit->patches, loli_u16_pos(emit->code) - 1);
}

void loli_emit_change_match_branch(loli_emit_state *emit)
{
    loli_block *block = emit->block;

    if (block->match_case_start != emit->match_case_pos) {
        if (emit->block->last_exit != loli_u16_pos(emit->code))
            emit->block->flags &= ~BLOCK_ALWAYS_EXITS;

         
        int pos = loli_u16_pop(emit->patches);
        int adjust = loli_u16_get(emit->code, pos);

         
        loli_u16_write_2(emit->code, o_jump, 1);
        loli_u16_write_1(emit->patches, loli_u16_pos(emit->code) - 1);

         
        loli_u16_set_at(emit->code, pos,
                loli_u16_pos(emit->code) + adjust - pos);
    }
}

void loli_emit_eval_match_expr(loli_emit_state *emit, loli_expr_state *es)
{
    loli_ast *ast = es->root;
    loli_block *block = emit->block;
    eval_enforce_value(emit, ast, NULL, "Match expression has no value.");

    block->match_case_start = emit->match_case_pos;

    loli_class *match_class = ast->result->type->cls;

    if ((match_class->flags & CLS_IS_ENUM) == 0 &&
        (match_class->flags & CLS_IS_BUILTIN))
        loli_raise_syn(emit->raiser,
                "Match expression is not a user class or enum.");

     
    loli_u16_write_1(emit->patches, 0);
}



static loli_proto_stack *new_proto_stack(int initial)
{
    loli_proto_stack *result = loli_malloc(sizeof(*result));

    result->data = loli_malloc(initial * sizeof(*result->data));
    result->pos = 0;
    result->size = initial;

    return result;
}

static void free_proto_stack(loli_proto_stack *stack)
{
    int i;
     
    for (i = 0;i < stack->pos;i++) {
        loli_proto *p = stack->data[i];
        loli_free(p->name);
        loli_free(p->locals);
        loli_free(p->code);
        loli_free(p->arg_names);
        loli_free(p);
    }

    loli_free(stack->data);
    loli_free(stack);
}

static void grow_protos(loli_proto_stack *stack)
{
    int new_size = stack->size * 2;
    loli_proto **new_data = loli_realloc(stack->data,
            sizeof(*new_data) * stack->size * 2);

    stack->data = new_data;
    stack->size = new_size;
}

loli_proto *loli_emit_new_proto(loli_emit_state *emit, const char *module_path,
        const char *class_name, const char *name)
{
    loli_proto_stack *protos = emit->protos;

    if (protos->pos == protos->size)
        grow_protos(protos);

    loli_proto *p = loli_malloc(sizeof(*p));
    char *proto_name;

    if (class_name != NULL) {
        if (name[0] != '<') {
            proto_name = loli_malloc(strlen(class_name) + strlen(name) + 2);
            strcpy(proto_name, class_name);
            strcat(proto_name, ".");
            strcat(proto_name, name);
        }
        else {
             
            proto_name = loli_malloc(strlen(class_name) + 1);
            strcpy(proto_name, class_name);
        }
    }
    else {
        proto_name = loli_malloc(strlen(name) + 1);
        strcpy(proto_name, name);
    }

    p->module_path = module_path;
    p->name = proto_name;
    p->locals = NULL;
    p->code = NULL;
    p->arg_names = NULL;

    protos->data[protos->pos] = p;
    protos->pos++;

    return p;
}

loli_proto *loli_emit_proto_for_var(loli_emit_state *emit, loli_var *var)
{
    loli_value *v = loli_vs_nth(emit->symtab->literals, var->reg_spot);
    return v->value.function->proto;
}

static const char *opname(loli_expr_op op)
{
    static const char *opnames[] =
    {"+", "++", "-", "==", "<", "<=", ">", ">=", "!=", "%", "*", "/", "<<",
     ">>", "&", "|", "^", "!", "-", "~", "&&", "||", "|>", "=", "+=", "-=",
     "%=", "*=", "/=", "<<=", ">>=", "&=", "|=", "^="};

    return opnames[op];
}

static void ensure_valid_condition_type(loli_emit_state *emit, loli_type *type)
{
    int cls_id = type->cls->id;

    if (cls_id != LOLI_ID_INTEGER &&
        cls_id != LOLI_ID_DOUBLE &&
        cls_id != LOLI_ID_STRING &&
        cls_id != LOLI_ID_LIST &&
        cls_id != LOLI_ID_BOOLEAN)
        loli_raise_syn(emit->raiser, "^T is not a valid condition type.", type);
}

static void check_valid_subscript(loli_emit_state *emit, loli_ast *var_ast,
        loli_ast *index_ast)
{
    int var_cls_id = var_ast->result->type->cls->id;
    if (var_cls_id == LOLI_ID_LIST || var_cls_id == LOLI_ID_BYTESTRING) {
        uint16_t index_id = index_ast->result->type->cls->id;

        if (index_id != LOLI_ID_INTEGER &&
            index_id != LOLI_ID_BYTE)
            loli_raise_adjusted(emit->raiser, var_ast->line_num,
                    "%s index is not an Integer or a Byte.",
                    var_ast->result->type->cls->name);
    }
    else if (var_cls_id == LOLI_ID_HASH) {
        loli_type *want_key = var_ast->result->type->subtypes[0];
        loli_type *have_key = index_ast->result->type;

        if (want_key != have_key) {
            loli_raise_adjusted(emit->raiser, var_ast->line_num,
                    "Hash index should be type '^T', not type '^T'.",
                    want_key, have_key);
        }
    }
    else if (var_cls_id == LOLI_ID_TUPLE) {
        if (index_ast->tree_type != tree_integer) {
            loli_raise_adjusted(emit->raiser, var_ast->line_num,
                    "Tuple subscripts must be Integer literals.", "");
        }

        int index_value = index_ast->backing_value;
        loli_type *var_type = var_ast->result->type;
        if (index_value < 0 || index_value >= var_type->subtype_count) {
            loli_raise_adjusted(emit->raiser, var_ast->line_num,
                    "Index %d is out of range for ^T.", index_value, var_type);
        }
    }
    else {
        loli_raise_adjusted(emit->raiser, var_ast->line_num,
                "Cannot subscript type '^T'.",
                var_ast->result->type);
    }
}

static loli_type *get_subscript_result(loli_emit_state *emit, loli_type *type,
        loli_ast *index_ast)
{
    loli_type *result;
    if (type->cls->id == LOLI_ID_LIST)
        result = type->subtypes[0];
    else if (type->cls->id == LOLI_ID_HASH)
        result = type->subtypes[1];
    else if (type->cls->id == LOLI_ID_TUPLE) {
         
        int literal_index = index_ast->backing_value;
        result = type->subtypes[literal_index];
    }
    else if (type->cls->id == LOLI_ID_BYTESTRING)
        result = emit->symtab->byte_class->self_type;
    else
         
        result = NULL;

    return result;
}


static void write_build_op(loli_emit_state *emit, int opcode,
        loli_ast *first_arg, int line_num, int num_values, loli_storage *s)
{
    int i;
    loli_ast *arg;
    loli_u16_write_prep(emit->code, 5 + num_values);

    loli_u16_write_1(emit->code, opcode);

    if (opcode == o_build_hash)
         
        loli_u16_write_1(emit->code, s->type->subtypes[0]->cls->id);

    loli_u16_write_1(emit->code, num_values);

    for (i = 0, arg = first_arg; arg != NULL; arg = arg->next_arg, i++)
        loli_u16_write_1(emit->code, arg->result->reg_spot);

    loli_u16_write_2(emit->code, s->reg_spot, line_num);
}


static void ensure_valid_scope(loli_emit_state *emit, loli_sym *sym)
{
    if (sym->flags & (SYM_SCOPE_PRIVATE | SYM_SCOPE_PROTECTED)) {
        loli_class *block_class = emit->block->class_entry;
        loli_class *parent;
        int is_private = (sym->flags & SYM_SCOPE_PRIVATE);
        char *name;

        if (sym->item_kind == ITEM_TYPE_PROPERTY) {
            loli_prop_entry *prop = (loli_prop_entry *)sym;
            parent = prop->cls;
            name = prop->name;
        }
        else {
            loli_var *v = (loli_var *)sym;
            parent = v->parent;
            name = v->name;
        }

        if ((is_private && block_class != parent) ||
            (is_private == 0 &&
             (block_class == NULL || loli_class_greater_eq(parent, block_class) == 0))) {
            char *scope_name = is_private ? "private" : "protected";
            loli_raise_syn(emit->raiser,
                       "%s.%s is marked %s, and not available here.",
                       parent->name, name, scope_name);
        }
    }
}

static loli_type *determine_left_type(loli_emit_state *emit, loli_ast *ast)
{
    loli_type *result_type = NULL;

    if (ast->tree_type == tree_global_var ||
        ast->tree_type == tree_local_var)
        result_type = ast->sym->type;
    else if (ast->tree_type == tree_subscript) {
        loli_ast *var_tree = ast->arg_start;
        loli_ast *index_tree = var_tree->next_arg;

        result_type = determine_left_type(emit, var_tree);

        if (result_type != NULL) {
            if (result_type->cls->id == LOLI_ID_HASH)
                result_type = result_type->subtypes[1];
            else if (result_type->cls->id == LOLI_ID_TUPLE) {
                if (index_tree->tree_type != tree_integer)
                    result_type = NULL;
                else {
                    int literal_index = index_tree->backing_value;
                    if (literal_index < 0 ||
                        literal_index > result_type->subtype_count)
                        result_type = NULL;
                    else
                        result_type = result_type->subtypes[literal_index];
                }
            }
            else if (result_type->cls->id == LOLI_ID_LIST)
                result_type = result_type->subtypes[0];
            else if (result_type->cls->id == LOLI_ID_BYTESTRING)
                result_type = emit->symtab->byte_class->self_type;
        }
    }
    else if (ast->tree_type == tree_oo_access) {
        loli_type *lookup_type = determine_left_type(emit, ast->arg_start);
        if (lookup_type != NULL) {
            char *oo_name = loli_sp_get(emit->expr_strings, ast->pile_pos);
            loli_class *lookup_class = lookup_type->cls;

            loli_prop_entry *prop = loli_find_property(lookup_class, oo_name);

            if (prop) {
                result_type = prop->type;
                if (result_type->flags & TYPE_IS_UNRESOLVED) {
                    result_type = loli_ts_resolve_by_second(emit->ts,
                            lookup_type, result_type);
                }
            }
        }
    }
    else if (ast->tree_type == tree_property)
        result_type = ast->property->type;
     

    return result_type;
}

static int assign_optimize_check(loli_ast *ast)
{
    int can_optimize = 1;

    do {
         
        if (ast->left->tree_type == tree_global_var) {
            can_optimize = 0;
            break;
        }

        loli_ast *right_tree = ast->right;

         
        while (right_tree->tree_type == tree_parenth)
            right_tree = right_tree->arg_start;

         
        if (right_tree->tree_type == tree_local_var) {
            can_optimize = 0;
            break;
        }

         
        if (right_tree->tree_type == tree_binary &&
            (right_tree->op == expr_logical_and ||
             right_tree->op == expr_logical_or)) {
            can_optimize = 0;
            break;
        }

         
        if (right_tree->tree_type == tree_binary &&
            right_tree->op >= expr_assign) {
            can_optimize = 0;
            break;
        }
    } while (0);

    return can_optimize;
}

static int type_matchup(loli_emit_state *emit, loli_type *want_type,
        loli_ast *right)
{
    int ret;
    loli_type *right_type = right->result->type;

    if (want_type == right_type ||
        loli_ts_type_greater_eq(emit->ts, want_type, right_type))
        ret = 1;
    else
        ret = 0;

    return ret;
}


static char *keypos_to_keyarg(char *, int);

static void inconsistent_type_error(loli_emit_state *emit, loli_ast *ast,
        loli_type *expect, const char *context)
{
    loli_raise_adjusted(emit->raiser, ast->line_num,
            "%s do not have a consistent type.\n"
            "Expected Type: ^T\n"
            "Received Type: ^T",
            context, expect, ast->result->type);
}

static void bad_assign_error(loli_emit_state *emit, int line_num,
        loli_type *left_type, loli_type *right_type)
{
    loli_raise_adjusted(emit->raiser, line_num,
            "Cannot assign type '^T' to type '^T'.",
            right_type, left_type);
}

static void incomplete_type_assign_error(loli_emit_state *emit, int line_num,
        loli_type *right_type)
{
    loli_raise_adjusted(emit->raiser, line_num,
            "Right side of assignment has incomplete type '^T'.",
            right_type);
}

static void add_call_name_to_msgbuf(loli_emit_state *emit, loli_msgbuf *msgbuf,
        loli_ast *ast)
{
    loli_item *item = ast->item;

    if (item->item_kind == ITEM_TYPE_VAR) {
        loli_var *v = (loli_var *)item;

        if (v->flags & VAR_IS_READONLY) {
            loli_value *val = loli_vs_nth(emit->symtab->literals, v->reg_spot);
            loli_proto *p = val->value.function->proto;
            loli_mb_add(msgbuf, p->name);
        }
        else
            loli_mb_add(msgbuf, v->name);
    }
    else if (item->item_kind == ITEM_TYPE_PROPERTY) {
        loli_prop_entry *p = (loli_prop_entry *)ast->item;
        loli_mb_add_fmt(msgbuf, "%s.%s", p->cls->name, p->name);
    }
    else if (item->item_kind == ITEM_TYPE_VARIANT) {
        loli_variant_class *v = (loli_variant_class *)ast->item;
        loli_mb_add_fmt(msgbuf, "%s", v->name);
    }
    else
        loli_mb_add(msgbuf, "(anonymous)");
}

static void error_bad_arg(loli_emit_state *emit, loli_ast *ast,
        loli_type *call_type, int index, loli_type *got)
{
     
    loli_ts_reset_scoops(emit->ts);

    loli_type *expected;

    if ((call_type->flags & TYPE_IS_VARARGS) == 0 ||
        index < call_type->subtype_count - 2)
        expected = call_type->subtypes[index + 1];
    else {
         
        expected = call_type->subtypes[call_type->subtype_count - 1];
        expected = expected->subtypes[0];
    }

    if (expected->flags & TYPE_IS_UNRESOLVED)
        expected = loli_ts_resolve(emit->ts, expected);

    loli_msgbuf *msgbuf = emit->raiser->aux_msgbuf;
    loli_mb_flush(msgbuf);

    loli_mb_add_fmt(msgbuf, "Parameter #%d to '", index + 1);
    add_call_name_to_msgbuf(emit, msgbuf, ast);
    loli_mb_add_fmt(msgbuf,
            "' is invalid:\n"
            "Expected type: ^T\n"
            "But got type: ^T", expected, got);

    loli_raise_adjusted(emit->raiser, ast->line_num, loli_mb_raw(msgbuf), "");
}

static void error_argument_count(loli_emit_state *emit, loli_ast *ast,
        int count, int min, int max)
{
     
    if (ast->keep_first_call_arg) {
        min--;
        count--;
        if (max != -1)
            max--;
    }

     
    const char *div_str = "";
    char arg_str[8], min_str[8] = "", max_str[8] = "";

    if (count == -1)
        strncpy(arg_str, "none", sizeof(arg_str));
    else
        snprintf(arg_str, sizeof(arg_str), "%d", count);

    snprintf(min_str, sizeof(min_str), "%d", min);

    if (min == max)
        div_str = "";
    else if (max == -1)
        div_str = "+";
    else {
        div_str = "..";
        snprintf(max_str, sizeof(max_str), "%d", max);
    }

    loli_msgbuf *msgbuf = emit->raiser->aux_msgbuf;
    loli_mb_flush(msgbuf);

    loli_mb_add(msgbuf, "Wrong number of arguments to ");
    add_call_name_to_msgbuf(emit, msgbuf, ast);
    loli_mb_add_fmt(msgbuf, " (%s for %s%s%s).", arg_str, min_str, div_str,
            max_str);

    loli_raise_adjusted(emit->raiser, ast->line_num, loli_mb_raw(msgbuf),
            "");
}

static void error_keyarg_not_supported(loli_emit_state *emit, loli_ast *ast)
{
    loli_msgbuf *msgbuf = emit->raiser->aux_msgbuf;
    loli_mb_flush(msgbuf);

    add_call_name_to_msgbuf(emit, msgbuf, ast);

    if (ast->sym->item_kind == ITEM_TYPE_VAR &&
        ast->sym->flags & VAR_IS_READONLY)
        loli_mb_add(msgbuf,
                " does not specify any keyword arguments.");
    else
        loli_mb_add(msgbuf,
                " is not capable of receiving keyword arguments.");

    loli_raise_adjusted(emit->raiser, ast->line_num, loli_mb_raw(msgbuf), "");
}

static void error_keyarg_not_valid(loli_emit_state *emit, loli_ast *ast,
        loli_ast *arg)
{
    char *key_name = loli_sp_get(emit->expr_strings, arg->left->pile_pos);
    loli_msgbuf *msgbuf = emit->raiser->aux_msgbuf;
    loli_mb_flush(msgbuf);

    add_call_name_to_msgbuf(emit, msgbuf, ast);
    loli_mb_add_fmt(msgbuf, " does not have a keyword named ':%s'.", key_name);

    loli_raise_adjusted(emit->raiser, arg->line_num, loli_mb_raw(msgbuf), "");
}

static void error_keyarg_duplicate(loli_emit_state *emit, loli_ast *ast,
        loli_ast *arg)
{
    char *key_name = loli_sp_get(emit->expr_strings, arg->left->pile_pos);
    loli_msgbuf *msgbuf = emit->raiser->aux_msgbuf;
    loli_mb_flush(msgbuf);

    loli_mb_add(msgbuf, "Call to ");
    add_call_name_to_msgbuf(emit, msgbuf, ast);
    loli_mb_add_fmt(msgbuf, " has multiple values for parameter ':%s'.", key_name);

    loli_raise_adjusted(emit->raiser, arg->line_num, loli_mb_raw(msgbuf), "");
}

static void error_keyarg_before_posarg(loli_emit_state *emit, loli_ast *arg)
{
    loli_raise_adjusted(emit->raiser, arg->line_num,
            "Positional argument after keyword argument.", "");
}

static void error_keyarg_missing_params(loli_emit_state *emit, loli_ast *ast,
        loli_type *call_type, char *keyword_names)
{
    loli_msgbuf *msgbuf = emit->raiser->aux_msgbuf;
    loli_mb_flush(msgbuf);

    loli_mb_add(msgbuf, "Call to ");
    add_call_name_to_msgbuf(emit, msgbuf, ast);
    loli_mb_add(msgbuf, " is missing parameters:");

    loli_ast *arg_iter = ast->arg_start;
    loli_type **arg_types = call_type->subtypes;
    int i = 0, stop = call_type->subtype_count - 1;

    if (call_type->flags & TYPE_IS_VARARGS)
        stop--;

    for (arg_iter = ast->arg_start;
         i != stop;
         arg_iter = arg_iter->next_arg) {
        if (arg_iter->keyword_arg_pos != i) {
            int cycle_end = arg_iter->keyword_arg_pos;
            int skip = stop + 1;

            if (arg_iter->next_arg == NULL) {
                cycle_end = stop;
                skip = arg_iter->keyword_arg_pos;
            }

            while (i != cycle_end) {
                if (i == skip) {
                    i++;
                    continue;
                }

                char *arg_name = keypos_to_keyarg(keyword_names, i);

                i++;

                loli_type *t = arg_types[i];

                if (t->cls->id == LOLI_ID_OPTARG)
                    continue;

                if (*arg_name != ' ')
                    loli_mb_add_fmt(msgbuf,
                            "\n* Parameter #%d (:%s) of type ^T.", i, arg_name,
                            t);
                else
                    loli_mb_add_fmt(msgbuf, "\n* Parameter #%d of type ^T.", i,
                            t);
            }
        }
    }

    loli_raise_adjusted(emit->raiser, ast->line_num, loli_mb_raw(msgbuf), "");
}



static void eval_oo_access_for_item(loli_emit_state *emit, loli_ast *ast)
{
    if (ast->arg_start->tree_type != tree_local_var)
        eval_tree(emit, ast->arg_start, NULL);


    loli_class *lookup_class = ast->arg_start->result->type->cls;
    char *oo_name = loli_sp_get(emit->expr_strings, ast->pile_pos);
    loli_item *item = loli_find_or_dl_member(emit->parser, lookup_class,
            oo_name, NULL);

    if (item == NULL) {
        loli_class *cls = loli_find_class_of_member(lookup_class, oo_name);
        if (cls) {
            loli_raise_adjusted(emit->raiser, ast->arg_start->line_num,
                    "%s is a private member of class %s, and not visible here.",
                    oo_name, cls->name);
        }
        else {
            loli_raise_adjusted(emit->raiser, ast->arg_start->line_num,
                    "Class %s has no method or property named %s.",
                    lookup_class->name, oo_name);
        }
    }
    else if (item->item_kind == ITEM_TYPE_PROPERTY &&
             ast->arg_start->tree_type == tree_self) {
        loli_raise_adjusted(emit->raiser, ast->arg_start->line_num,
                "Use @<name> to get/set properties, not self.<name>.", "");
    }
    else
        ast->item = item;

    ensure_valid_scope(emit, (loli_sym *)item);
}

static loli_type *get_solved_property_type(loli_emit_state *emit, loli_ast *ast)
{
    loli_type *property_type = ast->property->type;
    if (property_type->flags & TYPE_IS_UNRESOLVED) {
        property_type = loli_ts_resolve_by_second(emit->ts,
                ast->arg_start->result->type, property_type);
    }

    return property_type;
}

static void oo_property_read(loli_emit_state *emit, loli_ast *ast)
{
    loli_prop_entry *prop = ast->property;
    loli_type *type = get_solved_property_type(emit, ast);
    loli_storage *result = get_storage(emit, type);

     
    loli_u16_write_5(emit->code, o_property_get, prop->id,
            ast->arg_start->result->reg_spot, result->reg_spot, ast->line_num);

    ast->result = (loli_sym *)result;
}

static void eval_oo_access(loli_emit_state *emit, loli_ast *ast)
{
    eval_oo_access_for_item(emit, ast);
     
    if (ast->item->item_kind == ITEM_TYPE_PROPERTY)
        oo_property_read(emit, ast);
    else {
        loli_storage *result = get_storage(emit, ast->sym->type);
        loli_u16_write_4(emit->code, o_load_readonly, ast->sym->reg_spot,
                result->reg_spot, ast->line_num);
        ast->result = (loli_sym *)result;
    }
}



static void emit_binary_op(loli_emit_state *emit, loli_ast *ast)
{
    loli_sym *lhs_sym = ast->left->result;
    loli_sym *rhs_sym = ast->right->result;
    loli_class *lhs_class = lhs_sym->type->cls;
    loli_class *rhs_class = rhs_sym->type->cls;
    int opcode = -1;
    loli_storage *s;

    if (lhs_sym->type == rhs_sym->type) {
        int lhs_id = lhs_class->id;
        int op = ast->op;

        if (lhs_id == LOLI_ID_INTEGER) {
            if (op == expr_plus)
                opcode = o_int_add;
            else if (op == expr_minus)
                opcode = o_int_minus;
            else if (op == expr_multiply)
                opcode = o_int_multiply;
            else if (op == expr_divide)
                opcode = o_int_divide;
            else if (op == expr_modulo)
                opcode = o_int_modulo;
            else if (op == expr_left_shift)
                opcode = o_int_left_shift;
            else if (op == expr_right_shift)
                opcode = o_int_right_shift;
            else if (op == expr_bitwise_and)
                opcode = o_int_bitwise_and;
            else if (op == expr_bitwise_or)
                opcode = o_int_bitwise_or;
            else if (op == expr_bitwise_xor)
                opcode = o_int_bitwise_xor;
        }
        else if (lhs_id == LOLI_ID_DOUBLE) {
            if (op == expr_plus)
                opcode = o_number_add;
            else if (op == expr_minus)
                opcode = o_number_minus;
            else if (op == expr_multiply)
                opcode = o_number_multiply;
            else if (op == expr_divide)
                opcode = o_number_divide;
        }

        if (lhs_id == LOLI_ID_INTEGER ||
            lhs_id == LOLI_ID_BYTE ||
            lhs_id == LOLI_ID_DOUBLE ||
            lhs_id == LOLI_ID_STRING) {
            if (op == expr_lt_eq) {
                loli_sym *temp = rhs_sym;
                rhs_sym = lhs_sym;
                lhs_sym = temp;
                opcode = o_compare_greater_eq;
            }
            else if (op == expr_lt) {
                loli_sym *temp = rhs_sym;
                rhs_sym = lhs_sym;
                lhs_sym = temp;
                opcode = o_compare_greater;
            }
            else if (op == expr_gr_eq)
                opcode = o_compare_greater_eq;
            else if (op == expr_gr)
                opcode = o_compare_greater;
        }

        if (op == expr_eq_eq)
            opcode = o_compare_eq;
        else if (op == expr_not_eq)
            opcode = o_compare_not_eq;
    }

    if (opcode == -1)
        loli_raise_adjusted(emit->raiser, ast->line_num,
                   "Invalid operation: ^T %s ^T.", ast->left->result->type,
                   opname(ast->op), ast->right->result->type);

    loli_class *storage_class;
    switch (ast->op) {
        case expr_plus:
        case expr_minus:
        case expr_multiply:
        case expr_divide:
            storage_class = lhs_sym->type->cls;
            break;
        case expr_eq_eq:
        case expr_lt:
        case expr_lt_eq:
        case expr_gr:
        case expr_gr_eq:
        case expr_not_eq:
            storage_class = emit->symtab->boolean_class;
            break;
        default:
            storage_class = emit->symtab->integer_class;
    }

     
    if (lhs_sym->item_kind == ITEM_TYPE_STORAGE &&
        lhs_class == storage_class)
        s = (loli_storage *)lhs_sym;
    else if (rhs_sym->item_kind == ITEM_TYPE_STORAGE &&
             rhs_class == storage_class)
        s = (loli_storage *)rhs_sym;
    else {
        s = get_storage(emit, storage_class->self_type);
        s->flags |= SYM_NOT_ASSIGNABLE;
    }

    loli_u16_write_5(emit->code, opcode, lhs_sym->reg_spot, rhs_sym->reg_spot,
            s->reg_spot, ast->line_num);

    ast->result = (loli_sym *)s;
}

static void set_compound_spoof_op(loli_emit_state *emit, loli_ast *ast)
{
    loli_expr_op spoof_op;

    if (ast->op == expr_div_assign)
        spoof_op = expr_divide;
    else if (ast->op == expr_mul_assign)
        spoof_op = expr_multiply;
    else if (ast->op == expr_modulo_assign)
        spoof_op = expr_modulo;
    else if (ast->op == expr_plus_assign)
        spoof_op = expr_plus;
    else if (ast->op == expr_minus_assign)
        spoof_op = expr_minus;
    else if (ast->op == expr_left_shift_assign)
        spoof_op = expr_left_shift;
    else if (ast->op == expr_right_shift_assign)
        spoof_op = expr_right_shift;
    else if (ast->op == expr_bitwise_and_assign)
        spoof_op = expr_bitwise_and;
    else if (ast->op == expr_bitwise_or_assign)
        spoof_op = expr_bitwise_or;
    else if (ast->op == expr_bitwise_xor_assign)
        spoof_op = expr_bitwise_xor;
    else {
        spoof_op = expr_assign;
        loli_raise_syn(emit->raiser, "Invalid compound op: %s.",
                opname(ast->op));
    }

    ast->op = spoof_op;
}

static void emit_compound_op(loli_emit_state *emit, loli_ast *ast)
{
    loli_tree_type left_tt = ast->left->tree_type;

    if (left_tt == tree_global_var ||
        left_tt == tree_local_var) {
        if (left_tt == tree_global_var)
            eval_tree(emit, ast->left, NULL);
    }
    else if (left_tt == tree_property) {
        eval_tree(emit, ast->left, NULL);
    }
    else if (left_tt == tree_oo_access) {
        oo_property_read(emit, ast->left);
    }
    else if (left_tt == tree_upvalue) {
        loli_var *left_var = (loli_var *)ast->left->sym;
         
        uint16_t spot = find_closed_sym_spot(emit, left_var->function_depth,
                (loli_sym *)left_var);

        loli_storage *s = get_storage(emit, ast->left->sym->type);
        loli_u16_write_4(emit->code, o_closure_get, spot, s->reg_spot,
                ast->line_num);
        ast->left->result = (loli_sym *)s;
    }
    else if (left_tt == tree_subscript) {
         
        loli_ast *index_ast = ast->left->arg_start->next_arg;
        loli_sym *var_sym = ast->left->arg_start->result;
        loli_sym *index_sym = index_ast->result;
        loli_type *elem_type = get_subscript_result(emit, var_sym->type,
                index_ast);
        loli_storage *s = get_storage(emit, elem_type);

        loli_u16_write_5(emit->code, o_subscript_get, var_sym->reg_spot,
                index_sym->reg_spot, s->reg_spot, ast->line_num);

        ast->left->result = (loli_sym *)s;

         
    }

    loli_expr_op save_op = ast->op;

    set_compound_spoof_op(emit, ast);
    emit_binary_op(emit, ast);
    ast->op = save_op;
}

static void eval_assign_global_local(loli_emit_state *emit, loli_ast *ast)
{
    eval_tree(emit, ast->right, ast->left->result->type);
}

static void eval_assign_property(loli_emit_state *emit, loli_ast *ast)
{
    if (emit->function_block->self == NULL)
        close_over_class_self(emit, ast);

    ensure_valid_scope(emit, ast->left->sym);
    eval_tree(emit, ast->right, ast->left->property->type);
}

static void eval_assign_oo(loli_emit_state *emit, loli_ast *ast)
{
    eval_oo_access_for_item(emit, ast->left);
    ensure_valid_scope(emit, ast->left->sym);
     
    if (ast->left->item->item_kind != ITEM_TYPE_PROPERTY)
        loli_raise_adjusted(emit->raiser, ast->line_num,
                "Left side of %s is not assignable.", opname(ast->op));

    loli_type *left_type = get_solved_property_type(emit, ast->left);

    eval_tree(emit, ast->right, left_type);

    loli_type *right_type = ast->right->result->type;

    if (right_type->flags & TYPE_IS_INCOMPLETE)
        incomplete_type_assign_error(emit, ast->line_num, right_type);

    if (left_type != right_type &&
        type_matchup(emit, left_type, ast->right) == 0) {
        emit->raiser->line_adjust = ast->line_num;
        bad_assign_error(emit, ast->line_num, left_type, right_type);
    }
}

static void eval_assign_upvalue(loli_emit_state *emit, loli_ast *ast)
{
    eval_tree(emit, ast->right, NULL);

    loli_var *left_var = (loli_var *)ast->left->sym;
    uint16_t spot = find_closed_sym_spot(emit, left_var->function_depth,
            (loli_sym *)left_var);

    if (spot == (uint16_t)-1)
        spot = checked_close_over_var(emit, left_var);
}

static void eval_assign_sub(loli_emit_state *emit, loli_ast *ast)
{
    loli_ast *var_ast = ast->left->arg_start;
    loli_ast *index_ast = var_ast->next_arg;

     
    loli_type *left_type = determine_left_type(emit, ast->left);

    if (ast->right->tree_type != tree_local_var)
        eval_tree(emit, ast->right, left_type);

    if (var_ast->tree_type != tree_local_var) {
        eval_tree(emit, var_ast, NULL);
        if (var_ast->result->flags & SYM_NOT_ASSIGNABLE) {
            loli_raise_adjusted(emit->raiser, ast->line_num,
                    "Left side of %s is not assignable.", opname(ast->op));
        }
    }

    if (index_ast->tree_type != tree_local_var)
        eval_tree(emit, index_ast, NULL);

    check_valid_subscript(emit, var_ast, index_ast);

    loli_type *elem_type = get_subscript_result(emit, var_ast->result->type,
            index_ast);
    loli_type *right_type = ast->right->result->type;

    if (right_type->flags & TYPE_IS_INCOMPLETE)
        incomplete_type_assign_error(emit, ast->line_num, right_type);

    if (type_matchup(emit, elem_type, ast->right) == 0) {
        emit->raiser->line_adjust = ast->line_num;
        bad_assign_error(emit, ast->line_num, elem_type, right_type);
    }
}

static void eval_assign(loli_emit_state *emit, loli_ast *ast)
{
    loli_tree_type left_tt = ast->left->tree_type;
    loli_sym *left_sym = NULL;
    loli_sym *right_sym = NULL;

    if (left_tt == tree_local_var ||
        left_tt == tree_global_var) {
        eval_assign_global_local(emit, ast);

        left_sym = ast->left->result;
        left_sym->flags &= ~SYM_NOT_INITIALIZED;
        right_sym = ast->right->result;

        if (left_sym->type == NULL)
            left_sym->type = right_sym->type;
    }
    else if (left_tt == tree_property) {
        eval_assign_property(emit, ast);
         
        left_sym = ast->left->sym;
        left_sym->flags &= ~SYM_NOT_INITIALIZED;
        right_sym = ast->right->result;

        if (left_sym->type == NULL)
            left_sym->type = right_sym->type;
    }
    else if (left_tt == tree_oo_access) {
        eval_assign_oo(emit, ast);
        left_sym = ast->left->sym;
        right_sym = ast->right->result;
         
        goto after_type_check;
    }
    else if (left_tt == tree_upvalue) {
        eval_assign_upvalue(emit, ast);
        left_sym = ast->left->sym;
        right_sym = ast->right->result;
    }
    else if (left_tt == tree_subscript) {
        eval_assign_sub(emit, ast);
        right_sym = ast->right->result;
         
        goto after_type_check;
    }
    else
        loli_raise_adjusted(emit->raiser, ast->line_num,
                "Left side of %s is not assignable.", opname(ast->op));

    if (right_sym->type->flags & TYPE_IS_INCOMPLETE)
        incomplete_type_assign_error(emit, ast->line_num, right_sym->type);

    if (left_sym->type != right_sym->type &&
        loli_ts_type_greater_eq(emit->ts, left_sym->type, right_sym->type) == 0)
        bad_assign_error(emit, ast->line_num, left_sym->type, right_sym->type);

after_type_check:;

    if (ast->op > expr_assign) {
        emit_compound_op(emit, ast);
         
        right_sym = ast->result;
    }

    if (left_tt == tree_local_var ||
        left_tt == tree_global_var) {
        if (assign_optimize_check(ast)) {
             
            int pos = loli_u16_pos(emit->code) - 2;

            loli_u16_set_at(emit->code, pos, left_sym->reg_spot);
        }
        else {
            uint16_t left_id = left_sym->type->cls->id;
            uint16_t opcode;

            if (left_tt == tree_global_var)
                opcode = o_global_set;
            else if (left_id == LOLI_ID_INTEGER ||
                     left_id == LOLI_ID_DOUBLE)
                opcode = o_assign_noref;
            else
                opcode = o_assign;

            loli_u16_write_4(emit->code, opcode, right_sym->reg_spot,
                    left_sym->reg_spot, ast->line_num);
        }
    }
    else if (left_tt == tree_property) {
        loli_u16_write_5(emit->code, o_property_set,
                ((loli_prop_entry *)left_sym)->id,
                emit->function_block->self->reg_spot,
                right_sym->reg_spot, ast->line_num);
    }
    else if (left_tt == tree_oo_access) {
        uint16_t left_id = ((loli_prop_entry *)left_sym)->id;

        loli_u16_write_5(emit->code, o_property_set, left_id,
                ast->left->arg_start->result->reg_spot, right_sym->reg_spot,
                ast->line_num);
    }
    else if (left_tt == tree_upvalue) {
        loli_var *left_var = (loli_var *)left_sym;
        uint16_t spot = find_closed_sym_spot(emit, left_var->function_depth,
                left_sym);

        loli_u16_write_4(emit->code, o_closure_set, spot, right_sym->reg_spot,
                ast->line_num);
    }
    else if (left_tt == tree_subscript) {
        loli_ast *index_ast = ast->left->arg_start->next_arg;
        loli_sym *var_sym = ast->left->arg_start->result;
        loli_sym *index_sym = index_ast->result;

        loli_u16_write_5(emit->code, o_subscript_set, var_sym->reg_spot,
                index_sym->reg_spot, right_sym->reg_spot, ast->line_num);
    }

    if (ast->parent &&
         (ast->parent->tree_type != tree_binary ||
          ast->parent->op < expr_assign)) {
        loli_raise_syn(emit->raiser,
                "Cannot nest an assignment within an expression.");
    }
    else if (ast->parent == NULL) {
         
        ast->result = NULL;
    }
    else
        ast->result = right_sym;
}

static void eval_property(loli_emit_state *emit, loli_ast *ast)
{
    ensure_valid_scope(emit, ast->sym);
    if (emit->function_block->self == NULL)
        close_over_class_self(emit, ast);

    if (ast->property->flags & SYM_NOT_INITIALIZED)
        loli_raise_adjusted(emit->raiser, ast->line_num,
                "Invalid use of uninitialized property '@%s'.",
                ast->property->name);

    loli_storage *result = get_storage(emit, ast->property->type);

    loli_u16_write_5(emit->code, o_property_get, ast->property->id,
            emit->function_block->self->reg_spot,
            result->reg_spot, ast->line_num);

    ast->result = (loli_sym *)result;
}

static void eval_lambda(loli_emit_state *emit, loli_ast *ast,
        loli_type *expect)
{
    int save_expr_num = emit->expr_num;
    char *lambda_body = loli_sp_get(emit->expr_strings, ast->pile_pos);

    if (expect) {
        if (expect->cls->id != LOLI_ID_FUNCTION)
            expect = NULL;
        else if (expect->subtypes[0]->cls == loli_self_class) {
            loli_raise_adjusted(emit->raiser, ast->line_num,
                    "Lambdas cannot return the self type (not a class method).",
                    "");
        }
    }

    loli_sym *lambda_result = (loli_sym *)loli_parser_lambda_eval(emit->parser,
            ast->line_num, lambda_body, expect);

     
    emit->expr_num = save_expr_num;

    loli_storage *s = get_storage(emit, lambda_result->type);

    if ((emit->function_block->flags & BLOCK_MAKE_CLOSURE) == 0)
        loli_u16_write_4(emit->code, o_load_readonly, lambda_result->reg_spot,
                s->reg_spot, ast->line_num);
    else
        emit_create_function(emit, lambda_result, s);

    ast->result = (loli_sym *)s;
}

static void eval_logical_op(loli_emit_state *emit, loli_ast *ast)
{
    loli_storage *result;
    int andor_start;
    int jump_on = (ast->op == expr_logical_or);

     
    if (ast->parent == NULL ||
        (ast->parent->tree_type != tree_binary || ast->parent->op != ast->op))
        andor_start = loli_u16_pos(emit->patches);
    else
        andor_start = -1;

    if (ast->left->tree_type != tree_local_var)
        eval_tree(emit, ast->left, NULL);

     
    if ((ast->left->tree_type == tree_binary && ast->left->op == ast->op) == 0)
        emit_jump_if(emit, ast->left, jump_on);

    if (ast->right->tree_type != tree_local_var)
        eval_tree(emit, ast->right, NULL);

    emit_jump_if(emit, ast->right, jump_on);

    if (andor_start != -1) {
        int save_pos;
        loli_symtab *symtab = emit->symtab;

        result = get_storage(emit, symtab->boolean_class->self_type);

        int truthy = (ast->op == expr_logical_and);

        loli_u16_write_4(emit->code, o_load_boolean, truthy, result->reg_spot,
                ast->line_num);

         
        loli_u16_write_2(emit->code, o_jump, 0);
        save_pos = loli_u16_pos(emit->code) - 1;

        write_patches_since(emit, andor_start);

        loli_u16_write_4(emit->code, o_load_boolean, !truthy, result->reg_spot,
                ast->line_num);

         
        loli_u16_set_at(emit->code, save_pos,
                loli_u16_pos(emit->code) + 1 - save_pos);
        ast->result = (loli_sym *)result;
    }
}

static void eval_subscript(loli_emit_state *emit, loli_ast *ast,
        loli_type *expect)
{
    loli_ast *var_ast = ast->arg_start;
    loli_ast *index_ast = var_ast->next_arg;
    if (var_ast->tree_type != tree_local_var)
        eval_tree(emit, var_ast, NULL);

    if (index_ast->tree_type != tree_local_var)
        eval_tree(emit, index_ast, NULL);

    check_valid_subscript(emit, var_ast, index_ast);

    loli_type *type_for_result;
    type_for_result = get_subscript_result(emit, var_ast->result->type,
            index_ast);

    loli_storage *result = get_storage(emit, type_for_result);

    loli_u16_write_5(emit->code, o_subscript_get, var_ast->result->reg_spot,
            index_ast->result->reg_spot, result->reg_spot, ast->line_num);

    if (var_ast->result->flags & SYM_NOT_ASSIGNABLE)
        result->flags |= SYM_NOT_ASSIGNABLE;

    ast->result = (loli_sym *)result;
}

static void eval_typecast(loli_emit_state *emit, loli_ast *ast)
{
    loli_type *cast_type = ast->arg_start->next_arg->type;
    loli_ast *right_tree = ast->arg_start;

    eval_tree(emit, right_tree, cast_type);

    ast->result = right_tree->result;

    if (cast_type != ast->result->type &&
        type_matchup(emit, cast_type, ast->right) == 0)
        loli_raise_adjusted(emit->raiser, ast->line_num,
                "Cannot cast type '^T' to type '^T'.",
                ast->result->type, cast_type);
}

static void eval_unary_op(loli_emit_state *emit, loli_ast *ast)
{
     
    if (ast->left->tree_type != tree_local_var)
        eval_tree(emit, ast->left, NULL);

    loli_class *lhs_class = ast->left->result->type->cls;
    uint16_t opcode = 0, lhs_id = lhs_class->id;
    loli_storage *storage;

    loli_expr_op op = ast->op;

    if (lhs_id == LOLI_ID_BOOLEAN) {
        if (op == expr_unary_not)
            opcode = o_unary_not;
    }
    else if (lhs_id == LOLI_ID_INTEGER) {
        if (op == expr_unary_minus)
            opcode = o_unary_minus;
        else if (op == expr_unary_not)
            opcode = o_unary_not;
        else if (op == expr_unary_bitwise_not)
            opcode = o_unary_bitwise_not;
    }
    else if (lhs_id == LOLI_ID_DOUBLE) {
        if (op == expr_unary_minus)
            opcode = o_unary_minus;
    }

    if (opcode == 0)
        loli_raise_adjusted(emit->raiser, ast->line_num,
                "Invalid operation: %s%s.",
                opname(ast->op), lhs_class->name);

    storage = get_storage(emit, lhs_class->self_type);
    storage->flags |= SYM_NOT_ASSIGNABLE;

    loli_u16_write_4(emit->code, opcode, ast->left->result->reg_spot,
            storage->reg_spot, ast->line_num);

    ast->result = (loli_sym *)storage;
}

static void eval_build_tuple(loli_emit_state *emit, loli_ast *ast,
        loli_type *expect)
{
    if (ast->args_collected == 0)
        loli_raise_syn(emit->raiser, "Cannot create an empty Tuple.");

    if (expect != NULL &&
        (expect->cls->id != LOLI_ID_TUPLE ||
         ast->args_collected > expect->subtype_count))
        expect = NULL;

    int i;
    loli_ast *arg;

    for (i = 0, arg = ast->arg_start;
         arg != NULL;
         i++, arg = arg->next_arg) {
        loli_type *elem_type = NULL;

         
        if (expect)
            elem_type = expect->subtypes[i];

        eval_tree(emit, arg, elem_type);
    }

    for (i = 0, arg = ast->arg_start;
         i < ast->args_collected;
         i++, arg = arg->next_arg) {
        loli_tm_add(emit->tm, arg->result->type);
    }

    loli_type *new_type = loli_tm_make(emit->tm, emit->symtab->tuple_class,
            i);
    loli_storage *s = get_storage(emit, new_type);

    write_build_op(emit, o_build_tuple, ast->arg_start, ast->line_num,
            ast->args_collected, s);
    ast->result = (loli_sym *)s;
}

static void emit_literal(loli_emit_state *emit, loli_ast *ast)
{
    loli_storage *s = get_storage(emit, ast->type);

    loli_u16_write_4(emit->code, o_load_readonly, ast->literal_reg_spot,
            s->reg_spot, ast->line_num);

    ast->result = (loli_sym *)s;
}

static void emit_nonlocal_var(loli_emit_state *emit, loli_ast *ast)
{
    int opcode;
    uint16_t spot;
    loli_sym *sym = ast->sym;
    loli_storage *ret = get_storage(emit, sym->type);

    switch (ast->tree_type) {
        case tree_global_var:
            opcode = o_global_get;
            spot = sym->reg_spot;
            break;
        case tree_upvalue: {
            opcode = o_closure_get;
            loli_var *v = (loli_var *)sym;

            spot = find_closed_sym_spot(emit, v->function_depth, (loli_sym *)v);
            if (spot == (uint16_t)-1)
                spot = checked_close_over_var(emit, v);

            emit->function_block->flags |= BLOCK_MAKE_CLOSURE;
            break;
        }
        case tree_static_func:
            ensure_valid_scope(emit, ast->sym);
        default:
            ret->flags |= SYM_NOT_ASSIGNABLE;
            spot = sym->reg_spot;
            opcode = o_load_readonly;
            break;
    }

    if ((sym->flags & VAR_NEEDS_CLOSURE) == 0 ||
        ast->tree_type == tree_upvalue) {
        loli_u16_write_4(emit->code, opcode, spot, ret->reg_spot,
                ast->line_num);
    }
    else
        emit_create_function(emit, sym, ret);

    ast->result = (loli_sym *)ret;
}

static void emit_integer(loli_emit_state *emit, loli_ast *ast)
{
    loli_storage *s = get_storage(emit, emit->symtab->integer_class->self_type);

    loli_u16_write_4(emit->code, o_load_integer, ast->backing_value,
            s->reg_spot, ast->line_num);

    ast->result = (loli_sym *)s;
}

static void emit_boolean(loli_emit_state *emit, loli_ast *ast)
{
    loli_storage *s = get_storage(emit, emit->symtab->boolean_class->self_type);

    loli_u16_write_4(emit->code, o_load_boolean, ast->backing_value, s->reg_spot,
            ast->line_num);

    ast->result = (loli_sym *)s;
}

static void emit_byte(loli_emit_state *emit, loli_ast *ast)
{
    loli_storage *s = get_storage(emit, emit->symtab->byte_class->self_type);

    loli_u16_write_4(emit->code, o_load_byte, ast->backing_value, s->reg_spot,
            ast->line_num);

    ast->result = (loli_sym *)s;
}

static void eval_self(loli_emit_state *emit, loli_ast *ast)
{
    loli_storage *self = emit->function_block->self;

    if (self == NULL) {
        close_over_class_self(emit, ast);
        self = emit->function_block->self;
    }

    ast->result = (loli_sym *)self;
}


static loli_type *bidirectional_unify(loli_type_system *ts,
        loli_type *left_type, loli_type *right_type)
{
    loli_type *result = loli_ts_unify(ts, left_type, right_type);

    if (result == NULL)
        result = loli_ts_unify(ts, right_type, left_type);

    return result;
}

static void ensure_valid_key_type(loli_emit_state *emit, loli_ast *ast,
        loli_type *key_type)
{
    if (key_type == NULL)
        key_type = loli_question_type;

    if ((key_type->cls->flags & CLS_VALID_HASH_KEY) == 0)
        loli_raise_adjusted(emit->raiser, ast->line_num,
                "Type '^T' is not a valid key for Hash.", key_type);
}

static void make_empty_list_or_hash(loli_emit_state *emit, loli_ast *ast,
        loli_type *expect)
{
    loli_class *cls;
    int num, op;

    if (expect && expect->cls->id == LOLI_ID_HASH) {
        loli_type *key_type = expect->subtypes[0];
        loli_type *value_type = expect->subtypes[1];
        ensure_valid_key_type(emit, ast, key_type);

        if (value_type == NULL)
            value_type = loli_question_type;

        loli_tm_add(emit->tm, key_type);
        loli_tm_add(emit->tm, value_type);

        cls = emit->symtab->hash_class;
        op = o_build_hash;
        num = 2;
    }
    else {
        loli_type *elem_type = loli_question_type;

        if (expect && expect->cls->id == LOLI_ID_LIST) {
            elem_type = expect->subtypes[0];

             
            if (elem_type->cls->id == LOLI_ID_SCOOP)
                elem_type = loli_unit_type;
        }

        loli_tm_add(emit->tm, elem_type);

        cls = emit->symtab->list_class;
        op = o_build_list;
        num = 1;
    }

    loli_storage *s = get_storage(emit, loli_tm_make(emit->tm, cls, num));
    write_build_op(emit, op, ast->arg_start, ast->line_num, 0, s);
    ast->result = (loli_sym *)s;
}

static void eval_build_hash(loli_emit_state *emit, loli_ast *ast,
        loli_type *expect)
{
    loli_ast *tree_iter;

    loli_type *key_type, *value_type;

    if (expect && expect->cls->id == LOLI_ID_HASH) {
        key_type = expect->subtypes[0];
        value_type = expect->subtypes[1];
        if (key_type == NULL)
            key_type = loli_question_type;

        if (value_type == NULL)
            value_type = loli_question_type;
    }
    else {
        key_type = loli_question_type;
        value_type = loli_question_type;
    }

    for (tree_iter = ast->arg_start;
         tree_iter != NULL;
         tree_iter = tree_iter->next_arg->next_arg) {

        loli_ast *key_tree, *value_tree;
        key_tree = tree_iter;
        value_tree = tree_iter->next_arg;

        loli_type *unify_type;

        eval_tree(emit, key_tree, key_type);

         
        if (key_type == loli_question_type) {
            key_type = key_tree->result->type;
            ensure_valid_key_type(emit, ast, key_type);
        }
        else if (key_type != key_tree->result->type) {
            inconsistent_type_error(emit, key_tree, key_type, "Hash keys");
        }

        eval_tree(emit, value_tree, value_type);
        unify_type = bidirectional_unify(emit->ts, value_type,
                value_tree->result->type);

        if (unify_type == NULL)
            inconsistent_type_error(emit, value_tree, value_type,
                    "Hash values");
        else
            value_type = unify_type;
    }

    loli_class *hash_cls = emit->symtab->hash_class;
    loli_tm_add(emit->tm, key_type);
    loli_tm_add(emit->tm, value_type);
    loli_type *new_type = loli_tm_make(emit->tm, hash_cls, 2);

    loli_storage *s = get_storage(emit, new_type);

    write_build_op(emit, o_build_hash, ast->arg_start, ast->line_num,
            ast->args_collected, s);
    ast->result = (loli_sym *)s;
}

static void eval_build_list(loli_emit_state *emit, loli_ast *ast,
        loli_type *expect)
{
    if (ast->args_collected == 0) {
        make_empty_list_or_hash(emit, ast, expect);
        return;
    }

    loli_type *elem_type = NULL;
    loli_ast *arg;

    if (expect && expect->cls->id == LOLI_ID_LIST)
        elem_type = expect->subtypes[0];

    if (elem_type == NULL || elem_type->cls->id == LOLI_ID_SCOOP)
        elem_type = loli_question_type;

    for (arg = ast->arg_start;arg != NULL;arg = arg->next_arg) {
        eval_tree(emit, arg, elem_type);

        loli_type *new_elem_type = bidirectional_unify(emit->ts, elem_type,
                arg->result->type);

        if (new_elem_type == NULL)
            inconsistent_type_error(emit, arg, elem_type, "List elements");

        elem_type = new_elem_type;
    }

    loli_tm_add(emit->tm, elem_type);
    loli_type *new_type = loli_tm_make(emit->tm, emit->symtab->list_class,
            1);

    loli_storage *s = get_storage(emit, new_type);

    write_build_op(emit, o_build_list, ast->arg_start, ast->line_num,
            ast->args_collected, s);
    ast->result = (loli_sym *)s;
}



static void get_func_min_max(loli_type *call_type, unsigned int *min,
        unsigned int *max)
{
    *min = call_type->subtype_count - 1;
    *max = *min;

    if (call_type->flags & TYPE_HAS_OPTARGS) {
        int i;
        for (i = 1;i < call_type->subtype_count;i++) {
            if (call_type->subtypes[i]->cls->id == LOLI_ID_OPTARG)
                break;
        }
        *min = i - 1;
    }

    if (call_type->flags & TYPE_IS_VARARGS) {
        *max = (unsigned int)-1;

        if ((call_type->flags & TYPE_HAS_OPTARGS) == 0)
            *min = *min - 1;
    }
}

static void setup_call_result(loli_emit_state *emit, loli_ast *ast,
        loli_type *return_type)
{
    if (return_type == loli_self_class->self_type)
        ast->result = ast->arg_start->result;
    else if (ast->first_tree_type == tree_inherited_new)
        ast->result = (loli_sym *)emit->function_block->self;
    else {
        loli_ast *arg = ast->arg_start;

        if (return_type->flags & (TYPE_IS_UNRESOLVED | TYPE_HAS_SCOOP))
            return_type = loli_ts_resolve_unscoop(emit->ts, return_type);

        if (ast->first_tree_type == tree_variant) {
             
            arg = arg->next_arg;
        }

        loli_storage *s = NULL;

        for (;arg;arg = arg->next_arg) {
            if (arg->result->item_kind == ITEM_TYPE_STORAGE &&
                arg->result->type == return_type) {
                s = (loli_storage *)arg->result;
                break;
            }
        }

        if (s == NULL) {
            s = get_storage(emit, return_type);
            s->flags |= SYM_NOT_ASSIGNABLE;
        }

        ast->result = (loli_sym *)s;
    }
}

static void write_call(loli_emit_state *emit, loli_ast *ast,
        int argument_count, loli_storage *vararg_s)
{
    loli_ast *arg = ast->arg_start;
    int i = 0;

    loli_u16_write_3(emit->code, ast->call_op, ast->call_source_reg,
            argument_count + (vararg_s != NULL));

    for (arg = arg;
         i < argument_count;
         i++, arg = arg->next_arg)
        loli_u16_write_1(emit->code, arg->result->reg_spot);

    if (vararg_s)
        loli_u16_write_1(emit->code, vararg_s->reg_spot);

    loli_u16_write_2(emit->code, ast->result->reg_spot, ast->line_num);
}

static void write_call_keyopt(loli_emit_state *emit, loli_ast *ast,
        loli_type *call_type, int argument_count, loli_storage *vararg_s)
{
    loli_storage *s = get_storage(emit, loli_unset_type);

     
    s->expr_num = 0;

    loli_u16_write_3(emit->code, ast->call_op, ast->call_source_reg, 0);

    uint16_t arg_count_spot = loli_u16_pos(emit->code) - 1;
    uint16_t args_written = 0;
    uint16_t unset_reg_spot = s->reg_spot;
    uint16_t pos = ast->arg_start->keyword_arg_pos;
    uint16_t va_pos;
    loli_ast *arg = ast->arg_start;

     
    if (call_type->flags & TYPE_IS_VARARGS)
        va_pos = call_type->subtype_count - 2;
    else
        va_pos = (uint16_t)-1;

    while (1) {
        if (pos != args_written) {
             
            while (pos != args_written) {
                loli_u16_write_1(emit->code, unset_reg_spot);
                args_written++;
            }

            args_written = pos;
        }

        args_written++;

        if (pos == va_pos) {
            loli_u16_write_1(emit->code, vararg_s->reg_spot);
            break;
        }
        else {
            loli_u16_write_1(emit->code, arg->result->reg_spot);
            arg = arg->next_arg;

            if (arg)
                pos = arg->keyword_arg_pos;
            else if (va_pos != (uint16_t)-1 && vararg_s != NULL)
                 
                pos = va_pos;
            else
                 
                break;
        }
    }

    loli_u16_set_at(emit->code, arg_count_spot, args_written);
    loli_u16_write_2(emit->code, ast->result->reg_spot, ast->line_num);
}

static int eval_call_arg(loli_emit_state *emit, loli_ast *arg,
        loli_type *want_type)
{
    if (want_type->cls->id == LOLI_ID_OPTARG)
        want_type = want_type->subtypes[0];

    loli_type *eval_type = want_type;
    if (eval_type->flags & TYPE_IS_UNRESOLVED)
        eval_type = loli_ts_resolve(emit->ts, want_type);

    eval_tree(emit, arg, eval_type);
    loli_type *result_type = arg->result->type;

     
    if ((result_type->flags & TYPE_IS_UNRESOLVED) &&
        (arg->tree_type == tree_static_func ||
         arg->tree_type == tree_defined_func))
    {
         

        loli_type *solved_want = loli_ts_resolve(emit->ts, want_type);

        loli_ts_save_point p;
        loli_ts_scope_save(emit->ts, &p);
        loli_ts_check(emit->ts, result_type, solved_want);
        loli_type *solved_result = loli_ts_resolve(emit->ts, result_type);
        loli_ts_scope_restore(emit->ts, &p);

         
        if (solved_result == solved_want ||
            loli_ts_type_greater_eq(emit->ts, solved_want, solved_result))
            result_type = solved_result;
    }

     
    if (((want_type->flags & TYPE_IS_UNRESOLVED) &&
          loli_ts_check(emit->ts, want_type, result_type))
        ||
        (((want_type->flags & TYPE_IS_UNRESOLVED) == 0) &&
         loli_ts_type_greater_eq(emit->ts, want_type, result_type)))
        return 1;
    else
        return 0;
}

static void run_call(loli_emit_state *emit, loli_ast *ast,
        loli_type *call_type, loli_type *expect)
{
    loli_ast *arg = ast->arg_start;
    int num_args = ast->args_collected;
    unsigned int min, max;

    get_func_min_max(call_type, &min, &max);

    if (num_args < min || num_args > max)
        error_argument_count(emit, ast, num_args, min, max);

    loli_type **arg_types = call_type->subtypes;

    int stop;
    if ((call_type->flags & TYPE_IS_VARARGS) == 0 ||
        call_type->subtype_count - 1 > num_args)
        stop = num_args;
    else
        stop = call_type->subtype_count - 2;

    int i;
    for (i = 0; i < stop; i++, arg = arg->next_arg) {
        if (eval_call_arg(emit, arg, arg_types[i + 1]) == 0)
            error_bad_arg(emit, ast, call_type, i, arg->result->type);
    }

    loli_storage *vararg_s = NULL;

     
    if (call_type->flags & TYPE_IS_VARARGS &&
        (num_args + 2) >= call_type->subtype_count) {

         
        loli_type *vararg_type = arg_types[i + 1];
        loli_type *original_vararg = vararg_type;
        int is_optarg = 0;

         
        if (vararg_type->cls->id == LOLI_ID_OPTARG) {
            is_optarg = 1;
            original_vararg = original_vararg->subtypes[0];
            vararg_type = original_vararg->subtypes[0];
        }
        else
            vararg_type = vararg_type->subtypes[0];

        loli_ast *vararg_iter = arg;

        int vararg_i;
        for (vararg_i = i;
             arg != NULL;
             arg = arg->next_arg, vararg_i++) {
            if (eval_call_arg(emit, arg, vararg_type) == 0)
                error_bad_arg(emit, ast, call_type, vararg_i,
                        arg->result->type);
        }

        if (vararg_type->flags & TYPE_IS_UNRESOLVED)
            vararg_type = loli_ts_resolve(emit->ts, vararg_type);

        if (vararg_i != i || is_optarg == 0) {
            vararg_s = get_storage(emit, original_vararg);
            loli_u16_write_2(emit->code, o_build_list, vararg_i - i);
            for (;vararg_iter;vararg_iter = vararg_iter->next_arg)
                loli_u16_write_1(emit->code, vararg_iter->result->reg_spot);

            loli_u16_write_2(emit->code, vararg_s->reg_spot, ast->line_num);
        }
    }

    setup_call_result(emit, ast, arg_types[0]);
    write_call(emit, ast, stop, vararg_s);
}

static void begin_call(loli_emit_state *emit, loli_ast *ast,
        loli_type *expect, loli_type **call_type)
{
    loli_ast *first_arg = ast->arg_start;
    loli_tree_type first_tt = first_arg->tree_type;
    loli_sym *call_sym = NULL;
    uint16_t call_source_reg = (uint16_t)-1;
    uint16_t call_op = (uint8_t)-1;

    ast->first_tree_type = first_arg->tree_type;
    ast->keep_first_call_arg = 0;

    switch (first_tt) {
        case tree_method:
            call_sym = first_arg->sym;

             
            if ((call_sym->flags & VAR_IS_STATIC) == 0) {
                ast->keep_first_call_arg = 1;
                first_arg->tree_type = tree_self;
            }
            break;
        case tree_defined_func:
        case tree_inherited_new:
            call_sym = first_arg->sym;
            if (call_sym->flags & VAR_NEEDS_CLOSURE) {
                loli_storage *s = get_storage(emit, first_arg->sym->type);
                emit_create_function(emit, first_arg->sym, s);
                call_source_reg = s->reg_spot;
                call_op = o_call_register;
            }
            break;
        case tree_static_func:
            ensure_valid_scope(emit, first_arg->sym);
            call_sym = first_arg->sym;
            break;
        case tree_oo_access:
            eval_oo_access_for_item(emit, first_arg);
            if (first_arg->item->item_kind == ITEM_TYPE_PROPERTY) {
                oo_property_read(emit, first_arg);
                call_sym = (loli_sym *)first_arg->sym;
                call_source_reg = first_arg->result->reg_spot;
                call_op = o_call_register;
            }
            else {
                ast->keep_first_call_arg = 1;
                call_sym = first_arg->sym;
                 
                first_arg->tree_type = tree_oo_cached;
            }
            break;
        case tree_variant: {
            loli_variant_class *variant = first_arg->variant;
            if (variant->flags & CLS_EMPTY_VARIANT)
                loli_raise_syn(emit->raiser, "Variant %s should not get args.",
                        variant->name);

            ast->variant = variant;
            *call_type = variant->build_type;
            call_op = o_build_variant;
            call_source_reg = variant->cls_id;
            break;
        }
        case tree_global_var:
        case tree_upvalue:
            eval_tree(emit, first_arg, NULL);
            call_sym = (loli_sym *)first_arg->sym;
            call_source_reg = first_arg->result->reg_spot;
            call_op = o_call_register;
            break;
        default:
            eval_tree(emit, first_arg, NULL);
            call_sym = (loli_sym *)first_arg->result;
            break;
    }

    if (call_sym) {
        if (call_source_reg == (uint16_t)-1)
            call_source_reg = call_sym->reg_spot;

        if (call_op == (uint8_t)-1) {
            if (call_sym->flags & VAR_IS_READONLY) {
                if (call_sym->flags & VAR_IS_FOREIGN_FUNC)
                    call_op = o_call_foreign;
                else
                    call_op = o_call_native;
            }
            else
                call_op = o_call_register;
        }

        ast->sym = call_sym;
        *call_type = call_sym->type;

        if (call_sym->type->cls->id != LOLI_ID_FUNCTION)
            loli_raise_adjusted(emit->raiser, ast->line_num,
                    "Cannot anonymously call resulting type '^T'.",
                    call_sym->type);
    }

    if (ast->keep_first_call_arg == 0) {
        ast->arg_start = ast->arg_start->next_arg;
        ast->args_collected--;
    }

    ast->call_source_reg = call_source_reg;
    ast->call_op = call_op;
}

static void setup_typing_for_call(loli_emit_state *emit, loli_ast *ast,
        loli_type *expect, loli_type *call_type)
{
    loli_tree_type first_tt = ast->first_tree_type;

    if (first_tt == tree_local_var ||
        first_tt == tree_upvalue ||
        first_tt == tree_inherited_new)
         
        loli_ts_check(emit->ts, call_type, call_type);
    else if (expect)
         
        loli_ts_check(emit->ts, call_type->subtypes[0], expect);
}

static void eval_call(loli_emit_state *emit, loli_ast *ast, loli_type *expect)
{
    loli_type *call_type = NULL;
    begin_call(emit, ast, expect, &call_type);

    loli_ts_save_point p;
     
    loli_ts_scope_save(emit->ts, &p);

    if (call_type->flags & TYPE_IS_UNRESOLVED)
        setup_typing_for_call(emit, ast, expect, call_type);

    run_call(emit, ast, call_type, expect);
    loli_ts_scope_restore(emit->ts, &p);
}

static char *keypos_to_keyarg(char *arg_names, int pos)
{
    while (pos) {
        if (*arg_names == ' ')
            arg_names += 2;
        else
            arg_names += strlen(arg_names) + 1;

        pos--;
    }

    return arg_names;
}

static int keyarg_to_pos(const char *valid, const char *keyword_given)
{
    int index = 0;
    char *iter = (char *)valid;

    while (1) {
        if (*iter == ' ') {
            iter += 2;
            index++;
        }
        else if (*iter == '\t') {
            index = -1;
            break;
        }
        else if (strcmp(iter, keyword_given) == 0)
            break;
        else {
            iter += strlen(iter) + 1;
            index++;
        }
    }

    return index;
}

static loli_type *get_va_type(loli_type *call_type)
{
    loli_type *va_type;

    if (call_type->flags & TYPE_IS_VARARGS) {
        va_type = call_type->subtypes[call_type->subtype_count - 1];

        if (va_type->cls->id == LOLI_ID_OPTARG)
            va_type = va_type->subtypes[0];
    }
    else
         
        va_type = call_type;

    return va_type;
}

static loli_ast *relink_arg(loli_ast *source_ast, loli_ast *new_ast)
{
    if (source_ast->keyword_arg_pos > new_ast->keyword_arg_pos) {
        new_ast->next_arg = source_ast;
        new_ast->left = source_ast->left;
        source_ast = new_ast;
    }
    else {
        loli_ast *iter_ast = source_ast;
        while (1) {
            loli_ast *next_ast = iter_ast->next_arg;
            if (next_ast->keyword_arg_pos > new_ast->keyword_arg_pos) {
                new_ast->next_arg = next_ast;
                iter_ast->next_arg = new_ast;
                break;
            }

            iter_ast = next_ast;
        }
    }

    return source_ast;
}

static char *get_keyarg_names(loli_emit_state *emit, loli_ast *ast)
{
    char *names = NULL;

    if (ast->sym->item_kind == ITEM_TYPE_VAR) {
        loli_var *var = (loli_var *)ast->sym;

        if (var->flags & VAR_IS_READONLY) {
            loli_proto *p = loli_emit_proto_for_var(emit, var);

            names = p->arg_names;
        }
    }
    else if (ast->sym->item_kind == ITEM_TYPE_VARIANT)
        names = ast->variant->arg_names;

    return names;
}

static int keyarg_at_pos(loli_ast *ast, loli_ast *arg_stop, int pos)
{
    int found = 0;
    loli_ast *arg;

    for (arg = ast->arg_start; arg != arg_stop; arg = arg->next_arg) {
        if (arg->keyword_arg_pos == pos) {
            found = 1;
            break;
        }
    }

    return found;
}

static void keyargs_mark_and_verify(loli_emit_state *emit, loli_ast *ast,
        loli_type *call_type)
{
    loli_ast *arg = ast->arg_start;
    int num_args = ast->args_collected, have_keyargs = 0, va_pos = INT_MAX;
    int i;

    if (call_type->flags & TYPE_IS_VARARGS)
        va_pos = call_type->subtype_count - 2;

    char *keyword_names = get_keyarg_names(emit, ast);

    if (keyword_names == NULL)
        error_keyarg_not_supported(emit, ast);

    for (i = 0; arg != NULL; i++, arg = arg->next_arg) {
        int pos;

        if (arg->tree_type == tree_binary &&
            arg->op == expr_named_arg) {
            have_keyargs = 1;
            char *key_name = loli_sp_get(emit->expr_strings,
                    arg->left->pile_pos);

            pos = keyarg_to_pos(keyword_names, key_name);

            if (pos == -1)
                error_keyarg_not_valid(emit, ast, arg);

            if (va_pos <= pos)
                num_args--;
            else if (keyarg_at_pos(ast, arg, pos))
                error_keyarg_duplicate(emit, ast, arg);
        }
        else if (have_keyargs == 0) {
            if (i > va_pos)
                pos = va_pos;
            else
                pos = i;
        }
        else
            error_keyarg_before_posarg(emit, arg);

        arg->keyword_arg_pos = pos;
    }

    unsigned int min, max;

    get_func_min_max(call_type, &min, &max);

    if (min > num_args)
        error_keyarg_missing_params(emit, ast, call_type, keyword_names);
}

static void run_named_call(loli_emit_state *emit, loli_ast *ast,
        loli_type *call_type, loli_type *expect)
{
    int num_args = ast->args_collected;
    unsigned int min, max;

    get_func_min_max(call_type, &min, &max);

     
    if (num_args > max)
        error_argument_count(emit, ast, num_args, min, max);

     
    keyargs_mark_and_verify(emit, ast, call_type);

     

    loli_type **arg_types = call_type->subtypes;
    loli_type *va_elem_type = get_va_type(call_type)->subtypes[0];
    loli_ast *arg = ast->arg_start;
    loli_ast *next_arg = arg->next_arg;
    loli_ast *basic_arg_head = NULL;
    loli_ast *var_arg_head = NULL;
    int base_count = 0, va_count = 0, va_pos = INT_MAX;
    int i;

    if (call_type->flags & TYPE_IS_VARARGS)
        va_pos = call_type->subtype_count - 2;

    for (i = 0; arg != NULL; i++) {
        loli_ast *real_arg = arg;
        int is_vararg = 0;
        uint16_t pos = arg->keyword_arg_pos;

        if (arg->tree_type == tree_binary && arg->op == expr_named_arg)
            real_arg = arg->right;

        loli_type *arg_type;

        if (va_pos > pos)
            arg_type = arg_types[pos + 1];
        else {
            arg_type = va_elem_type;
            is_vararg = 1;
        }

        if (eval_call_arg(emit, real_arg, arg_type) == 0)
            error_bad_arg(emit, ast, call_type, pos, real_arg->result->type);

         
        arg->result = real_arg->result;

         
        next_arg = arg->next_arg;
        arg->next_arg = NULL;

         
        arg->keyword_arg_pos = pos;

         
        if (is_vararg == 0) {
             
            if (basic_arg_head == NULL) {
                basic_arg_head = arg;
                basic_arg_head->left = arg;
            }
            else if (basic_arg_head->left->keyword_arg_pos
                     < arg->keyword_arg_pos) {
                basic_arg_head->left->next_arg = arg;
                basic_arg_head->left = arg;
            }
            else
                basic_arg_head = relink_arg(basic_arg_head, arg);

            base_count++;
        }
        else {
             
            if (var_arg_head == NULL) {
                var_arg_head = arg;
                var_arg_head->left = arg;
            }
            else {
                var_arg_head->left->next_arg = arg;
                var_arg_head->left = arg;
            }

            va_count++;
        }

        arg = next_arg;
    }

    loli_storage *vararg_s = NULL;

    if (va_pos != INT_MAX) {
        loli_type *va_type = call_type->subtypes[call_type->subtype_count - 1];

        if (va_type->cls->id != LOLI_ID_OPTARG ||
            var_arg_head) {
        loli_type *va_list_type = get_va_type(call_type);

        if (va_list_type->flags & TYPE_IS_UNRESOLVED)
            va_list_type = loli_ts_resolve(emit->ts, va_list_type);

        vararg_s = get_storage(emit, va_list_type);
        loli_u16_write_2(emit->code, o_build_list, va_count);

        for (;var_arg_head;var_arg_head = var_arg_head->next_arg)
            loli_u16_write_1(emit->code, var_arg_head->result->reg_spot);

        loli_u16_write_2(emit->code, vararg_s->reg_spot, ast->line_num);
        }
    }

    ast->arg_start = basic_arg_head;
    setup_call_result(emit, ast, arg_types[0]);

    if ((call_type->flags & TYPE_HAS_OPTARGS) == 0)
        write_call(emit, ast, base_count, vararg_s);
    else
        write_call_keyopt(emit, ast, call_type, base_count, vararg_s);
}

static void eval_named_call(loli_emit_state *emit, loli_ast *ast,
        loli_type *expect)
{
    loli_type *call_type = NULL;
    begin_call(emit, ast, expect, &call_type);

    loli_ts_save_point p;
     
    loli_ts_scope_save(emit->ts, &p);

    if (call_type->flags & TYPE_IS_UNRESOLVED)
        setup_typing_for_call(emit, ast, expect, call_type);

    run_named_call(emit, ast, call_type, expect);
    loli_ts_scope_restore(emit->ts, &p);
}

static void eval_variant(loli_emit_state *emit, loli_ast *ast,
        loli_type *expect)
{
    loli_variant_class *variant = ast->variant;
     
    if ((variant->flags & CLS_EMPTY_VARIANT) == 0) {
        unsigned int min, max;
        get_func_min_max(variant->build_type, &min, &max);
        ast->keep_first_call_arg = 0;
        error_argument_count(emit, ast, -1, min, max);
    }

    loli_u16_write_2(emit->code, o_load_empty_variant, variant->cls_id);

    loli_type *storage_type;

    if (variant->parent->generic_count) {
        loli_type *self_type = variant->parent->self_type;
        loli_ts_save_point p;
        loli_ts_scope_save(emit->ts, &p);

         
        if (expect && expect->cls == variant->parent)
            loli_ts_check(emit->ts, self_type, expect);

        storage_type = loli_ts_resolve(emit->ts, self_type);

        loli_ts_scope_restore(emit->ts, &p);
    }
    else
        storage_type = variant->parent->self_type;

    loli_storage *s = get_storage(emit, storage_type);
    loli_u16_write_2(emit->code, s->reg_spot, ast->line_num);
    ast->result = (loli_sym *)s;
}

static void eval_func_pipe(loli_emit_state *emit, loli_ast *ast,
        loli_type *expect)
{
     
    ast->right->next_arg = ast->left;
     
    ast->args_collected = 2;
     
    ast->tree_type = tree_call;
    eval_call(emit, ast, expect);
}

static void eval_plus_plus(loli_emit_state *emit, loli_ast *ast)
{
    if (ast->left->tree_type != tree_local_var)
        eval_tree(emit, ast->left, NULL);

    if (ast->right->tree_type != tree_local_var)
        eval_tree(emit, ast->right, NULL);

    if (ast->parent == NULL ||
        (ast->parent->tree_type != tree_binary ||
         ast->parent->op != expr_plus_plus)) {
        loli_u16_write_2(emit->code, o_interpolation, 0);

        int fix_spot = loli_u16_pos(emit->code) - 1;
        loli_ast *iter_ast = ast->left;
        loli_storage *s = get_storage(emit,
                emit->symtab->string_class->self_type);

        while (1) {
            if (iter_ast->tree_type != tree_binary ||
                iter_ast->op != expr_plus_plus)
                break;

            iter_ast = iter_ast->left;
        }

        iter_ast = iter_ast->parent;
        loli_u16_write_1(emit->code, iter_ast->left->result->reg_spot);

        while (iter_ast != ast) {
            loli_u16_write_1(emit->code, iter_ast->right->result->reg_spot);
            iter_ast = iter_ast->parent;
        }

        loli_u16_write_1(emit->code, iter_ast->right->result->reg_spot);

        loli_u16_set_at(emit->code, fix_spot,
                loli_u16_pos(emit->code) - fix_spot - 1);

        loli_u16_write_2(emit->code, s->reg_spot, ast->line_num);

        ast->result = (loli_sym *)s;
    }
}



static void eval_tree(loli_emit_state *emit, loli_ast *ast, loli_type *expect)
{
    if (ast->tree_type == tree_global_var ||
        ast->tree_type == tree_defined_func ||
        ast->tree_type == tree_static_func ||
        ast->tree_type == tree_method ||
        ast->tree_type == tree_inherited_new ||
        ast->tree_type == tree_upvalue)
        emit_nonlocal_var(emit, ast);
    else if (ast->tree_type == tree_literal)
        emit_literal(emit, ast);
    else if (ast->tree_type == tree_integer)
        emit_integer(emit, ast);
    else if (ast->tree_type == tree_byte)
        emit_byte(emit, ast);
    else if (ast->tree_type == tree_boolean)
        emit_boolean(emit, ast);
    else if (ast->tree_type == tree_call)
        eval_call(emit, ast, expect);
    else if (ast->tree_type == tree_binary) {
        if (ast->op >= expr_assign)
            eval_assign(emit, ast);
        else if (ast->op == expr_logical_or || ast->op == expr_logical_and)
            eval_logical_op(emit, ast);
        else if (ast->op == expr_func_pipe)
            eval_func_pipe(emit, ast, expect);
        else if (ast->op == expr_plus_plus)
            eval_plus_plus(emit, ast);
        else {
            if (ast->left->tree_type != tree_local_var)
                eval_tree(emit, ast->left, NULL);

            if (ast->right->tree_type != tree_local_var)
                eval_tree(emit, ast->right, ast->left->result->type);

            emit_binary_op(emit, ast);
        }
    }
    else if (ast->tree_type == tree_parenth) {
        loli_ast *start = ast->arg_start;

        eval_tree(emit, start, expect);
        ast->result = start->result;
   }
    else if (ast->tree_type == tree_unary)
        eval_unary_op(emit, ast);
    else if (ast->tree_type == tree_list)
        eval_build_list(emit, ast, expect);
    else if (ast->tree_type == tree_hash)
        eval_build_hash(emit, ast, expect);
    else if (ast->tree_type == tree_tuple)
        eval_build_tuple(emit, ast, expect);
    else if (ast->tree_type == tree_subscript)
        eval_subscript(emit, ast, expect);
    else if (ast->tree_type == tree_typecast)
        eval_typecast(emit, ast);
    else if (ast->tree_type == tree_oo_access)
        eval_oo_access(emit, ast);
    else if (ast->tree_type == tree_property)
        eval_property(emit, ast);
    else if (ast->tree_type == tree_variant)
        eval_variant(emit, ast, expect);
    else if (ast->tree_type == tree_lambda)
        eval_lambda(emit, ast, expect);
    else if (ast->tree_type == tree_self)
        eval_self(emit, ast);
    else if (ast->tree_type == tree_oo_cached)
        ast->result = ast->arg_start->result;
    else if (ast->tree_type == tree_named_call)
        eval_named_call(emit, ast, expect);
}

static void eval_enforce_value(loli_emit_state *emit, loli_ast *ast,
        loli_type *expect, const char *message)
{
    eval_tree(emit, ast, expect);
    emit->expr_num++;

    if (ast->result == NULL)
        loli_raise_syn(emit->raiser, message);
}

void loli_emit_eval_expr(loli_emit_state *emit, loli_expr_state *es)
{
    eval_tree(emit, es->root, NULL);
    emit->expr_num++;
}

void loli_emit_eval_expr_to_var(loli_emit_state *emit, loli_expr_state *es,
        loli_var *var)
{
    loli_ast *ast = es->root;

    eval_tree(emit, ast, NULL);
    emit->expr_num++;

    if (ast->result->type->cls->id != LOLI_ID_INTEGER) {
        loli_raise_syn(emit->raiser,
                   "Expected type 'Integer', but got type '^T'.",
                   ast->result->type);
    }

     
    loli_u16_write_4(emit->code, o_assign_noref, ast->result->reg_spot,
            var->reg_spot, ast->line_num);
}

void loli_emit_eval_condition(loli_emit_state *emit, loli_expr_state *es)
{
    loli_ast *ast = es->root;
    loli_block_type current_type = emit->block->block_type;

    if (((ast->tree_type == tree_boolean ||
          ast->tree_type == tree_byte ||
          ast->tree_type == tree_integer) &&
          ast->backing_value != 0) == 0) {
        eval_enforce_value(emit, ast, NULL,
                "Conditional expression has no value.");
        ensure_valid_condition_type(emit, ast->result->type);

        if (current_type != block_do_while)
            emit_jump_if(emit, ast, 0);
        else {
            int location = loli_u16_pos(emit->code) - emit->block->code_start;
            loli_u16_write_4(emit->code, o_jump_if, 1, ast->result->reg_spot,
                    (uint16_t)-location);
        }
    }
    else {
        if (current_type != block_do_while) {
             
            loli_u16_write_1(emit->patches, 0);
        }
        else {
             
            int location = loli_u16_pos(emit->code) - emit->block->code_start;
            loli_u16_write_2(emit->code, o_jump, (uint16_t)-location);
        }
    }
}

void loli_emit_eval_lambda_body(loli_emit_state *emit, loli_expr_state *es,
        loli_type *full_type)
{
    loli_type *wanted_type = NULL;
    if (full_type)
        wanted_type = full_type->subtypes[0];

    eval_tree(emit, es->root, wanted_type);

    loli_sym *root_result = es->root->result;

     
    if (root_result) {
        loli_u16_write_3(emit->code, o_return_value, root_result->reg_spot,
                es->root->line_num);
        emit->block->last_exit = loli_u16_pos(emit->code);
    }
}

void loli_emit_eval_return(loli_emit_state *emit, loli_expr_state *es,
        loli_type *return_type)
{
    if (return_type != loli_unit_type) {
        loli_ast *ast = es->root;

        eval_enforce_value(emit, ast, return_type,
                "'return' expression has no value.");

        if (ast->result->type != return_type &&
            type_matchup(emit, return_type, ast) == 0) {
            loli_raise_adjusted(emit->raiser, ast->line_num,
                    "return expected type '^T' but got type '^T'.", return_type,
                    ast->result->type);
        }

        write_pop_try_blocks_up_to(emit, emit->function_block);
        loli_u16_write_3(emit->code, o_return_value, ast->result->reg_spot,
                ast->line_num);
        emit->block->last_exit = loli_u16_pos(emit->code);
    }
    else {
        write_pop_try_blocks_up_to(emit, emit->function_block);
        loli_u16_write_2(emit->code, o_return_unit, *emit->lex_linenum);
    }
}

void loli_emit_raise(loli_emit_state *emit, loli_expr_state *es)
{
    loli_ast *ast = es->root;
    eval_enforce_value(emit, ast, NULL, "'raise' expression has no value.");

    loli_class *result_cls = ast->result->type->cls;
    if (loli_class_greater_eq_id(LOLI_ID_EXCEPTION, result_cls) == 0) {
        loli_raise_syn(emit->raiser, "Invalid class '%s' given to raise.",
                result_cls->name);
    }

    loli_u16_write_3(emit->code, o_exception_raise, ast->result->reg_spot,
            ast->line_num);
    emit->block->last_exit = loli_u16_pos(emit->code);
}

void loli_reset_main(loli_emit_state *emit)
{
    emit->code->pos = 0;
}


void loli_prepare_main(loli_emit_state *emit, loli_function_val *main_func)
{
    int register_count = emit->main_block->next_reg_spot;

    loli_u16_write_1(emit->code, o_vm_exit);

    main_func->code_len = loli_u16_pos(emit->code);
    main_func->code = emit->code->data;
    main_func->proto->code = main_func->code;
    main_func->reg_count = register_count;
}
