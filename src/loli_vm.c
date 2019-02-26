#include <stdarg.h>
#include <stddef.h>
#include <string.h>

#include "loli.h"

#include "loli_alloc.h"
#include "loli_vm.h"
#include "loli_parser.h"
#include "loli_value_stack.h"
#include "loli_value_flags.h"
#include "loli_value_raw.h"

#include "loli_int_opcode.h"

extern loli_gc_entry *loli_gc_stopper;
void loli_value_destroy(loli_value *);
void loli_mb_escape_add_str(loli_msgbuf *, const char *);

static uint16_t foreign_code[1] = {o_vm_exit};

#define SAVE_LINE(to_add) \
current_frame->code = code + to_add

#define INITIAL_REGISTER_COUNT 16


static void add_call_frame(loli_vm_state *);
static void invoke_gc(loli_vm_state *);

static loli_vm_state *new_vm_state(loli_raiser *raiser, int count)
{
    loli_vm_catch_entry *catch_entry = loli_malloc(sizeof(*catch_entry));
    catch_entry->prev = NULL;
    catch_entry->next = NULL;

    int i;
    loli_value **register_base = loli_malloc(count * sizeof(*register_base));

    for (i = 0;i < count;i++) {
        register_base[i] = loli_malloc(sizeof(*register_base[i]));
        register_base[i]->flags = 0;
    }

    loli_value **register_end = register_base + count;

     
    loli_call_frame *toplevel_frame = loli_malloc(sizeof(*toplevel_frame));

     
    loli_call_frame *first_frame = loli_malloc(sizeof(*toplevel_frame));

    toplevel_frame->start = register_base;
    toplevel_frame->top = register_base;
    toplevel_frame->register_end = register_end;
    toplevel_frame->code = NULL;
    toplevel_frame->return_target = NULL;
    toplevel_frame->prev = NULL;
    toplevel_frame->next = first_frame;
    first_frame->start = register_base;
    first_frame->top = register_base;
    first_frame->register_end = register_end;
    first_frame->code = NULL;
    first_frame->function = NULL;
    first_frame->return_target = register_base[0];
    first_frame->prev = toplevel_frame;
    first_frame->next = NULL;

    loli_vm_state *vm = loli_malloc(sizeof(*vm));

    vm->call_depth = 0;
    vm->depth_max = 100;
    vm->raiser = raiser;
    vm->catch_chain = NULL;
    vm->call_chain = NULL;
    vm->exception_value = NULL;
    vm->exception_cls = NULL;
    vm->catch_chain = catch_entry;
     
    vm->call_chain = toplevel_frame;
    vm->vm_buffer = loli_new_msgbuf(64);
    vm->register_root = register_base;

    return vm;
}

loli_vm_state *loli_new_vm_state(loli_raiser *raiser)
{
    loli_vm_state *vm = new_vm_state(raiser, INITIAL_REGISTER_COUNT);
    loli_global_state *gs = loli_malloc(sizeof(*gs));

    gs->regs_from_main = vm->call_chain->start;
    gs->class_table = NULL;
    gs->readonly_table = NULL;
    gs->class_count = 0;
    gs->readonly_count = 0;
    gs->gc_live_entries = NULL;
    gs->gc_spare_entries = NULL;
    gs->gc_live_entry_count = 0;
    gs->gc_pass = 0;
    gs->first_vm = vm;

    vm->gs = gs;

    return vm;
}

void loli_destroy_vm(loli_vm_state *vm)
{
    loli_value **register_root = vm->register_root;
    loli_value *reg;
    int i;
    if (vm->catch_chain != NULL) {
        while (vm->catch_chain->prev)
            vm->catch_chain = vm->catch_chain->prev;

        loli_vm_catch_entry *catch_iter = vm->catch_chain;
        loli_vm_catch_entry *catch_next;
        while (catch_iter) {
            catch_next = catch_iter->next;
            loli_free(catch_iter);
            catch_iter = catch_next;
        }
    }

    int total = vm->call_chain->register_end - register_root - 1;

    for (i = total;i >= 0;i--) {
        reg = register_root[i];

        loli_deref(reg);

        loli_free(reg);
    }

    loli_free(register_root);

    loli_call_frame *frame_iter = vm->call_chain;
    loli_call_frame *frame_next;

    while (frame_iter->prev)
        frame_iter = frame_iter->prev;

    while (frame_iter) {
        frame_next = frame_iter->next;
        loli_free(frame_iter);
        frame_iter = frame_next;
    }

    loli_free_msgbuf(vm->vm_buffer);
}

static void destroy_gc_entries(loli_vm_state *vm)
{
    loli_global_state *gs = vm->gs;
    loli_gc_entry *gc_iter, *gc_temp;

    if (gs->gc_live_entry_count) {
         
        for (gc_iter = gs->gc_live_entries;
             gc_iter;
             gc_iter = gc_iter->next) {
            if (gc_iter->value.generic != NULL) {
                 
                gc_iter->last_pass = -1;
                loli_value_destroy((loli_value *)gc_iter);
            }
        }

        gc_iter = gs->gc_live_entries;

        while (gc_iter) {
            gc_temp = gc_iter->next;

             
            loli_free(gc_iter->value.generic);
            loli_free(gc_iter);

            gc_iter = gc_temp;
        }
    }

    gc_iter = vm->gs->gc_spare_entries;
    while (gc_iter != NULL) {
        gc_temp = gc_iter->next;

        loli_free(gc_iter);

        gc_iter = gc_temp;
    }
}

void loli_free_vm(loli_vm_state *vm)
{
     
    if (vm->gs->gc_live_entry_count)
        invoke_gc(vm);

    loli_destroy_vm(vm);

    destroy_gc_entries(vm);

    loli_free(vm->gs->class_table);
    loli_free(vm->gs);
    loli_free(vm);
}


static void gc_mark(int, loli_value *);

static void invoke_gc(loli_vm_state *vm)
{
     
    vm = vm->gs->first_vm;

     
    vm->gs->gc_pass++;

    loli_value **regs_from_main = vm->gs->regs_from_main;
    int pass = vm->gs->gc_pass;
    int i;
    loli_gc_entry *gc_iter;
    int total = vm->call_chain->register_end - vm->gs->regs_from_main;

     
    for (i = 0;i < total;i++) {
        loli_value *reg = regs_from_main[i];
        if (reg->flags & VAL_HAS_SWEEP_FLAG)
            gc_mark(pass, reg);
    }

     
    for (gc_iter = vm->gs->gc_live_entries;
         gc_iter;
         gc_iter = gc_iter->next) {
        if (gc_iter->last_pass != pass &&
            gc_iter->value.generic != NULL) {
             
            gc_iter->last_pass = -1;
            loli_value_destroy((loli_value *)gc_iter);
        }
    }

    int current_top = vm->call_chain->top - vm->gs->regs_from_main;

     
    for (i = total;i < current_top;i++) {
        loli_value *reg = regs_from_main[i];
        if (reg->flags & VAL_IS_GC_TAGGED &&
            reg->value.gc_generic->gc_entry == loli_gc_stopper) {
            reg->flags = 0;
        }
    }

     
    i = 0;
    loli_gc_entry *new_live_entries = NULL;
    loli_gc_entry *new_spare_entries = vm->gs->gc_spare_entries;
    loli_gc_entry *iter_next = NULL;
    gc_iter = vm->gs->gc_live_entries;

    while (gc_iter) {
        iter_next = gc_iter->next;

        if (gc_iter->last_pass == -1) {
            loli_free(gc_iter->value.generic);

            gc_iter->next = new_spare_entries;
            new_spare_entries = gc_iter;
        }
        else {
            i++;
            gc_iter->next = new_live_entries;
            new_live_entries = gc_iter;
        }

        gc_iter = iter_next;
    }

     
    if (vm->gs->gc_threshold <= i)
        vm->gs->gc_threshold *= vm->gs->gc_multiplier;

    vm->gs->gc_live_entry_count = i;
    vm->gs->gc_live_entries = new_live_entries;
    vm->gs->gc_spare_entries = new_spare_entries;
}

static void list_marker(int pass, loli_value *v)
{
    if (v->flags & VAL_IS_GC_TAGGED) {
         
        loli_gc_entry *e = v->value.container->gc_entry;
        if (e->last_pass == pass)
            return;

        e->last_pass = pass;
    }

    loli_container_val *list_val = v->value.container;
    int i;

    for (i = 0;i < list_val->num_values;i++) {
        loli_value *elem = list_val->values[i];

        if (elem->flags & VAL_HAS_SWEEP_FLAG)
            gc_mark(pass, elem);
    }
}

static void hash_marker(int pass, loli_value *v)
{
    loli_hash_val *hv = v->value.hash;
    int i;

    for (i = 0;i < hv->num_bins;i++) {
        loli_hash_entry *entry = hv->bins[i];
        if (entry)
            gc_mark(pass, entry->record);
    }
}

static void function_marker(int pass, loli_value *v)
{
    if (v->flags & VAL_IS_GC_TAGGED) {
        loli_gc_entry *e = v->value.function->gc_entry;
        if (e->last_pass == pass)
            return;

        e->last_pass = pass;
    }

    loli_function_val *function_val = v->value.function;

    loli_value **upvalues = function_val->upvalues;
    int count = function_val->num_upvalues;
    int i;

    for (i = 0;i < count;i++) {
        loli_value *up = upvalues[i];
        if (up && (up->flags & VAL_HAS_SWEEP_FLAG))
            gc_mark(pass, up);
    }
}

static void coroutine_marker(int pass, loli_value *v)
{
    if (v->flags & VAL_IS_GC_TAGGED) {
        loli_gc_entry *e = v->value.function->gc_entry;
        if (e->last_pass == pass)
            return;

        e->last_pass = pass;
    }

    loli_coroutine_val *co_val = v->value.coroutine;
    loli_vm_state *co_vm = co_val->vm;
    loli_value **base = co_vm->register_root;
    int total = co_vm->call_chain->register_end - base - 1;
    int i;

    for (i = total;i >= 0;i--) {
        loli_value *v = base[i];

        if (v->flags & VAL_HAS_SWEEP_FLAG)
            gc_mark(pass, v);
    }

    loli_function_val *base_function = co_val->base_function;

    if (base_function->upvalues) {
         
        loli_value v;
        v.flags = V_FUNCTION_BASE;
        v.value.function = base_function;
        function_marker(pass, &v);
    }

    loli_value *receiver = co_val->receiver;

    if (receiver->flags & VAL_HAS_SWEEP_FLAG)
        gc_mark(pass, receiver);
}

static void gc_mark(int pass, loli_value *v)
{
    if (v->flags & (VAL_IS_GC_TAGGED | VAL_IS_GC_SPECULATIVE)) {
        int base = FLAGS_TO_BASE(v);

        if (base == V_LIST_BASE     || base == V_TUPLE_BASE ||
            base == V_INSTANCE_BASE || base == V_VARIANT_BASE)
            list_marker(pass, v);
        else if (base == V_HASH_BASE)
            hash_marker(pass, v);
        else if (base == V_FUNCTION_BASE)
            function_marker(pass, v);
        else if (base == V_COROUTINE_BASE)
            coroutine_marker(pass, v);
    }
}

void loli_value_tag(loli_vm_state *vm, loli_value *v)
{
    loli_global_state *gs = vm->gs;

    if (gs->gc_live_entry_count >= gs->gc_threshold)
         
        invoke_gc(gs->first_vm);

    loli_gc_entry *new_entry;
    if (gs->gc_spare_entries != NULL) {
        new_entry = gs->gc_spare_entries;
        gs->gc_spare_entries = gs->gc_spare_entries->next;
    }
    else
        new_entry = loli_malloc(sizeof(*new_entry));

    new_entry->value.gc_generic = v->value.gc_generic;
    new_entry->last_pass = 0;
    new_entry->flags = v->flags;

    new_entry->next = gs->gc_live_entries;
    gs->gc_live_entries = new_entry;

     
    v->value.gc_generic->gc_entry = new_entry;
    gs->gc_live_entry_count++;

    v->flags |= VAL_IS_GC_TAGGED;
}



static void vm_error(loli_vm_state *, uint8_t, const char *);

static void grow_vm_registers(loli_vm_state *vm, int need)
{
    loli_value **old_start = vm->register_root;
    int size = vm->call_chain->register_end - old_start;
    int i = size;

    need += size;

    do
        size *= 2;
    while (size < need);

    loli_value **new_regs = loli_realloc(old_start, size * sizeof(*new_regs));

    if (vm == vm->gs->first_vm)
        vm->gs->regs_from_main = new_regs;

     
    for (;i < size;i++) {
        loli_value *v = loli_malloc(sizeof(*v));
        v->flags = 0;

        new_regs[i] = v;
    }

    loli_value **end = new_regs + size;
    loli_call_frame *frame = vm->call_chain;

    while (frame) {
        frame->start = new_regs + (frame->start - old_start);
        frame->top = new_regs + (frame->top - old_start);
        frame->register_end = end;
        frame = frame->prev;
    }

    frame = vm->call_chain->next;
    while (frame) {
        frame->register_end = end;
        frame = frame->next;
    }

    vm->register_root = new_regs;
}

static void vm_setup_before_call(loli_vm_state *vm, uint16_t *code)
{
    loli_call_frame *current_frame = vm->call_chain;
    if (current_frame->next == NULL) {
        if (vm->call_depth > vm->depth_max) {
            SAVE_LINE(code[2] + 5);
            vm_error(vm, LOLI_ID_RUNTIMEERROR,
                    "Function call recursion limit reached.");
        }

        add_call_frame(vm);
    }

    int i = code[2];
    current_frame->code = code + i + 5;

    loli_call_frame *next_frame = current_frame->next;
    next_frame->start = current_frame->top;
    next_frame->code = NULL;
    next_frame->return_target = current_frame->start[code[i + 3]];
}

static void clear_extra_registers(loli_call_frame *next_frame, uint16_t *code)
{
    int i = code[2];
    loli_value **target_regs = next_frame->start;

    for (;i < next_frame->function->reg_count;i++) {
        loli_value *reg = target_regs[i];
        loli_deref(reg);

        reg->flags = 0;
    }
}

static void prep_registers(loli_call_frame *frame, uint16_t *code)
{
    loli_call_frame *next_frame = frame->next;
    int i;
    loli_value **input_regs = frame->start;
    loli_value **target_regs = next_frame->start;

     
    for (i = 0;i < code[2];i++) {
        loli_value *get_reg = input_regs[code[3+i]];
        loli_value *set_reg = target_regs[i];

        if (get_reg->flags & VAL_IS_DEREFABLE)
            get_reg->value.generic->refcount++;

        if (set_reg->flags & VAL_IS_DEREFABLE)
            loli_deref(set_reg);

        *set_reg = *get_reg;
    }
}

static loli_string_val *new_sv(char *buffer, int size)
{
    loli_string_val *sv = loli_malloc(sizeof(*sv));
    sv->refcount = 1;
    sv->string = buffer;
    sv->size = size;
    return sv;
}

loli_bytestring_val *loli_new_bytestring_raw(const char *source, int len)
{
    char *buffer = loli_malloc((len + 1) * sizeof(*buffer));
    memcpy(buffer, source, len);
    buffer[len] = '\0';

    return (loli_bytestring_val *)new_sv(buffer, len);
}

loli_string_val *loli_new_string_raw(const char *source)
{
    size_t len = strlen(source);
    char *buffer = loli_malloc((len + 1) * sizeof(*buffer));
    strcpy(buffer, source);

    return new_sv(buffer, len);
}

static loli_container_val *new_container(uint16_t class_id, int num_values)
{
    loli_container_val *cv = loli_malloc(sizeof(*cv));
    cv->values = loli_malloc(num_values * sizeof(*cv->values));
    cv->refcount = 1;
    cv->num_values = num_values;
    cv->extra_space = 0;
    cv->class_id = class_id;
    cv->gc_entry = NULL;

    int i;
    for (i = 0;i < num_values;i++) {
        loli_value *elem = loli_malloc(sizeof(*elem));
        elem->flags = 0;
        cv->values[i] = elem;
    }

    return cv;
}

static void move_byte(loli_value *v, uint8_t z)
{
    if (v->flags & VAL_IS_DEREFABLE)
        loli_deref(v);

    v->value.integer = z;
    v->flags = V_BYTE_FLAG | V_BYTE_BASE;
}

static void move_function_f(uint32_t f, loli_value *v, loli_function_val *z)
{
    if (v->flags & VAL_IS_DEREFABLE)
        loli_deref(v);

    v->value.function = z;
    v->flags = f | V_FUNCTION_BASE | V_FUNCTION_BASE | VAL_IS_DEREFABLE;
}

static void move_hash_f(uint32_t f, loli_value *v, loli_hash_val *z)
{
    if (v->flags & VAL_IS_DEREFABLE)
        loli_deref(v);

    v->value.hash = z;
    v->flags = f | V_HASH_BASE | VAL_IS_DEREFABLE;
}

static void move_instance_f(uint32_t f, loli_value *v, loli_container_val *z)
{
    if (v->flags & VAL_IS_DEREFABLE)
        loli_deref(v);

    v->value.container = z;
    v->flags = f | V_INSTANCE_BASE | VAL_IS_DEREFABLE;
}

static void move_list_f(uint32_t f, loli_value *v, loli_container_val *z)
{
    if (v->flags & VAL_IS_DEREFABLE)
        loli_deref(v);

    v->value.container = z;
    v->flags = f | V_LIST_BASE | VAL_IS_DEREFABLE;
}

static void move_string(loli_value *v, loli_string_val *z)
{
    if (v->flags & VAL_IS_DEREFABLE)
        loli_deref(v);

    v->value.string = z;
    v->flags = VAL_IS_DEREFABLE | V_STRING_FLAG | V_STRING_BASE;
}

static void move_tuple_f(uint32_t f, loli_value *v, loli_container_val *z)
{
    if (v->flags & VAL_IS_DEREFABLE)
        loli_deref(v);

    v->value.container = z;
    v->flags = f | VAL_IS_DEREFABLE | V_TUPLE_BASE;
}

static void move_unit(loli_value *v)
{
    if (v->flags & VAL_IS_DEREFABLE)
        loli_deref(v);

    v->value.integer = 0;
    v->flags = V_UNIT_BASE;
}

static void move_variant_f(uint32_t f, loli_value *v, loli_container_val *z)
{
    if (v->flags & VAL_IS_DEREFABLE)
        loli_deref(v);

    v->value.container = z;
    v->flags = f | VAL_IS_DEREFABLE | V_VARIANT_BASE;
}

#define PUSH_PREAMBLE \
loli_call_frame *frame = s->call_chain; \
if (frame->top == frame->register_end) { \
    grow_vm_registers(s, 1); \
} \
 \
loli_value *target = *frame->top; \
if (target->flags & VAL_IS_DEREFABLE) \
    loli_deref(target); \
 \
frame->top++;

#define SET_TARGET(push_flags, field, push_value) \
target->flags = push_flags; \
target->value.field = push_value

#define PUSH_CONTAINER(id, container_flags, size) \
PUSH_PREAMBLE \
loli_container_val *c = new_container(id, size); \
SET_TARGET(VAL_IS_DEREFABLE | container_flags, container, c); \
return c

static void push_coroutine(loli_state *s, loli_coroutine_val *co)
{
    PUSH_PREAMBLE
    SET_TARGET(V_COROUTINE_BASE | VAL_IS_DEREFABLE, coroutine, co);
}

void loli_push_boolean(loli_state *s, int v)
{
    PUSH_PREAMBLE
    SET_TARGET(V_BOOLEAN_BASE, integer, v);
}

void loli_push_bytestring(loli_state *s, const char *source, int len)
{
    PUSH_PREAMBLE
    char *buffer = loli_malloc((len + 1) * sizeof(*buffer));
    memcpy(buffer, source, len);
    buffer[len] = '\0';

    loli_string_val *sv = new_sv(buffer, len);

    SET_TARGET(V_BYTESTRING_FLAG | V_BYTESTRING_BASE | VAL_IS_DEREFABLE, string, sv);
}

void loli_push_byte(loli_state *s, uint8_t v)
{
    PUSH_PREAMBLE
    SET_TARGET(V_BYTE_FLAG | V_BYTE_BASE, integer, v);
}

void loli_push_double(loli_state *s, double v)
{
    PUSH_PREAMBLE
    SET_TARGET(V_DOUBLE_FLAG | V_DOUBLE_BASE, doubleval, v);
}

void loli_push_empty_variant(loli_state *s, uint16_t id)
{
    PUSH_PREAMBLE
    SET_TARGET(V_EMPTY_VARIANT_BASE, integer, id);
}

void loli_push_file(loli_state *s, FILE *inner_file, const char *mode)
{
    PUSH_PREAMBLE
    loli_file_val *filev = loli_malloc(sizeof(*filev));

    int plus = strchr(mode, '+') != NULL;

    filev->refcount = 1;
    filev->inner_file = inner_file;
    filev->read_ok = (*mode == 'r' || plus);
    filev->write_ok = (*mode == 'w' || plus);
    filev->is_builtin = 0;

    SET_TARGET(V_FILE_BASE | VAL_IS_DEREFABLE, file, filev);
}

loli_foreign_val *loli_push_foreign(loli_state *s, uint16_t id,
        loli_destroy_func func, size_t size)
{
    PUSH_PREAMBLE
    loli_foreign_val *fv = loli_malloc(size * sizeof(*fv));
    fv->refcount = 1;
    fv->class_id = id;
    fv->destroy_func = func;

    SET_TARGET(VAL_IS_DEREFABLE | V_FOREIGN_BASE, foreign, fv);
    return fv;
}

loli_hash_val *loli_push_hash(loli_state *s, int size)
{
    PUSH_PREAMBLE
    loli_hash_val *h = loli_new_hash_raw(size);
    SET_TARGET(V_HASH_BASE | VAL_IS_DEREFABLE, hash, h);
    return h;
}

loli_container_val *loli_push_instance(loli_state *s, uint16_t id,
        uint32_t size)
{
    PUSH_CONTAINER(id, V_INSTANCE_BASE, size);
}

void loli_push_integer(loli_state *s, int64_t v)
{
    PUSH_PREAMBLE
    SET_TARGET(V_INTEGER_FLAG | V_INTEGER_BASE, integer, v);
}

loli_container_val *loli_push_list(loli_state *s, uint32_t size)
{
    PUSH_CONTAINER(LOLI_ID_LIST, V_LIST_BASE, size);
}

loli_container_val *loli_push_super(loli_state *s, uint16_t id,
        uint32_t initial)
{
    loli_value *v = s->call_chain->return_target;

    if (FLAGS_TO_BASE(v) == V_INSTANCE_BASE) {
        loli_container_val *pending_instance = v->value.container;
        if (pending_instance->instance_ctor_need != 0) {
            pending_instance->instance_ctor_need = 0;
            loli_push_value(s, v);
            return pending_instance;
        }
    }

    return loli_push_instance(s, id, initial);
}

void loli_push_string(loli_state *s, const char *source)
{
    PUSH_PREAMBLE
    size_t len = strlen(source);
    char *buffer = loli_malloc((len + 1) * sizeof(*buffer));
    strcpy(buffer, source);

    loli_string_val *sv = new_sv(buffer, len);

    SET_TARGET(V_STRING_FLAG | V_STRING_BASE | VAL_IS_DEREFABLE, string, sv);
}

void loli_push_string_sized(loli_state *s, const char *source, int len)
{
    PUSH_PREAMBLE
    char *buffer = loli_malloc((len + 1) * sizeof(*buffer));
    memcpy(buffer, source, len);
    buffer[len] = '\0';

    loli_string_val *sv = new_sv(buffer, len);

    SET_TARGET(V_STRING_FLAG | V_STRING_BASE | VAL_IS_DEREFABLE, string, sv);
}

loli_container_val *loli_push_tuple(loli_state *s, uint32_t size)
{
    PUSH_CONTAINER(LOLI_ID_TUPLE, V_TUPLE_BASE, size);
}

void loli_push_unit(loli_state *s)
{
    PUSH_PREAMBLE
    SET_TARGET(LOLI_ID_UNIT, integer, 0);
}

void loli_push_value(loli_state *s, loli_value *v)
{
    PUSH_PREAMBLE
    if (v->flags & VAL_IS_DEREFABLE)
        v->value.generic->refcount++;

    target->flags = v->flags;
    target->value = v->value;
}

loli_container_val *loli_push_variant(loli_state *s, uint16_t id, uint32_t size)
{
    PUSH_CONTAINER(id, V_VARIANT_BASE, size);
}

#define RETURN_PREAMBLE \
loli_value *target = s->call_chain->return_target; \
if (target->flags & VAL_IS_DEREFABLE) \
    loli_deref(target);

void loli_return_boolean(loli_state *s, int v)
{
    RETURN_PREAMBLE
    SET_TARGET(V_BOOLEAN_BASE, integer, v);
}

void loli_return_byte(loli_state *s, uint8_t v)
{
    RETURN_PREAMBLE
    SET_TARGET(V_BYTE_FLAG | V_BYTE_BASE, integer, v);
}

void loli_return_double(loli_state *s, double v)
{
    RETURN_PREAMBLE
    SET_TARGET(V_DOUBLE_FLAG | V_DOUBLE_BASE, doubleval, v);
}

void loli_return_integer(loli_state *s, int64_t v)
{
    RETURN_PREAMBLE
    SET_TARGET(V_INTEGER_FLAG | V_INTEGER_BASE, integer, v);
}

void loli_return_none(loli_state *s)
{
    RETURN_PREAMBLE
    SET_TARGET(V_EMPTY_VARIANT_BASE, integer, LOLI_ID_NONE);
}

void loli_return_super(loli_state *s)
{
    loli_value *target = s->call_chain->return_target;
    loli_value *top = *(s->call_chain->top - 1);

    if (FLAGS_TO_BASE(target) == V_INSTANCE_BASE &&
        target->value.container == top->value.container) {
        return;
    }

    if (target->flags & VAL_IS_DEREFABLE)
        loli_deref(target);

    *target = *top;
    top->flags = 0;
}

void loli_return_top(loli_state *s)
{
    loli_value *target = s->call_chain->return_target;
    if (target->flags & VAL_IS_DEREFABLE)
        loli_deref(target);

    loli_value *top = *(s->call_chain->top - 1);
    *target = *top;

    top->flags = 0;
}

void loli_return_unit(loli_state *s)
{
    RETURN_PREAMBLE
    SET_TARGET(LOLI_ID_UNIT, container, NULL);
}

void loli_return_value(loli_state *s, loli_value *v)
{
    loli_value *target = s->call_chain->return_target;
    loli_value_assign(target, v);
}


static void add_call_frame(loli_vm_state *vm)
{
    loli_call_frame *new_frame = loli_malloc(sizeof(*new_frame));

    new_frame->prev = vm->call_chain;
    new_frame->next = NULL;
    new_frame->return_target = NULL;
     
    new_frame->register_end = vm->call_chain->register_end;

    vm->call_chain->next = new_frame;
    vm->call_chain = new_frame;
}

static void add_catch_entry(loli_vm_state *vm)
{
    loli_vm_catch_entry *new_entry = loli_malloc(sizeof(*new_entry));

    vm->catch_chain->next = new_entry;
    new_entry->next = NULL;
    new_entry->prev = vm->catch_chain;
}


static const char *names[] = {
    "Exception",
    "IOError",
    "KeyError",
    "RuntimeError",
    "ValueError",
    "IndexError",
    "DivisionByZeroError"
};

static void dispatch_exception(loli_vm_state *vm);

static void vm_error(loli_vm_state *vm, uint8_t id, const char *message)
{
    loli_class *c = vm->gs->class_table[id];
    if (c == NULL) {
         
        c = loli_dynaload_exception(vm->gs->parser,
                names[id - LOLI_ID_EXCEPTION]);

         
        vm->gs->readonly_table = vm->gs->parser->symtab->literals->data;
        vm->gs->class_table[id] = c;
    }

    vm->exception_cls = c;
    loli_msgbuf *msgbuf = loli_mb_flush(vm->raiser->msgbuf);
    loli_mb_add(msgbuf, message);

    dispatch_exception(vm);
}

#define LOLI_ERROR(err, id) \
void loli_##err##Error(loli_vm_state *vm, const char *fmt, ...) \
{ \
    loli_msgbuf *msgbuf = loli_mb_flush(vm->raiser->aux_msgbuf); \
 \
    va_list var_args; \
    va_start(var_args, fmt); \
    loli_mb_add_fmt_va(msgbuf, fmt, var_args); \
    va_end(var_args); \
 \
    vm_error(vm, id, loli_mb_raw(msgbuf)); \
}

LOLI_ERROR(DivisionByZero, LOLI_ID_DBZERROR)
LOLI_ERROR(Index,          LOLI_ID_INDEXERROR)
LOLI_ERROR(IO,             LOLI_ID_IOERROR)
LOLI_ERROR(Key,            LOLI_ID_KEYERROR)
LOLI_ERROR(Runtime,        LOLI_ID_RUNTIMEERROR)
LOLI_ERROR(Value,          LOLI_ID_VALUEERROR)

static void key_error(loli_vm_state *vm, loli_value *key, uint16_t line_num)
{
    loli_msgbuf *msgbuf = loli_mb_flush(vm->raiser->aux_msgbuf);

    if (key->flags & V_STRING_FLAG)
        loli_mb_escape_add_str(msgbuf, key->value.string->string);
    else
        loli_mb_add_fmt(msgbuf, "%ld", key->value.integer);

    vm_error(vm, LOLI_ID_KEYERROR, loli_mb_raw(msgbuf));
}

static void boundary_error(loli_vm_state *vm, int64_t bad_index,
        uint16_t line_num)
{
    loli_msgbuf *msgbuf = loli_mb_flush(vm->raiser->aux_msgbuf);
    loli_mb_add_fmt(msgbuf, "Subscript index %ld is out of range.",
            bad_index);

    vm_error(vm, LOLI_ID_INDEXERROR, loli_mb_raw(msgbuf));
}


static loli_container_val *build_traceback_raw(loli_vm_state *);

void loli_builtin__calltrace(loli_vm_state *vm)
{
     
    vm->call_depth--;
    vm->call_chain = vm->call_chain->prev;

    loli_container_val *trace = build_traceback_raw(vm);

    vm->call_depth++;
    vm->call_chain = vm->call_chain->next;

    move_list_f(0, vm->call_chain->return_target, trace);
}

static void do_print(loli_vm_state *vm, FILE *target, loli_value *source,int nl)
{
    if (source->flags & V_STRING_FLAG)
        fputs(source->value.string->string, target);
    else {
        loli_msgbuf *msgbuf = loli_mb_flush(vm->vm_buffer);
        loli_mb_add_value(msgbuf, vm, source);
        fputs(loli_mb_raw(msgbuf), target);
    }

    if(nl){
      fputc('\n', target);
    }
    loli_return_unit(vm);
}

void loli_builtin__say(loli_vm_state *vm)
{
    loli_container_val *list_val = loli_arg_container(vm, 0);
    
    int i;
    for (i = 0;i < list_val->num_values; i++) {
        do_print(vm, stdout, loli_con_get(list_val, i), 0);
    }
}

void loli_builtin__sayln(loli_vm_state *vm)
{
    loli_container_val *list_val = loli_arg_container(vm, 0);
    
    int i;
    for (i = 0;i < list_val->num_values; i++) {
        do_print(vm, stdout, loli_con_get(list_val, i), 1);
    }
}

void loli_stdout_print(loli_vm_state *vm)
{
     
    uint16_t spot = *vm->call_chain->function->cid_table;
    loli_file_val *stdout_val = vm->gs->regs_from_main[spot]->value.file;
    if (stdout_val->inner_file == NULL)
        vm_error(vm, LOLI_ID_VALUEERROR, "IO operation on closed file.");

    do_print(vm, stdout_val->inner_file, loli_arg_value(vm, 0), 1);
}



static void do_o_property_set(loli_vm_state *vm, uint16_t *code)
{
    loli_value **vm_regs = vm->call_chain->start;
    loli_value *rhs_reg;
    int index;
    loli_container_val *ival;

    index = code[1];
    ival = vm_regs[code[2]]->value.container;
    rhs_reg = vm_regs[code[3]];

    loli_value_assign(ival->values[index], rhs_reg);
}

static void do_o_property_get(loli_vm_state *vm, uint16_t *code)
{
    loli_value **vm_regs = vm->call_chain->start;
    loli_value *result_reg;
    int index;
    loli_container_val *ival;

    index = code[1];
    ival = vm_regs[code[2]]->value.container;
    result_reg = vm_regs[code[3]];

    loli_value_assign(result_reg, ival->values[index]);
}

#define RELATIVE_INDEX(limit) \
    if (index_int < 0) { \
        int64_t new_index = limit + index_int; \
        if (new_index < 0) \
            boundary_error(vm, index_int, code[4]); \
 \
        index_int = new_index; \
    } \
    else if (index_int >= limit) \
        boundary_error(vm, index_int, code[4]);

static void do_o_subscript_set(loli_vm_state *vm, uint16_t *code)
{
    loli_value **vm_regs = vm->call_chain->start;
    loli_value *lhs_reg, *index_reg, *rhs_reg;
    uint16_t base;

    lhs_reg = vm_regs[code[1]];
    index_reg = vm_regs[code[2]];
    rhs_reg = vm_regs[code[3]];
    base = FLAGS_TO_BASE(lhs_reg);

    if (base != V_HASH_BASE) {
        int64_t index_int = index_reg->value.integer;

        if (base == V_BYTESTRING_BASE) {
            loli_string_val *bytev = lhs_reg->value.string;
            RELATIVE_INDEX(bytev->size)
            bytev->string[index_int] = (char)rhs_reg->value.integer;
        }
        else {
             
            loli_container_val *list_val = lhs_reg->value.container;
            RELATIVE_INDEX(list_val->num_values)
            loli_value_assign(list_val->values[index_int], rhs_reg);
        }
    }
    else
        loli_hash_set(vm, lhs_reg->value.hash, index_reg, rhs_reg);
}

static void do_o_subscript_get(loli_vm_state *vm, uint16_t *code)
{
    loli_value **vm_regs = vm->call_chain->start;
    loli_value *lhs_reg, *index_reg, *result_reg;
    uint16_t base;

    lhs_reg = vm_regs[code[1]];
    index_reg = vm_regs[code[2]];
    result_reg = vm_regs[code[3]];
    base = FLAGS_TO_BASE(lhs_reg);

    if (base != V_HASH_BASE) {
        int64_t index_int = index_reg->value.integer;

        if (base == V_BYTESTRING_BASE) {
            loli_string_val *bytev = lhs_reg->value.string;
            RELATIVE_INDEX(bytev->size)
            move_byte(result_reg, (uint8_t) bytev->string[index_int]);
        }
        else {
             
            loli_container_val *list_val = lhs_reg->value.container;
            RELATIVE_INDEX(list_val->num_values)
            loli_value_assign(result_reg, list_val->values[index_int]);
        }
    }
    else {
        loli_value *elem = loli_hash_get(vm, lhs_reg->value.hash, index_reg);

         
        if (elem == NULL)
            key_error(vm, index_reg, code[4]);

        loli_value_assign(result_reg, elem);
    }
}

#undef RELATIVE_INDEX

static void do_o_build_hash(loli_vm_state *vm, uint16_t *code)
{
    loli_value **vm_regs = vm->call_chain->start;
    int i, num_values;
    loli_value *result, *key_reg, *value_reg;

    num_values = code[2];
    result = vm_regs[code[3 + num_values]];

    loli_hash_val *hash_val = loli_new_hash_raw(num_values / 2);

    for (i = 0;
         i < num_values;
         i += 2) {
        key_reg = vm_regs[code[3 + i]];
        value_reg = vm_regs[code[3 + i + 1]];

        loli_hash_set(vm, hash_val, key_reg, value_reg);
    }

    move_hash_f(VAL_IS_GC_SPECULATIVE, result, hash_val);
}

static void do_o_build_list_tuple(loli_vm_state *vm, uint16_t *code)
{
    loli_value **vm_regs = vm->call_chain->start;
    int num_elems = code[1];
    loli_value *result = vm_regs[code[2+num_elems]];
    loli_container_val *lv;

    if (code[0] == o_build_list) {
        lv = new_container(LOLI_ID_LIST, num_elems);
    }
    else {
        lv = (loli_container_val *)new_container(LOLI_ID_TUPLE, num_elems);
    }

    loli_value **elems = lv->values;

    int i;
    for (i = 0;i < num_elems;i++) {
        loli_value *rhs_reg = vm_regs[code[2+i]];
        loli_value_assign(elems[i], rhs_reg);
    }

    if (code[0] == o_build_list)
        move_list_f(VAL_IS_GC_SPECULATIVE, result, lv);
    else
        move_tuple_f(VAL_IS_GC_SPECULATIVE, result, (loli_container_val *)lv);
}

static void do_o_build_variant(loli_vm_state *vm, uint16_t *code)
{
    loli_value **vm_regs = vm->call_chain->start;
    int variant_id = code[1];
    int count = code[2];
    loli_value *result = vm_regs[code[code[2] + 3]];

    loli_container_val *ival = new_container(variant_id, count);
    loli_value **slots = ival->values;

    int i;
    for (i = 0;i < count;i++) {
        loli_value *rhs_reg = vm_regs[code[3+i]];
        loli_value_assign(slots[i], rhs_reg);
    }

    move_variant_f(VAL_IS_GC_SPECULATIVE, result, ival);
}

static void do_o_exception_raise(loli_vm_state *vm, loli_value *exception_val)
{
     

    loli_container_val *ival = exception_val->value.container;
    char *message = ival->values[0]->value.string->string;
    loli_class *raise_cls = vm->gs->class_table[ival->class_id];

     
    vm->exception_value = exception_val;
    vm->exception_cls = raise_cls;

    loli_msgbuf *msgbuf = loli_mb_flush(vm->raiser->msgbuf);
    loli_mb_add(msgbuf, message);

    dispatch_exception(vm);
}

static void do_o_new_instance(loli_vm_state *vm, uint16_t *code)
{
    int total_entries;
    int cls_id = code[1];
    loli_value **vm_regs = vm->call_chain->start;
    loli_value *result = vm_regs[code[2]];
    loli_class *instance_class = vm->gs->class_table[cls_id];

    total_entries = instance_class->prop_count;

     
    loli_value *pending_value = vm->call_chain->return_target;
    if (FLAGS_TO_BASE(pending_value) == V_INSTANCE_BASE) {
        loli_container_val *cv = pending_value->value.container;

        if (cv->instance_ctor_need) {
            cv->instance_ctor_need--;
            loli_value_assign(result, pending_value);
            return;
        }
    }

    uint32_t flags =
        (instance_class->flags & CLS_GC_FLAGS) << VAL_FROM_CLS_GC_SHIFT;

    loli_container_val *iv = new_container(cls_id, total_entries);
    iv->instance_ctor_need = instance_class->inherit_depth;

    if (flags == VAL_IS_GC_SPECULATIVE)
        move_instance_f(VAL_IS_GC_SPECULATIVE, result, iv);
    else {
        move_instance_f(0, result, iv);
        if (flags == VAL_IS_GC_TAGGED)
            loli_value_tag(vm, result);
    }
}

static void do_o_interpolation(loli_vm_state *vm, uint16_t *code)
{
    loli_value **vm_regs = vm->call_chain->start;
    int count = code[1];
    loli_msgbuf *vm_buffer = loli_mb_flush(vm->vm_buffer);

    int i;
    for (i = 0;i < count;i++) {
        loli_value *v = vm_regs[code[2 + i]];
        loli_mb_add_value(vm_buffer, vm, v);
    }

    loli_value *result_reg = vm_regs[code[2 + i]];

    loli_string_val *sv = loli_new_string_raw(loli_mb_raw(vm_buffer));
    move_string(result_reg, sv);
}



static loli_value *make_cell_from(loli_value *value)
{
    loli_value *result = loli_malloc(sizeof(*result));
    *result = *value;
    result->cell_refcount = 1;
    if (value->flags & VAL_IS_DEREFABLE)
        value->value.generic->refcount++;

    return result;
}

static loli_function_val *new_function_copy(loli_function_val *to_copy)
{
    loli_function_val *f = loli_malloc(sizeof(*f));

    *f = *to_copy;
    f->refcount = 1;

    return f;
}

static loli_value **do_o_closure_new(loli_vm_state *vm, uint16_t *code)
{
    int count = code[1];
    loli_value *result = vm->call_chain->start[code[2]];

    loli_function_val *last_call = vm->call_chain->function;

    loli_function_val *closure_func = new_function_copy(last_call);

    loli_value **upvalues = loli_malloc(sizeof(*upvalues) * count);

     
    int i;
    for (i = 0;i < count;i++)
        upvalues[i] = NULL;

    closure_func->num_upvalues = count;
    closure_func->upvalues = upvalues;

     
    move_function_f(0, result, closure_func);
    loli_value_tag(vm, result);

     
    vm->call_chain->function = closure_func;

    return upvalues;
}

static void copy_upvalues(loli_function_val *target, loli_function_val *source)
{
    loli_value **source_upvalues = source->upvalues;
    int count = source->num_upvalues;

    loli_value **new_upvalues = loli_malloc(sizeof(*new_upvalues) * count);
    loli_value *up;
    int i;

    for (i = 0;i < count;i++) {
        up = source_upvalues[i];
        if (up)
            up->cell_refcount++;

        new_upvalues[i] = up;
    }

    target->upvalues = new_upvalues;
    target->num_upvalues = count;
}

static void do_o_closure_function(loli_vm_state *vm, uint16_t *code)
{
    loli_value **vm_regs = vm->call_chain->start;
    loli_function_val *input_closure = vm->call_chain->function;

    loli_value *target = vm->gs->readonly_table[code[1]];
    loli_function_val *target_func = target->value.function;

    loli_value *result_reg = vm_regs[code[2]];
    loli_function_val *new_closure = new_function_copy(target_func);

    copy_upvalues(new_closure, input_closure);

    uint16_t *locals = new_closure->proto->locals;
    if (locals) {
        loli_value **upvalues = new_closure->upvalues;
        int i, end = locals[0];
        for (i = 1;i < end;i++) {
            int pos = locals[i];
            loli_value *up = upvalues[pos];
            if (up) {
                up->cell_refcount--;
                upvalues[pos] = NULL;
            }
        }
    }

    move_function_f(VAL_IS_GC_SPECULATIVE, result_reg, new_closure);
    loli_value_tag(vm, result_reg);
}



static loli_container_val *build_traceback_raw(loli_vm_state *vm)
{
    loli_call_frame *frame_iter = vm->call_chain;
    int depth = vm->call_depth;
    int i;

    loli_msgbuf *msgbuf = loli_msgbuf_get(vm);
    loli_container_val *lv = new_container(LOLI_ID_LIST, depth);

     
    for (i = depth;
         i >= 1;
         i--, frame_iter = frame_iter->prev) {
        loli_function_val *func_val = frame_iter->function;
        loli_proto *proto = func_val->proto;
        const char *path = proto->module_path;
        char line[16] = "";
        if (func_val->code)
            sprintf(line, "%d:", frame_iter->code[-1]);

        const char *str = loli_mb_sprintf(msgbuf, "%s:%s from %s", path,
                line, proto->name);

        loli_string_val *sv = loli_new_string_raw(str);
        move_string(lv->values[i - 1], sv);
    }

    return lv;
}

static void make_proper_exception_val(loli_vm_state *vm,
        loli_class *raised_cls, loli_value *result)
{
    const char *raw_message = loli_mb_raw(vm->raiser->msgbuf);
    loli_container_val *ival = new_container(raised_cls->id, 2);

    loli_string_val *sv = loli_new_string_raw(raw_message);
    move_string(ival->values[0], sv);

    move_list_f(0, ival->values[1], build_traceback_raw(vm));

    move_instance_f(VAL_IS_GC_SPECULATIVE, result, ival);
}

static void fixup_exception_val(loli_vm_state *vm, loli_value *result)
{
    loli_value_assign(result, vm->exception_value);
    loli_container_val *raw_trace = build_traceback_raw(vm);
    loli_container_val *iv = result->value.container;

    move_list_f(VAL_IS_GC_SPECULATIVE, loli_con_get(iv, 1), raw_trace);
}

static void dispatch_exception(loli_vm_state *vm)
{
    loli_raiser *raiser = vm->raiser;
    loli_class *raised_cls = vm->exception_cls;
    loli_vm_catch_entry *catch_iter = vm->catch_chain->prev;
    int match = 0;
    int jump_location;
    uint16_t *code;

    vm->exception_cls = raised_cls;

    while (catch_iter != NULL) {
         
        if (catch_iter->catch_kind == catch_callback) {
            vm->call_chain = catch_iter->call_frame;
            vm->call_depth = catch_iter->call_frame_depth;
            catch_iter->callback_func(vm);
            catch_iter = catch_iter->prev;
            continue;
        }

        loli_call_frame *call_frame = catch_iter->call_frame;
        code = call_frame->function->code;
         
        jump_location = catch_iter->code_pos + code[catch_iter->code_pos] - 1;

        while (1) {
            loli_class *catch_class =
                    vm->gs->class_table[code[jump_location + 1]];

            if (loli_class_greater_eq(catch_class, raised_cls)) {
                 
                jump_location += 4;
                match = 1;
                break;
            }
            else {
                int move_by = code[jump_location + 2];
                if (move_by == 0)
                    break;

                jump_location += move_by;
            }
        }

        if (match)
            break;

        catch_iter = catch_iter->prev;
    }

    loli_jump_link *jump_stop;

    if (match) {
        code += jump_location;
        if (*code == o_exception_store) {
            loli_value *catch_reg = catch_iter->call_frame->start[code[1]];

             
            if (vm->exception_value)
                fixup_exception_val(vm, catch_reg);
            else
                make_proper_exception_val(vm, raised_cls, catch_reg);

            code += 2;
        }

         
        vm->exception_value = NULL;
        vm->exception_cls = NULL;
        vm->call_chain = catch_iter->call_frame;
        vm->call_depth = catch_iter->call_frame_depth;
        vm->call_chain->code = code;
         
        vm->catch_chain = catch_iter;

        jump_stop = catch_iter->jump_entry->prev;
    }
    else
         
        jump_stop = NULL;

    while (raiser->all_jumps->prev != jump_stop)
        raiser->all_jumps = raiser->all_jumps->prev;

    longjmp(raiser->all_jumps->jump, 1);
}



static loli_coroutine_val *new_coroutine(loli_vm_state *base_vm,
        loli_function_val *base_function)
{
    loli_coroutine_val *result = loli_malloc(sizeof(*result));
    loli_value *receiver = loli_malloc(sizeof(*receiver));

     
    receiver->flags = V_UNIT_BASE;

    result->refcount = 1;
    result->class_id = LOLI_ID_COROUTINE;
    result->status = co_waiting;
    result->vm = base_vm;
    result->base_function = base_function;
    result->receiver = receiver;

    return result;
}

static void coroutine_call_prep(loli_vm_state *vm, int count)
{
    loli_call_frame *source_frame = vm->call_chain;
    loli_call_frame *target_frame = vm->call_chain->next;

     
    target_frame->top = source_frame->top;
    source_frame->top -= count;
    target_frame->start = source_frame->top;

    vm->call_depth++;

    target_frame->top += target_frame->function->reg_count - count;
    vm->call_chain = target_frame;
}

static loli_state *coroutine_build(loli_state *s)
{
    loli_function_val *to_copy = loli_arg_function(s, 0);

    if (to_copy->foreign_func != NULL)
        loli_RuntimeError(s, "Only native functions can be coroutines.");

    loli_function_val *base_func = new_function_copy(to_copy);

    if (to_copy->upvalues)
        copy_upvalues(base_func, to_copy);
    else
        base_func->upvalues = NULL;

    loli_vm_state *base_vm = new_vm_state(loli_new_raiser(),
            INITIAL_REGISTER_COUNT + to_copy->reg_count);
    loli_call_frame *toplevel_frame = base_vm->call_chain;

    base_vm->gs = s->gs;
    base_vm->depth_max = s->depth_max;
    base_vm->data = s->data;
     
    toplevel_frame->code = foreign_code;
     
    toplevel_frame->function = base_func;

     

    loli_coroutine_val *co_val = new_coroutine(base_vm, base_func);

    push_coroutine(s, co_val);
     
    loli_value_tag(s, loli_stack_get_top(s));

     
    loli_call_prepare(base_vm, base_func);
    loli_push_value(base_vm, loli_stack_get_top(s));

    return base_vm;
}

void loli_builtin_Coroutine_build(loli_state *s)
{
    loli_state *base_vm = coroutine_build(s);

    coroutine_call_prep(base_vm, 1);
    loli_return_top(s);
}

void loli_builtin_Coroutine_build_with_value(loli_state *s)
{
    loli_state *base_vm = coroutine_build(s);

    loli_push_value(base_vm, loli_arg_value(s, 1));

    coroutine_call_prep(base_vm, 2);
    loli_return_top(s);
}

void loli_builtin_Coroutine_receive(loli_state *s)
{
    loli_coroutine_val *co_val = loli_arg_coroutine(s, 0);

    if (co_val->vm != s)
        loli_RuntimeError(s,
                "Attempt to receive a value from another coroutine.");

    loli_push_value(s, co_val->receiver);
    loli_return_top(s);
}

static void coroutine_resume(loli_vm_state *origin, loli_coroutine_val *co_val,
        loli_value *to_send)
{
     
    if (co_val->status != co_waiting) {
        loli_push_none(origin);
        return;
    }

    loli_vm_state *target = co_val->vm;
    loli_coroutine_status new_status = co_running;

    if (to_send)
        loli_value_assign(co_val->receiver, to_send);

    co_val->status = co_running;

     
    loli_jump_link *jump_base = target->raiser->all_jumps;

    loli_value *result = NULL;

    if (setjmp(jump_base->jump) == 0) {
         
        loli_vm_execute(target);

        new_status = co_done;
    }
    else if (target->exception_cls == NULL) {
         
        result = loli_stack_get_top(target);

        new_status = co_waiting;

         
        target->call_chain = target->call_chain->prev;
        target->call_depth--;
    }
    else
         
        new_status = co_failed;

    co_val->status = new_status;

    if (result) {
        loli_container_val *con = loli_push_some(origin);
        loli_push_value(origin, result);
        loli_con_set_from_stack(origin, con, 0);
    }
    else
        loli_push_none(origin);
}

void loli_builtin_Coroutine_resume(loli_state *s)
{
    coroutine_resume(s, loli_arg_coroutine(s, 0), NULL);
    loli_return_top(s);
}

void loli_builtin_Coroutine_resume_with(loli_state *s)
{
    coroutine_resume(s, loli_arg_coroutine(s, 0), loli_arg_value(s, 1));
    loli_return_top(s);
}

void loli_builtin_Coroutine_yield(loli_state *s)
{
    loli_coroutine_val *co_target = loli_arg_coroutine(s, 0);
    loli_value *to_yield = loli_arg_value(s, 1);

    loli_vm_state *co_vm = co_target->vm;

    if (co_vm != s)
        loli_RuntimeError(s, "Cannot yield from another coroutine.");

    loli_raiser *co_raiser = co_vm->raiser;

     
    if (co_raiser->all_jumps->prev->prev != NULL)
        loli_RuntimeError(s, "Cannot yield while in a foreign call.");

     
    loli_return_unit(s);

     
    loli_push_value(co_vm, to_yield);

     
    loli_release_jump(co_raiser);

    longjmp(co_raiser->all_jumps->jump, 1);
}


loli_msgbuf *loli_msgbuf_get(loli_vm_state *vm)
{
    return loli_mb_flush(vm->vm_buffer);
}


void loli_call_prepare(loli_vm_state *vm, loli_function_val *func)
{
    loli_call_frame *caller_frame = vm->call_chain;
    caller_frame->code = foreign_code;

    if (caller_frame->next == NULL) {
        add_call_frame(vm);
         
        vm->call_chain = caller_frame;
    }

    loli_call_frame *target_frame = caller_frame->next;
    target_frame->code = func->code;
    target_frame->function = func;
    target_frame->return_target = *caller_frame->top;

    loli_push_unit(vm);
}

loli_value *loli_call_result(loli_vm_state *vm)
{
    return vm->call_chain->next->return_target;
}

void loli_call(loli_vm_state *vm, int count)
{
    loli_call_frame *source_frame = vm->call_chain;
    loli_call_frame *target_frame = vm->call_chain->next;
    loli_function_val *target_fn = target_frame->function;

     
    target_frame->top = source_frame->top;
    source_frame->top -= count;
    target_frame->start = source_frame->top;

    vm->call_depth++;

    if (target_fn->code == NULL) {
        vm->call_chain = target_frame;
        target_fn->foreign_func(vm);

        vm->call_chain = target_frame->prev;
        vm->call_depth--;
    }
    else {
         
        target_frame->code = target_fn->code;

        int diff = target_frame->function->reg_count - count;

        if (target_frame->top + diff > target_frame->register_end) {
            vm->call_chain = target_frame;
            grow_vm_registers(vm, diff);
        }

        loli_value **start = target_frame->top;
        loli_value **end = target_frame->top + diff;
        while (start != end) {
            loli_value *v = *start;
            loli_deref(v);
            v->flags = 0;
            start++;
        }

        target_frame->top += diff;
        vm->call_chain = target_frame;

        loli_vm_execute(vm);

         
    }
}

void loli_error_callback_push(loli_state *s, loli_error_callback_func func)
{
    if (s->catch_chain->next == NULL)
        add_catch_entry(s);

    loli_vm_catch_entry *catch_entry = s->catch_chain;
    catch_entry->call_frame = s->call_chain;
    catch_entry->call_frame_depth = s->call_depth;
    catch_entry->callback_func = func;
    catch_entry->catch_kind = catch_callback;

    s->catch_chain = s->catch_chain->next;
}

void loli_error_callback_pop(loli_state *s)
{
    s->catch_chain = s->catch_chain->prev;
}



void loli_vm_ensure_class_table(loli_vm_state *vm, int size)
{
    int old_count = vm->gs->class_count;

    if (size >= vm->gs->class_count) {
        if (vm->gs->class_count == 0)
            vm->gs->class_count = 1;

        while (size >= vm->gs->class_count)
            vm->gs->class_count *= 2;

        vm->gs->class_table = loli_realloc(vm->gs->class_table,
                sizeof(*vm->gs->class_table) * vm->gs->class_count);
    }

     
    if (old_count == 0) {
        int i;
        for (i = LOLI_ID_EXCEPTION;i < LOLI_ID_UNIT;i++)
            vm->gs->class_table[i] = NULL;
    }
}

void loli_vm_add_class_unchecked(loli_vm_state *vm, loli_class *cls)
{
    vm->gs->class_table[cls->id] = cls;
}


#define INTEGER_OP(OP) \
lhs_reg = vm_regs[code[1]]; \
rhs_reg = vm_regs[code[2]]; \
vm_regs[code[3]]->value.integer = \
lhs_reg->value.integer OP rhs_reg->value.integer; \
vm_regs[code[3]]->flags = V_INTEGER_FLAG | V_INTEGER_BASE; \
code += 5;

#define DOUBLE_OP(OP) \
lhs_reg = vm_regs[code[1]]; \
rhs_reg = vm_regs[code[2]]; \
vm_regs[code[3]]->value.doubleval = \
lhs_reg->value.doubleval OP rhs_reg->value.doubleval; \
vm_regs[code[3]]->flags = V_DOUBLE_FLAG | V_DOUBLE_BASE; \
code += 5;

#define EQUALITY_COMPARE_OP(OP) \
lhs_reg = vm_regs[code[1]]; \
rhs_reg = vm_regs[code[2]]; \
if (lhs_reg->flags & V_DOUBLE_FLAG) { \
    vm_regs[code[3]]->value.integer = \
    (lhs_reg->value.doubleval OP rhs_reg->value.doubleval); \
} \
else if (lhs_reg->flags & V_INTEGER_FLAG) { \
    vm_regs[code[3]]->value.integer =  \
    (lhs_reg->value.integer OP rhs_reg->value.integer); \
} \
else if (lhs_reg->flags & V_STRING_FLAG) { \
    vm_regs[code[3]]->value.integer = \
    strcmp(lhs_reg->value.string->string, \
           rhs_reg->value.string->string) OP 0; \
} \
else { \
    SAVE_LINE(+5); \
    vm_regs[code[3]]->value.integer = \
    loli_value_compare(vm, lhs_reg, rhs_reg) OP 1; \
} \
vm_regs[code[3]]->flags = V_BOOLEAN_BASE; \
code += 5;

#define COMPARE_OP(OP) \
lhs_reg = vm_regs[code[1]]; \
rhs_reg = vm_regs[code[2]]; \
if (lhs_reg->flags & V_DOUBLE_FLAG) { \
    vm_regs[code[3]]->value.integer = \
    (lhs_reg->value.doubleval OP rhs_reg->value.doubleval); \
} \
else if (lhs_reg->flags & (V_INTEGER_FLAG | V_BYTE_FLAG)) { \
    vm_regs[code[3]]->value.integer = \
    (lhs_reg->value.integer OP rhs_reg->value.integer); \
} \
else if (lhs_reg->flags & V_STRING_FLAG) { \
    vm_regs[code[3]]->value.integer = \
    strcmp(lhs_reg->value.string->string, \
           rhs_reg->value.string->string) OP 0; \
} \
vm_regs[code[3]]->flags = V_BOOLEAN_BASE; \
code += 5;

void loli_vm_execute(loli_vm_state *vm)
{
    uint16_t *code;
    loli_value **vm_regs;
    int i;
    register int64_t for_temp;
    register loli_value *lhs_reg, *rhs_reg, *loop_reg, *step_reg;
    loli_function_val *fval;
    loli_value **upvalues = NULL;

    loli_call_frame *current_frame = vm->call_chain;
    loli_call_frame *next_frame = NULL;

    code = current_frame->code;

    loli_jump_link *link = loli_jump_setup(vm->raiser);
    if (setjmp(link->jump) != 0) {
         
        current_frame = vm->call_chain;
        code = current_frame->code;
    }

    upvalues = current_frame->function->upvalues;
    vm_regs = vm->call_chain->start;

    while (1) {
        switch(code[0]) {
            case o_assign_noref:
                rhs_reg = vm_regs[code[1]];
                lhs_reg = vm_regs[code[2]];
                lhs_reg->flags = rhs_reg->flags;
                lhs_reg->value = rhs_reg->value;
                code += 4;
                break;
            case o_load_readonly:
                rhs_reg = vm->gs->readonly_table[code[1]];
                lhs_reg = vm_regs[code[2]];

                loli_deref(lhs_reg);

                lhs_reg->value = rhs_reg->value;
                lhs_reg->flags = rhs_reg->flags;
                code += 4;
                break;
            case o_load_empty_variant:
                lhs_reg = vm_regs[code[2]];

                loli_deref(lhs_reg);

                lhs_reg->value.integer = code[1];
                lhs_reg->flags = V_EMPTY_VARIANT_BASE;
                code += 4;
                break;
            case o_load_integer:
                lhs_reg = vm_regs[code[2]];
                lhs_reg->value.integer = (int16_t)code[1];
                lhs_reg->flags = V_INTEGER_FLAG | V_INTEGER_BASE;
                code += 4;
                break;
            case o_load_boolean:
                lhs_reg = vm_regs[code[2]];
                lhs_reg->value.integer = code[1];
                lhs_reg->flags = V_BOOLEAN_BASE;
                code += 4;
                break;
            case o_load_byte:
                lhs_reg = vm_regs[code[2]];
                lhs_reg->value.integer = (uint8_t)code[1];
                lhs_reg->flags = V_BYTE_FLAG | V_BYTE_BASE;
                code += 4;
                break;
            case o_int_add:
                INTEGER_OP(+)
                break;
            case o_int_minus:
                INTEGER_OP(-)
                break;
            case o_number_add:
                DOUBLE_OP(+)
                break;
            case o_number_minus:
                DOUBLE_OP(-)
                break;
            case o_compare_eq:
                EQUALITY_COMPARE_OP(==)
                break;
            case o_compare_greater:
                COMPARE_OP(>)
                break;
            case o_compare_greater_eq:
                COMPARE_OP(>=)
                break;
            case o_compare_not_eq:
                EQUALITY_COMPARE_OP(!=)
                break;
            case o_jump:
                code += (int16_t)code[1];
                break;
            case o_int_multiply:
                INTEGER_OP(*)
                break;
            case o_number_multiply:
                DOUBLE_OP(*)
                break;
            case o_int_divide:
                 
                rhs_reg = vm_regs[code[2]];
                if (rhs_reg->value.integer == 0) {
                    SAVE_LINE(+5);
                    vm_error(vm, LOLI_ID_DBZERROR,
                            "Attempt to divide by zero.");
                }
                INTEGER_OP(/)
                break;
            case o_int_modulo:
                 
                rhs_reg = vm_regs[code[2]];
                if (rhs_reg->value.integer == 0) {
                    SAVE_LINE(+5);
                    vm_error(vm, LOLI_ID_DBZERROR,
                            "Attempt to divide by zero.");
                }

                INTEGER_OP(%)
                break;
            case o_int_left_shift:
                INTEGER_OP(<<)
                break;
            case o_int_right_shift:
                INTEGER_OP(>>)
                break;
            case o_int_bitwise_and:
                INTEGER_OP(&)
                break;
            case o_int_bitwise_or:
                INTEGER_OP(|)
                break;
            case o_int_bitwise_xor:
                INTEGER_OP(^)
                break;
            case o_number_divide:
                rhs_reg = vm_regs[code[2]];
                if (rhs_reg->value.doubleval == 0) {
                    SAVE_LINE(+5);
                    vm_error(vm, LOLI_ID_DBZERROR,
                            "Attempt to divide by zero.");
                }

                DOUBLE_OP(/)
                break;
            case o_jump_if:
                lhs_reg = vm_regs[code[2]];
                {
                    int base = FLAGS_TO_BASE(lhs_reg);
                    int result;

                    if (base == V_INTEGER_BASE || base == V_BOOLEAN_BASE)
                        result = (lhs_reg->value.integer == 0);
                    else if (base == V_STRING_BASE)
                        result = (lhs_reg->value.string->size == 0);
                    else if (base == V_LIST_BASE)
                        result = (lhs_reg->value.container->num_values == 0);
                    else
                        result = 1;

                    if (result != code[1])
                        code += (int16_t)code[3];
                    else
                        code += 4;
                }
                break;
            case o_call_foreign:
                fval = vm->gs->readonly_table[code[1]]->value.function;

                foreign_func_body: ;

                vm_setup_before_call(vm, code);

                i = code[2];

                next_frame = current_frame->next;
                next_frame->function = fval;
                next_frame->top = next_frame->start + i;

                if (next_frame->top >= next_frame->register_end) {
                    vm->call_chain = next_frame;
                    grow_vm_registers(vm, i + 1);
                }

                prep_registers(current_frame, code);

                vm_regs = next_frame->start;
                vm->call_chain = next_frame;
                vm->call_depth++;

                fval->foreign_func(vm);

                vm->call_depth--;
                vm->call_chain = current_frame;
                vm_regs = current_frame->start;
                code = current_frame->code;

                break;
            case o_call_native: {
                fval = vm->gs->readonly_table[code[1]]->value.function;

                native_func_body: ;

                vm_setup_before_call(vm, code);

                next_frame = current_frame->next;
                next_frame->function = fval;
                next_frame->top = next_frame->start + fval->reg_count;

                if (next_frame->top >= next_frame->register_end) {
                    vm->call_chain = next_frame;
                    grow_vm_registers(vm, fval->reg_count);
                }

                prep_registers(current_frame, code);
                 
                clear_extra_registers(next_frame, code);

                current_frame = current_frame->next;
                vm->call_chain = current_frame;
                vm->call_depth++;

                vm_regs = current_frame->start;
                code = fval->code;
                upvalues = fval->upvalues;

                break;
            }
            case o_call_register:
                fval = vm_regs[code[1]]->value.function;

                if (fval->code != NULL)
                    goto native_func_body;
                else
                    goto foreign_func_body;

                break;
            case o_interpolation:
                do_o_interpolation(vm, code);
                code += code[1] + 4;
                break;
            case o_unary_not:
                lhs_reg = vm_regs[code[1]];

                rhs_reg = vm_regs[code[2]];
                rhs_reg->flags = lhs_reg->flags;
                rhs_reg->value.integer = !(lhs_reg->value.integer);
                code += 4;
                break;
            case o_unary_minus:
                lhs_reg = vm_regs[code[1]];

                rhs_reg = vm_regs[code[2]];

                if (lhs_reg->flags & V_INTEGER_FLAG) {
                    rhs_reg->value.integer = -(lhs_reg->value.integer);
                    rhs_reg->flags = V_INTEGER_FLAG | V_INTEGER_BASE;
                }
                else {
                    rhs_reg->value.doubleval = -(lhs_reg->value.doubleval);
                    rhs_reg->flags = V_DOUBLE_FLAG | V_DOUBLE_BASE;
                }

                code += 4;
                break;
            case o_unary_bitwise_not:
                lhs_reg = vm_regs[code[1]];

                rhs_reg = vm_regs[code[2]];
                rhs_reg->flags = lhs_reg->flags;
                rhs_reg->value.integer = ~(lhs_reg->value.integer);
                code += 4;
                break;
            case o_return_unit:
                move_unit(current_frame->return_target);
                goto return_common;

            case o_return_value:
                lhs_reg = current_frame->return_target;
                rhs_reg = vm_regs[code[1]];
                loli_value_assign(lhs_reg, rhs_reg);

                return_common: ;

                current_frame = current_frame->prev;
                vm->call_chain = current_frame;
                vm->call_depth--;

                vm_regs = current_frame->start;
                upvalues = current_frame->function->upvalues;
                code = current_frame->code;
                break;
            case o_global_get:
                rhs_reg = vm->gs->regs_from_main[code[1]];
                lhs_reg = vm_regs[code[2]];

                loli_value_assign(lhs_reg, rhs_reg);
                code += 4;
                break;
            case o_global_set:
                rhs_reg = vm_regs[code[1]];
                lhs_reg = vm->gs->regs_from_main[code[2]];

                loli_value_assign(lhs_reg, rhs_reg);
                code += 4;
                break;
            case o_assign:
                rhs_reg = vm_regs[code[1]];
                lhs_reg = vm_regs[code[2]];

                loli_value_assign(lhs_reg, rhs_reg);
                code += 4;
                break;
            case o_subscript_get:
                 
                SAVE_LINE(+5);
                do_o_subscript_get(vm, code);
                code += 5;
                break;
            case o_property_get:
                do_o_property_get(vm, code);
                code += 5;
                break;
            case o_subscript_set:
                 
                SAVE_LINE(+5);
                do_o_subscript_set(vm, code);
                code += 5;
                break;
            case o_property_set:
                do_o_property_set(vm, code);
                code += 5;
                break;
            case o_build_hash:
                do_o_build_hash(vm, code);
                code += code[2] + 5;
                break;
            case o_build_list:
            case o_build_tuple:
                do_o_build_list_tuple(vm, code);
                code += code[1] + 4;
                break;
            case o_build_variant:
                do_o_build_variant(vm, code);
                code += code[2] + 5;
                break;
            case o_closure_function:
                do_o_closure_function(vm, code);
                code += 4;
                break;
            case o_closure_set:
                lhs_reg = upvalues[code[1]];
                rhs_reg = vm_regs[code[2]];
                if (lhs_reg == NULL)
                    upvalues[code[1]] = make_cell_from(rhs_reg);
                else
                    loli_value_assign(lhs_reg, rhs_reg);

                code += 4;
                break;
            case o_closure_get:
                lhs_reg = vm_regs[code[2]];
                rhs_reg = upvalues[code[1]];
                loli_value_assign(lhs_reg, rhs_reg);
                code += 4;
                break;
            case o_for_integer:
                 
                loop_reg = vm_regs[code[1]];
                rhs_reg  = vm_regs[code[2]];
                step_reg = vm_regs[code[3]];

                 
                for_temp = loop_reg->value.integer + step_reg->value.integer;

                 
                if ((step_reg->value.integer > 0)
                         
                        ? (for_temp < rhs_reg->value.integer)
                         
                        : (for_temp >= rhs_reg->value.integer)) {

                     
                    lhs_reg = vm_regs[code[4]];
                    lhs_reg->value.integer = for_temp;
                    loop_reg->value.integer = for_temp;
                    code += 7;
                }
                else
                    code += code[5];

                break;
            case o_catch_push:
            {
                if (vm->catch_chain->next == NULL)
                    add_catch_entry(vm);

                loli_vm_catch_entry *catch_entry = vm->catch_chain;
                catch_entry->call_frame = current_frame;
                catch_entry->call_frame_depth = vm->call_depth;
                catch_entry->code_pos = 1 + (code - current_frame->function->code);
                catch_entry->jump_entry = vm->raiser->all_jumps;
                catch_entry->catch_kind = catch_native;

                vm->catch_chain = vm->catch_chain->next;
                code += 3;
                break;
            }
            case o_catch_pop:
                vm->catch_chain = vm->catch_chain->prev;

                code++;
                break;
            case o_exception_raise:
                SAVE_LINE(+3);
                lhs_reg = vm_regs[code[1]];
                do_o_exception_raise(vm, lhs_reg);
                code += 3;
                break;
            case o_instance_new:
            {
                do_o_new_instance(vm, code);
                code += 4;
                break;
            }
            case o_jump_if_not_class:
                lhs_reg = vm_regs[code[2]];
                i = FLAGS_TO_BASE(lhs_reg);

                 
                if (i == V_VARIANT_BASE || i == V_INSTANCE_BASE)
                    i = lhs_reg->value.container->class_id;
                else
                    i = (uint16_t)lhs_reg->value.integer;

                if (i == code[1])
                    code += 4;
                else
                    code += code[3];

                break;
            case o_jump_if_set:
                lhs_reg = vm_regs[code[1]];

                if (lhs_reg->flags == 0)
                    code += 3;
                else
                    code += code[2];

                break;
            case o_closure_new:
                do_o_closure_new(vm, code);
                upvalues = current_frame->function->upvalues;
                code += 4;
                break;
            case o_for_setup:
                 
                lhs_reg = vm_regs[code[1]];
                rhs_reg = vm_regs[code[2]];
                step_reg = vm_regs[code[3]];
                loop_reg = vm_regs[code[4]];

                if (step_reg->value.integer == 0)
                    vm_error(vm, LOLI_ID_VALUEERROR,
                               "for loop step cannot be 0.");

                 
                loop_reg->value.integer =
                        lhs_reg->value.integer - step_reg->value.integer;
                lhs_reg->value.integer = loop_reg->value.integer;
                loop_reg->flags = V_INTEGER_FLAG | V_INTEGER_BASE;

                code += 6;
                break;
            case o_vm_exit:
                loli_release_jump(vm->raiser);
                return;
            default:
                return;
        }
    }
}
