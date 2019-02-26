#ifndef LOLI_H
# define LOLI_H
# include <stdarg.h>
# include <stdint.h>
# include <stdio.h>
typedef struct loli_bytestring_val_ loli_bytestring_val;
typedef struct loli_container_val_  loli_container_val;
typedef struct loli_coroutine_val_  loli_coroutine_val;
typedef struct loli_file_val_       loli_file_val;
typedef struct loli_foreign_val_    loli_foreign_val;
typedef struct loli_function_val_   loli_function_val;
typedef struct loli_generic_val_    loli_generic_val;
typedef struct loli_hash_val_       loli_hash_val;
typedef struct loli_string_val_     loli_string_val;
typedef struct loli_value_          loli_value;
typedef struct loli_vm_state_       loli_state;
typedef void (*loli_destroy_func)(loli_generic_val *);
typedef void (*loli_import_func)(loli_state *s, const char *target);
typedef void (*loli_render_func)(const char *content, void *data);
typedef void (*loli_call_entry_func)(loli_state *);
typedef struct loli_config_ {
    int argc;
    char **argv;
    int copy_str_input;
    int gc_start;
    int gc_multiplier;
    loli_render_func render_func;
    loli_import_func import_func;
    char sipkey[16];
    void *data;
} loli_config;
char *loli_get_version();
void loli_config_init(loli_config *config);
loli_config *loli_config_get(loli_state *s);
loli_state *loli_new_state(loli_config *config);
void loli_free_state(loli_state *s);
int loli_load_file(loli_state *s, const char *path);
int loli_load_string(loli_state *s, const char *context, const char *str);
int loli_parse_content(loli_state *s);
int loli_render_content(loli_state *s);
int loli_parse_expr(loli_state *s, const char **output);
const char *loli_error_message(loli_state *s);
const char *loli_error_message_no_trace(loli_state *s);
void loli_default_import_func(loli_state *s, const char *target);
int loli_import_file(loli_state *s, const char *target);
int loli_import_library(loli_state *s, const char *target);
int loli_import_library_data(loli_state *s, const char *target,
                             const char **info_table,
                             loli_call_entry_func *call_table);
int loli_import_string(loli_state *s, const char *target, const char *content);
void loli_import_use_local_dir(loli_state *s, const char *dir);
const char *loli_import_current_root_dir(loli_state *s);
#define LOLI_ID_UNSET         0
#define LOLI_ID_INTEGER       1
#define LOLI_ID_DOUBLE        2
#define LOLI_ID_STRING        3
#define LOLI_ID_BYTE          4
#define LOLI_ID_BYTESTRING    5
#define LOLI_ID_BOOLEAN       6
#define LOLI_ID_FUNCTION      7
#define LOLI_ID_LIST          8
#define LOLI_ID_HASH          9
#define LOLI_ID_TUPLE        10
#define LOLI_ID_FILE         11
#define LOLI_ID_OPTION       12
#define LOLI_ID_SOME         13
#define LOLI_ID_NONE         14
#define LOLI_ID_RESULT       15
#define LOLI_ID_FAILURE      16
#define LOLI_ID_SUCCESS      17
#define LOLI_ID_EXCEPTION    18
#define LOLI_ID_IOERROR      19
#define LOLI_ID_KEYERROR     20
#define LOLI_ID_RUNTIMEERROR 21
#define LOLI_ID_VALUEERROR   22
#define LOLI_ID_INDEXERROR   23
#define LOLI_ID_DBZERROR     24  
#define LOLI_ID_UNIT         25
#define LOLI_ID_COROUTINE    26
#define START_CLASS_ID       27
char *loli_bytestring_raw(loli_bytestring_val *byte_val);
int loli_bytestring_length(loli_bytestring_val *byte_val);
loli_value *loli_con_get(loli_container_val *con, int index);
void loli_con_set(loli_container_val *con, int index, loli_value *value);
void loli_con_set_from_stack(loli_state *s, loli_container_val *con, int index);
uint32_t loli_con_size(loli_container_val *con);
FILE *loli_file_for_read(loli_state *s, loli_file_val *file);
FILE *loli_file_for_write(loli_state *s, loli_file_val *file);
int loli_function_is_foreign(loli_function_val *func);
int loli_function_is_native(loli_function_val *func);
loli_value *loli_hash_get(loli_state *s, loli_hash_val *hash, loli_value *key);
void loli_hash_set(loli_state *s, loli_hash_val *hash, loli_value *key, loli_value *record);
void loli_hash_set_from_stack(loli_state *s, loli_hash_val *hash);
int loli_hash_take(loli_state *s, loli_hash_val *hash, loli_value *key);
void loli_list_insert(loli_container_val *con, int index, loli_value *value);
void loli_list_reserve(loli_container_val *con, int size);
void loli_list_take(loli_state *s, loli_container_val *con, int index);
void loli_list_push(loli_container_val *con, loli_value *value);
char *loli_string_raw(loli_string_val *string_val);
int loli_string_length(loli_string_val *string_val);
int                  loli_arg_boolean   (loli_state *s, int index);
uint8_t              loli_arg_byte      (loli_state *s, int index);
loli_bytestring_val *loli_arg_bytestring(loli_state *s, int index);
loli_container_val * loli_arg_container (loli_state *s, int index);
loli_coroutine_val * loli_arg_coroutine (loli_state *s, int index);
double               loli_arg_double    (loli_state *s, int index);
loli_file_val *      loli_arg_file      (loli_state *s, int index);
loli_function_val *  loli_arg_function  (loli_state *s, int index);
loli_generic_val *   loli_arg_generic   (loli_state *s, int index);
loli_hash_val *      loli_arg_hash      (loli_state *s, int index);
int64_t              loli_arg_integer   (loli_state *s, int index);
loli_string_val *    loli_arg_string    (loli_state *s, int index);
char *               loli_arg_string_raw(loli_state *s, int index);
loli_value *         loli_arg_value     (loli_state *s, int index);
int loli_arg_count(loli_state *s);
int loli_arg_isa(loli_state *s, int index, uint16_t class_id);
#define loli_arg_is_failure(s, index) loli_arg_isa(s, index, LOLI_ID_FAILURE)
#define loli_arg_is_none(s, index) loli_arg_isa(s, index, LOLI_ID_NONE)
#define loli_arg_is_some(s, index) loli_arg_isa(s, index, LOLI_ID_SOME)
#define loli_arg_is_success(s, index) loli_arg_isa(s, index, LOLI_ID_SUCCESS)
void                loli_push_boolean      (loli_state *s, int value);
void                loli_push_byte         (loli_state *s, uint8_t value);
void                loli_push_bytestring   (loli_state *s, const char *source,
                                            int size);
void                loli_push_double       (loli_state *s, double value);
void                loli_push_empty_variant(loli_state *s, uint16_t class_id);
void                loli_push_file         (loli_state *s, FILE *f,
                                            const char *mode);
loli_foreign_val *  loli_push_foreign      (loli_state *s, uint16_t class_id,
                                            loli_destroy_func destroy_fn,
                                            size_t size);
loli_hash_val *     loli_push_hash         (loli_state *s, int size);
loli_container_val *loli_push_instance     (loli_state *s, uint16_t class_id,
                                            uint32_t size);
void                loli_push_integer      (loli_state *s, int64_t value);
loli_container_val *loli_push_list         (loli_state *s, uint32_t size);
void                loli_push_string       (loli_state *s, const char *source);
void                loli_push_string_sized (loli_state *s, const char *source, int size);
loli_container_val *loli_push_super        (loli_state *s, uint16_t class_id,
                                            uint32_t size);
loli_container_val *loli_push_tuple        (loli_state *s, uint32_t size);
void                loli_push_unit         (loli_state *s);
void                loli_push_value        (loli_state *s, loli_value *value);
loli_container_val *loli_push_variant      (loli_state *s, uint16_t class_id,
                                            uint32_t size);
# define loli_push_failure(s) loli_push_variant(s, LOLI_ID_FAILURE, 1)
# define loli_push_none(s) loli_push_empty_variant(s, LOLI_ID_NONE)
# define loli_push_some(s) loli_push_variant(s, LOLI_ID_SOME, 1)
# define loli_push_success(s) loli_push_variant(s, LOLI_ID_SUCCESS, 1)
void loli_return_boolean(loli_state *s, int value);
void loli_return_byte   (loli_state *s, uint8_t value);
void loli_return_double (loli_state *s, double value);
void loli_return_integer(loli_state *s, int64_t value);
void loli_return_none   (loli_state *s);
void loli_return_super  (loli_state *s);
void loli_return_top    (loli_state *s);
void loli_return_unit   (loli_state *s);
void loli_return_value  (loli_state *s, loli_value *value);
int                  loli_as_boolean   (loli_value *value);
uint8_t              loli_as_byte      (loli_value *value);
loli_bytestring_val *loli_as_bytestring(loli_value *value);
loli_container_val * loli_as_container (loli_value *value);
loli_coroutine_val * loli_as_coroutine (loli_value *value);
double               loli_as_double    (loli_value *value);
loli_file_val *      loli_as_file      (loli_value *value);
loli_function_val *  loli_as_function  (loli_value *value);
loli_generic_val *   loli_as_generic   (loli_value *value);
loli_hash_val *      loli_as_hash      (loli_value *value);
int64_t              loli_as_integer   (loli_value *value);
loli_string_val *    loli_as_string    (loli_value *value);
char *               loli_as_string_raw(loli_value *value);
void loli_call(loli_state *s, int count);
void loli_call_prepare(loli_state *s, loli_function_val *func);
loli_value *loli_call_result(loli_state *s);
void loli_DivisionByZeroError(loli_state *s, const char *format, ...);
void loli_IndexError(loli_state *s, const char *format, ...);
void loli_IOError(loli_state *s, const char *format, ...);
void loli_KeyError(loli_state *s, const char *format, ...);
void loli_RuntimeError(loli_state *s, const char *format, ...);
void loli_ValueError(loli_state *s, const char *format, ...);
typedef void (*loli_error_callback_func)(loli_state *s);
void loli_error_callback_push(loli_state *s, loli_error_callback_func callback_fn);
void loli_error_callback_pop(loli_state *s);
void loli_stack_drop_top(loli_state *s);
loli_value *loli_stack_get_top(loli_state *s);
typedef struct loli_msgbuf_ loli_msgbuf;
loli_msgbuf *loli_new_msgbuf(uint32_t size);
void loli_free_msgbuf(loli_msgbuf *msgbuf);
void loli_mb_add(loli_msgbuf *msgbuf, const char *source);
void loli_mb_add_char(loli_msgbuf *msgbuf, char ch);
void loli_mb_add_fmt(loli_msgbuf *msgbuf, const char *format, ...);
void loli_mb_add_fmt_va(loli_msgbuf *msgbuf, const char *format, va_list);
void loli_mb_add_slice(loli_msgbuf *msgbuf, const char *source, int start, int end);
void loli_mb_add_value(loli_msgbuf *msgbuf, loli_state *s, loli_value *value);
loli_msgbuf *loli_mb_flush(loli_msgbuf *msgbuf);
const char *loli_mb_raw(loli_msgbuf *msgbuf);
int loli_mb_pos(loli_msgbuf *msgbuf);
const char *loli_mb_sprintf(loli_msgbuf *msgbuf, const char *format, ...);
loli_msgbuf *loli_msgbuf_get(loli_state *);
#define LOLI_FOREIGN_HEADER \
uint32_t refcount; \
uint16_t class_id; \
uint16_t do_not_use; \
loli_destroy_func destroy_func;
loli_function_val *loli_find_function(loli_state *s, const char *name);
void loli_module_register(loli_state *s, const char *name,
                          const char **info_table,
                          loli_call_entry_func *call_table);
int loli_is_valid_utf8(const char *source);
void loli_value_tag(loli_state *s, loli_value *value);
uint16_t loli_cid_at(loli_state *s, int index);
#endif
