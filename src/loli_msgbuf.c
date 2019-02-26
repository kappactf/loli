#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <inttypes.h>

#include "loli.h"

#include "loli_core_types.h"
#include "loli_value_flags.h"
#include "loli_vm.h"
#include "loli_alloc.h"

extern loli_type *loli_unit_type;

typedef struct loli_msgbuf_ {
     
    char *message;
     
    uint32_t pos;
     
    uint32_t size;
} loli_msgbuf;

loli_msgbuf *loli_new_msgbuf(uint32_t initial)
{
    loli_msgbuf *msgbuf = loli_malloc(sizeof(*msgbuf));

    msgbuf->message = loli_malloc(initial * sizeof(*msgbuf->message));
    msgbuf->message[0] = '\0';
    msgbuf->pos = 0;
    msgbuf->size = initial;

    return msgbuf;
}

static void resize_msgbuf(loli_msgbuf *msgbuf, int new_size)
{
    while (msgbuf->size < new_size)
        msgbuf->size *= 2;

    msgbuf->message = loli_realloc(msgbuf->message,
            msgbuf->size * sizeof(*msgbuf->message));
}

static void add_escaped_char(loli_msgbuf *msgbuf, char ch)
{
    char buffer[16];
    sprintf(buffer, "%03d", (unsigned char)ch);

    loli_mb_add(msgbuf, buffer);
}

static char get_escape(char ch)
{
    if (ch == '\n')
        ch = 'n';
    else if (ch == '\r')
        ch = 'r';
    else if (ch == '\t')
        ch = 't';
    else if (ch == '\'')
        ch = '\'';
    else if (ch == '"')
        ch = '"';
    else if (ch == '\\')
        ch = '\\';
    else if (ch == '\b')
        ch = 'b';
    else if (ch == '\a')
        ch = 'a';
    else
        ch = 0;

    return ch;
}

static void add_escaped_raw(loli_msgbuf *msgbuf, int is_bytestring,
        const char *str, int len)
{
    char escape_char = 0;
    int i, start;

    for (i = 0, start = 0;i < len;i++) {
        char ch = str[i];
        int need_escape = 1;

        if (isprint(ch) ||
            ((unsigned char)ch > 127 && is_bytestring == 0)) {
            need_escape = 0;
            escape_char = 0;
        }
        else
            escape_char = get_escape(ch);

        if (need_escape) {
            if (i != start)
                loli_mb_add_slice(msgbuf, str, start, i);

            loli_mb_add_char(msgbuf, '\\');
            if (escape_char)
                loli_mb_add_char(msgbuf, escape_char);
            else
                add_escaped_char(msgbuf, ch);

            start = i + 1;
        }
    }

    if (i != start)
        loli_mb_add_slice(msgbuf, str, start, i);

     
    if (is_bytestring)
        loli_mb_add_char(msgbuf, '\0');
}

void loli_free_msgbuf(loli_msgbuf *msgbuf)
{
    loli_free(msgbuf->message);
    loli_free(msgbuf);
}

const char *loli_mb_raw(loli_msgbuf *msgbuf)
{
    return msgbuf->message;
}

void loli_mb_add(loli_msgbuf *msgbuf, const char *str)
{
    size_t len = strlen(str);

    if ((msgbuf->pos + len + 1) > msgbuf->size)
        resize_msgbuf(msgbuf, msgbuf->pos + len + 1);

    strcat(msgbuf->message, str);
    msgbuf->pos += len;
}

static void add_bytestring(loli_msgbuf *msgbuf, loli_string_val *sv)
{
    add_escaped_raw(msgbuf, 1, sv->string, sv->size);
}

void loli_mb_escape_add_str(loli_msgbuf *msgbuf, const char *str)
{
     
    loli_mb_add_char(msgbuf, '"');
    add_escaped_raw(msgbuf, 0, str, strlen(str));
    loli_mb_add_char(msgbuf, '"');
}

void loli_mb_add_slice(loli_msgbuf *msgbuf, const char *text,
        int start, int stop)
{
    int range = (stop - start);

    if ((msgbuf->pos + range + 1) > msgbuf->size)
        resize_msgbuf(msgbuf, msgbuf->pos + range + 1);

    memcpy(msgbuf->message + msgbuf->pos, text + start, range);
    msgbuf->pos += range;
    msgbuf->message[msgbuf->pos] = '\0';
}

void loli_mb_add_char(loli_msgbuf *msgbuf, char c)
{
    char ch_buf[2] = {c, '\0'};

    loli_mb_add(msgbuf, ch_buf);
}

static void add_boolean(loli_msgbuf *msgbuf, int b)
{
    if (b == 0)
        loli_mb_add(msgbuf, "false");
    else
        loli_mb_add(msgbuf, "true");
}

static void add_byte(loli_msgbuf *msgbuf, uint8_t i)
{
    char buf[64];
    char ch = (char)i;
    char esc_ch = get_escape(ch);

    if (esc_ch)
        sprintf(buf, "'\\%c'", esc_ch);
    else if (isprint(ch))
        sprintf(buf, "'%c'", ch);
    else
        sprintf(buf, "'\\%03d'", (unsigned char) ch);

    loli_mb_add(msgbuf, buf);
}

static void add_int(loli_msgbuf *msgbuf, int i)
{
    char buf[64];
    sprintf(buf, "%d", i);

    loli_mb_add(msgbuf, buf);
}

static void add_int64(loli_msgbuf *msgbuf, int64_t i)
{
    char buf[64];
    sprintf(buf, "%" PRId64, i);

    loli_mb_add(msgbuf, buf);
}

static void add_double(loli_msgbuf *msgbuf, double d)
{
    char buf[64];
    sprintf(buf, "%g", d);

    loli_mb_add(msgbuf, buf);
}

loli_msgbuf *loli_mb_flush(loli_msgbuf *msgbuf)
{
    msgbuf->pos = 0;
    msgbuf->message[0] = '\0';
    return msgbuf;
}

static void add_type(loli_msgbuf *msgbuf, loli_type *type)
{
    loli_mb_add(msgbuf, type->cls->name);

    if (type->cls->id == LOLI_ID_FUNCTION) {
        loli_mb_add(msgbuf, " (");

        if (type->subtype_count > 1) {
            int i;

            for (i = 1;i < type->subtype_count - 1;i++) {
                add_type(msgbuf, type->subtypes[i]);
                loli_mb_add(msgbuf, ", ");
            }

            if (type->flags & TYPE_IS_VARARGS) {
                loli_type *v_type = type->subtypes[i];

                 
                if (v_type->cls->id == LOLI_ID_OPTARG) {
                    loli_mb_add(msgbuf, "*");
                    add_type(msgbuf, v_type->subtypes[0]->subtypes[0]);
                }
                else
                    add_type(msgbuf, v_type->subtypes[0]);

                loli_mb_add(msgbuf, "...");
            }
            else
                add_type(msgbuf, type->subtypes[i]);
        }
        if (type->subtypes[0] == loli_unit_type)
            loli_mb_add(msgbuf, ")");
        else {
            loli_mb_add(msgbuf, " => ");
            add_type(msgbuf, type->subtypes[0]);
            loli_mb_add(msgbuf, ")");
        }
    }
    else if (type->cls->generic_count != 0) {
        int i;
        int is_optarg = type->cls->id == LOLI_ID_OPTARG;

        if (is_optarg == 0)
            loli_mb_add(msgbuf, "[");

        for (i = 0;i < type->subtype_count;i++) {
            add_type(msgbuf, type->subtypes[i]);
            if (i != (type->subtype_count - 1))
                loli_mb_add(msgbuf, ", ");
        }

        if (is_optarg == 0)
            loli_mb_add(msgbuf, "]");
    }
}

void loli_mb_add_fmt_va(loli_msgbuf *msgbuf, const char *fmt,
        va_list var_args)
{
    char buffer[128];
    int i, text_start;
    size_t len;

    text_start = 0;
    len = strlen(fmt);

    for (i = 0;i < len;i++) {
        char c = fmt[i];
        if (c == '%') {
            if (i + 1 == len)
                break;

            if (i != text_start)
                loli_mb_add_slice(msgbuf, fmt, text_start, i);

            i++;
            c = fmt[i];

            if (c == 's') {
                char *str = va_arg(var_args, char *);
                loli_mb_add(msgbuf, str);
            }
            else if (c == 'd') {
                int d = va_arg(var_args, int);
                add_int(msgbuf, d);
            }
            else if (c == 'c') {
                char ch = va_arg(var_args, int);
                loli_mb_add_char(msgbuf, ch);
            }
            else if (c == 'p') {
                void *p = va_arg(var_args, void *);
                snprintf(buffer, 128, "%p", p);
                loli_mb_add(msgbuf, buffer);
            }
            else if (c == 'l' && fmt[i + 1] == 'd') {
                i++;
                int64_t d = va_arg(var_args, int64_t);
                add_int64(msgbuf, d);
            }
            else if (c == '%')
                loli_mb_add_char(msgbuf, '%');

            text_start = i+1;
        }
         
        else if (c == '^') {
            if (i != text_start)
                loli_mb_add_slice(msgbuf, fmt, text_start, i);

            i++;
            c = fmt[i];
            if (c == 'T') {
                loli_type *type = va_arg(var_args, loli_type *);
                add_type(msgbuf, type);
            }

            text_start = i+1;
        }
    }

    if (i != text_start)
        loli_mb_add_slice(msgbuf, fmt, text_start, i);
}

void loli_mb_add_fmt(loli_msgbuf *msgbuf, const char *fmt, ...)
{
    va_list var_args;
    va_start(var_args, fmt);
    loli_mb_add_fmt_va(msgbuf, fmt, var_args);
    va_end(var_args);
}

int loli_mb_pos(loli_msgbuf *msgbuf)
{
    return msgbuf->pos;
}

const char *loli_mb_sprintf(loli_msgbuf *msgbuf, const char *fmt, ...)
{
    loli_mb_flush(msgbuf);

    va_list var_args;
    va_start(var_args, fmt);
    loli_mb_add_fmt_va(msgbuf, fmt, var_args);
    va_end(var_args);

    return msgbuf->message;
}

typedef struct tag_ {
    struct tag_ *prev;
    loli_raw_value raw;
} tag;

static void add_value_to_msgbuf(loli_vm_state *, loli_msgbuf *, tag *,
        loli_value *);

static void add_list_like(loli_vm_state *vm, loli_msgbuf *msgbuf, tag *t,
        loli_value *v, const char *prefix, const char *suffix)
{
    int i;
    loli_value **values = v->value.container->values;
    int count = v->value.container->num_values;

    loli_mb_add(msgbuf, prefix);

     
    if (count != 0) {
        for (i = 0;i < count - 1;i++) {
            add_value_to_msgbuf(vm, msgbuf, t, values[i]);
            loli_mb_add(msgbuf, ", ");
        }
        if (i != count)
            add_value_to_msgbuf(vm, msgbuf, t, values[i]);
    }

    loli_mb_add(msgbuf, suffix);
}

static void add_value_to_msgbuf(loli_vm_state *vm, loli_msgbuf *msgbuf,
        tag *t, loli_value *v)
{
    if (v->flags & VAL_IS_GC_TAGGED) {
        tag *tag_iter = t;
        while (tag_iter) {
             
            if (memcmp(&tag_iter->raw, &v->value, sizeof(loli_raw_value)) == 0) {
                loli_mb_add(msgbuf, "[...]");
                return;
            }

            tag_iter = tag_iter->prev;
        }

        tag new_tag = {.prev = t, .raw = v->value};
        t = &new_tag;
    }

    int base = FLAGS_TO_BASE(v);

    if (base == V_BOOLEAN_BASE)
        add_boolean(msgbuf, v->value.integer);
    else if (base == V_INTEGER_BASE)
        add_int64(msgbuf, v->value.integer);
    else if (base == V_BYTE_BASE)
        add_byte(msgbuf, (uint8_t) v->value.integer);
    else if (base == V_DOUBLE_BASE)
        add_double(msgbuf, v->value.doubleval);
    else if (base == V_STRING_BASE)
        loli_mb_escape_add_str(msgbuf, v->value.string->string);
    else if (base == V_BYTESTRING_BASE)
        add_bytestring(msgbuf, v->value.string);
    else if (base == V_FUNCTION_BASE) {
        loli_function_val *fv = v->value.function;
        const char *builtin = "";

        if (fv->code == NULL)
            builtin = "built-in ";

        loli_mb_add_fmt(msgbuf, "<%sfunction %s>", builtin, fv->proto->name);
    }
    else if (base == V_LIST_BASE)
        add_list_like(vm, msgbuf, t, v, "[", "]");
    else if (base == V_TUPLE_BASE)
        add_list_like(vm, msgbuf, t, v, "<[", "]>");
    else if (base == V_HASH_BASE) {
        loli_hash_val *hv = v->value.hash;
        loli_mb_add_char(msgbuf, '[');
        int i, j;
        for (i = 0, j = 0;i < hv->num_bins;i++) {
            loli_hash_entry *entry = hv->bins[i];

            while (entry) {
                add_value_to_msgbuf(vm, msgbuf, t, entry->boxed_key);
                loli_mb_add(msgbuf, " => ");
                add_value_to_msgbuf(vm, msgbuf, t, entry->record);
                if (j != hv->num_entries - 1)
                    loli_mb_add(msgbuf, ", ");

                j++;
                entry = entry->next;
            }
        }
        loli_mb_add_char(msgbuf, ']');
    }
    else if (base == V_UNIT_BASE)
        loli_mb_add(msgbuf, "unit");
    else if (base == V_FILE_BASE) {
        loli_file_val *fv = v->value.file;
        const char *state = fv->inner_file ? "open" : "closed";
        loli_mb_add_fmt(msgbuf, "<%s file at %p>", state, fv);
    }
    else if (base == V_VARIANT_BASE) {
        uint16_t class_id = v->value.container->class_id;
        loli_class *variant_cls = vm->gs->class_table[class_id];

         
        if (variant_cls->parent->flags & CLS_ENUM_IS_SCOPED) {
            loli_mb_add(msgbuf, variant_cls->parent->name);
            loli_mb_add_char(msgbuf, '.');
        }

        loli_mb_add(msgbuf, variant_cls->name);
        add_list_like(vm, msgbuf, t, v, "(", ")");
    }
    else if (base == V_EMPTY_VARIANT_BASE) {
        uint16_t class_id = (uint16_t)v->value.integer;
        loli_class *variant_cls = vm->gs->class_table[class_id];

        if (variant_cls->parent->flags & CLS_ENUM_IS_SCOPED) {
            loli_mb_add(msgbuf, variant_cls->parent->name);
            loli_mb_add_char(msgbuf, '.');
        }

        loli_mb_add(msgbuf, variant_cls->name);
    }
    else {
        if (!v || !v->value.container)
            return;
        loli_container_val *cv = v->value.container;
        loli_class *cls = vm->gs->class_table[cv->class_id];
				
        loli_mb_add_fmt(msgbuf, "<%s at %p>", cls->name, v->value.generic);
    }
}

void loli_mb_add_value(loli_msgbuf *msgbuf, loli_vm_state *vm,
        loli_value *value)
{
    if (value->flags & V_STRING_FLAG)
        loli_mb_add(msgbuf, value->value.string->string);
    else
        add_value_to_msgbuf(vm, msgbuf, NULL, value);
}
