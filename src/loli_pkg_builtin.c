#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <math.h>

#include "loli.h"

#include "loli_parser.h"
#include "loli_symtab.h"
#include "loli_utf8.h"
#include "loli_value_structs.h"
#include "loli_value_raw.h"
#include "loli_value_flags.h"
#include "loli_alloc.h"

const char *loli_builtin_info_table[] = {
    "\0\0"
    ,"N\02Boolean\0"
    ,"m\0to_i\0(Boolean): Integer"
    ,"m\0to_s\0(Boolean): String"
    ,"N\02Byte\0"
    ,"m\0to_i\0(Byte): Integer"
    ,"m\0to_s\0(Byte): String"
    ,"N\04ByteString\0"
    ,"m\0each_byte\0(ByteString,Function(Byte))"
    ,"m\0to_s\0(ByteString): String"
    ,"m\0len\0(ByteString): Integer"
    ,"m\0slice\0(ByteString,*Integer,*Integer): ByteString"
    ,"N\01DivisionByZeroError\0< Exception"
    ,"m\0<new>\0(String): DivisionByZeroError"
    ,"N\012Coroutine\0[A,B]"
    ,"m\0build\0[A,B](Function(Coroutine[A,B])): Coroutine[A,B]"
    ,"m\0build_with_value\0[A,B,C](Function(Coroutine[A,B], C),C): Coroutine[A,B]"
    ,"m\0is_done\0[A,B](Coroutine[A,B]): Boolean"
    ,"m\0is_failed\0[A,B](Coroutine[A,B]): Boolean"
    ,"m\0is_waiting\0[A,B](Coroutine[A,B]): Boolean"
    ,"m\0is_running\0[A,B](Coroutine[A,B]): Boolean"
    ,"m\0receive\0[A,B](Coroutine[A,B]): B"
    ,"m\0resume\0[A,B](Coroutine[A,Unit]): Option[A]"
    ,"m\0resume_with\0[A,B](Coroutine[A,B],B): Option[A]"
    ,"m\0yield\0[A,B](Coroutine[A,B],A)"
    ,"N\01Double\0"
    ,"m\0to_i\0(Double): Integer"
    ,"N\03Exception\0"
    ,"m\0<new>\0(String): Exception"
    ,"3\0message\0String"
    ,"3\0traceback\0List[String]"
    ,"N\010File\0"
    ,"m\0close\0(File)"
    ,"m\0each_line\0(File,Function(ByteString))"
    ,"m\0flush\0(File)"
    ,"m\0open\0(String,String): File"
    ,"m\0print\0[A](File,A)"
    ,"m\0read\0(File,*Integer): ByteString"
    ,"m\0read_line\0(File): ByteString"
    ,"m\0write\0[A](File,A)"
    ,"N\0Function\0"
    ,"N\013Hash\0[A,B]"
    ,"m\0clear\0[A,B](Hash[A,B])"
    ,"m\0delete\0[A,B](Hash[A,B],A)"
    ,"m\0each_pair\0[A,B](Hash[A,B],Function(A, B))"
    ,"m\0get\0[A,B](Hash[A,B],A): Option[B]"
    ,"m\0has_key\0[A,B](Hash[A,B],A): Boolean"
    ,"m\0keys\0[A,B](Hash[A,B]): List[A]"
    ,"m\0map_values\0[A,B,C](Hash[A,B],Function(B=>C)): Hash[A,C]"
    ,"m\0merge\0[A,B](Hash[A,B],Hash[A,B]...): Hash[A,B]"
    ,"m\0reject\0[A,B](Hash[A,B],Function(A, B=>Boolean)): Hash[A,B]"
    ,"m\0select\0[A,B](Hash[A,B],Function(A, B=>Boolean)): Hash[A,B]"
    ,"m\0len\0[A,B](Hash[A,B]): Integer"
    ,"N\01IndexError\0< Exception"
    ,"m\0<new>\0(String): IndexError"
    ,"N\04Integer\0"
    ,"m\0to_bool\0(Integer): Boolean"
    ,"m\0to_b\0(Integer): Byte"
    ,"m\0to_d\0(Integer): Double"
    ,"m\0to_s\0(Integer): String"
    ,"N\01IOError\0< Exception"
    ,"m\0<new>\0(String): IOError"
    ,"N\01KeyError\0< Exception"
    ,"m\0<new>\0(String): KeyError"
    ,"N\026List\0[A]"
    ,"m\0append\0[A](List[A], List[A]): List[A]"
    ,"m\0clear\0[A](List[A])"
    ,"m\0count\0[A](List[A],Function(A=>Boolean)): Integer"
    ,"m\0delete_at\0[A](List[A],Integer)"
    ,"m\0each\0[A](List[A],Function(A)): List[A]"
    ,"m\0each_index\0[A](List[A],Function(Integer)): List[A]"
    ,"m\0fold\0[A](List[A],A,Function(A, A=>A)): A"
    ,"m\0fill\0[A](Integer,Function(Integer=>A)): List[A]"
    ,"m\0get\0[A](List[A],Integer): Option[A]"
    ,"m\0insert\0[A](List[A],Integer,A)"
    ,"m\0join\0[A](List[A],*String): String"
    ,"m\0map\0[A,B](List[A],Function(A=>B)): List[B]"
    ,"m\0pop\0[A](List[A]): A"
    ,"m\0push\0[A](List[A],A): List[A]"
    ,"m\0reject\0[A](List[A],Function(A=>Boolean)): List[A]"
    ,"m\0repeat\0[A](Integer,A): List[A]"
    ,"m\0select\0[A](List[A],Function(A=>Boolean)): List[A]"
    ,"m\0len\0[A](List[A]): Integer"
    ,"m\0shift\0[A](List[A]): A"
    ,"m\0slice\0[A](List[A],*Integer,*Integer): List[A]"
    ,"m\0unshift\0[A](List[A],A)"
    ,"m\0zip\0[A](List[A],List[$1]...): List[Tuple[A,$1]]"
    ,"E\012Option\0[A]"
    ,"m\0and\0[A,B](Option[A],Option[B]): Option[B]"
    ,"m\0and_then\0[A,B](Option[A],Function(A=>Option[B])): Option[B]"
    ,"m\0is_none\0[A](Option[A]): Boolean"
    ,"m\0is_some\0[A](Option[A]): Boolean"
    ,"m\0map\0[A,B](Option[A],Function(A=>B)): Option[B]"
    ,"m\0or\0[A](Option[A],Option[A]): Option[A]"
    ,"m\0or_else\0[A](Option[A],Function(=>Option[A])): Option[A]"
    ,"m\0unwrap\0[A](Option[A]): A"
    ,"m\0unwrap_or\0[A](Option[A],A): A"
    ,"m\0unwrap_or_else\0[A](Option[A],Function(=>A)): A"
    ,"V\0Some\0(A)"
    ,"V\0None\0"
    ,"E\04Result\0[A,B]"
    ,"m\0failure\0[A,B](Result[A,B]): Option[A]"
    ,"m\0is_failure\0[A,B](Result[A,B]): Boolean"
    ,"m\0is_success\0[A,B](Result[A,B]): Boolean"
    ,"m\0success\0[A,B](Result[A,B]): Option[B]"
    ,"V\0Failure\0(A)"
    ,"V\0Success\0(B)"
    ,"N\01RuntimeError\0< Exception"
    ,"m\0<new>\0(String): RuntimeError"
    ,"N\021String\0"
    ,"m\0format\0(String,$1...): String"
    ,"m\0ends_with\0(String,String): Boolean"
    ,"m\0find\0(String,String,*Integer): Option[Integer]"
    ,"m\0is_alnum\0(String): Boolean"
    ,"m\0is_alpha\0(String): Boolean"
    ,"m\0is_digit\0(String): Boolean"
    ,"m\0is_space\0(String): Boolean"
    ,"m\0lower\0(String): String"
    ,"m\0char_at\0(String,Integer): Byte" 
    ,"m\0len\0(String): Integer" 
    ,"m\0lstrip\0(String,String): String"
    ,"m\0to_i\0(String): Integer"
    ,"m\0to_d\0(String): Double"
    ,"m\0replace\0(String,String,String): String"
    ,"m\0rstrip\0(String,String): String"
    ,"m\0slice\0(String,*Integer,*Integer): String"
    ,"m\0split\0(String,*String): List[String]"
    ,"m\0starts_with\0(String,String): Boolean"
    ,"m\0strip\0(String,String): String"
    ,"m\0to_bs\0(String): ByteString"
    ,"m\0trim\0(String): String"
    ,"m\0upper\0(String): String"
    ,"N\0Tuple\0"
    ,"N\01ValueError\0< Exception"
    ,"m\0<new>\0(String): ValueError"
    ,"F\0sayln\0[A](A...)"
    ,"F\0say\0[A](A...)"
    ,"F\0input\0(String...): ByteString"
    ,"F\0range\0(Integer, *Integer): List[Integer]"
    ,"F\0calltrace\0: List[String]"
    ,"R\0stdin\0File"
    ,"R\0stderr\0File"
    ,"R\0stdout\0File"
    ,"Z"
};
#define Boolean_OFFSET 1
#define Byte_OFFSET 4
#define ByteString_OFFSET 8
#define DivisionByZeroError_OFFSET 12
#define Coroutine_OFFSET 14
#define Double_OFFSET 25
#define Exception_OFFSET 27
#define File_OFFSET 31
#define Function_OFFSET 40
#define Hash_OFFSET 41
#define IndexError_OFFSET 53
#define Integer_OFFSET 55
#define IOError_OFFSET 60
#define KeyError_OFFSET 62
#define List_OFFSET 64
#define Option_OFFSET 87
#define Result_OFFSET 100
#define RuntimeError_OFFSET 107
#define String_OFFSET 109
#define Tuple_OFFSET 131
#define ValueError_OFFSET 132
#define toplevel_OFFSET 134
void loli_builtin_Boolean_to_i(loli_state *);
void loli_builtin_Boolean_to_s(loli_state *);
void loli_builtin_Byte_to_i(loli_state *);
void loli_builtin_Byte_to_s(loli_state *);
void loli_builtin_ByteString_each_byte(loli_state *);
void loli_builtin_ByteString_to_s(loli_state *);
void loli_builtin_ByteString_size(loli_state *);
void loli_builtin_ByteString_slice(loli_state *);
void loli_builtin_DivisionByZeroError_new(loli_state *);
void loli_builtin_Coroutine_build(loli_state *);
void loli_builtin_Coroutine_build_with_value(loli_state *);
void loli_builtin_Coroutine_is_done(loli_state *);
void loli_builtin_Coroutine_is_failed(loli_state *);
void loli_builtin_Coroutine_is_waiting(loli_state *);
void loli_builtin_Coroutine_is_running(loli_state *);
void loli_builtin_Coroutine_receive(loli_state *);
void loli_builtin_Coroutine_resume(loli_state *);
void loli_builtin_Coroutine_resume_with(loli_state *);
void loli_builtin_Coroutine_yield(loli_state *);
void loli_builtin_Double_to_i(loli_state *);
void loli_builtin_Exception_new(loli_state *);
void loli_builtin_File_close(loli_state *);
void loli_builtin_File_each_line(loli_state *);
void loli_builtin_File_flush(loli_state *);
void loli_builtin_File_open(loli_state *);
void loli_builtin_File_print(loli_state *);
void loli_builtin_File_read(loli_state *);
void loli_builtin_File_read_line(loli_state *);
void loli_builtin_File_write(loli_state *);
void loli_builtin_Hash_clear(loli_state *);
void loli_builtin_Hash_delete(loli_state *);
void loli_builtin_Hash_each_pair(loli_state *);
void loli_builtin_Hash_get(loli_state *);
void loli_builtin_Hash_has_key(loli_state *);
void loli_builtin_Hash_keys(loli_state *);
void loli_builtin_Hash_map_values(loli_state *);
void loli_builtin_Hash_merge(loli_state *);
void loli_builtin_Hash_reject(loli_state *);
void loli_builtin_Hash_select(loli_state *);
void loli_builtin_Hash_size(loli_state *);
void loli_builtin_IndexError_new(loli_state *);
void loli_builtin_Integer_to_bool(loli_state *);
void loli_builtin_Integer_to_byte(loli_state *);
void loli_builtin_Integer_to_d(loli_state *);
void loli_builtin_Integer_to_s(loli_state *);
void loli_builtin_IOError_new(loli_state *);
void loli_builtin_KeyError_new(loli_state *);
void loli_builtin_List_append(loli_state *);
void loli_builtin_List_clear(loli_state *);
void loli_builtin_List_count(loli_state *);
void loli_builtin_List_delete_at(loli_state *);
void loli_builtin_List_each(loli_state *);
void loli_builtin_List_each_index(loli_state *);
void loli_builtin_List_fold(loli_state *);
void loli_builtin_List_fill(loli_state *);
void loli_builtin_List_get(loli_state *);
void loli_builtin_List_insert(loli_state *);
void loli_builtin_List_join(loli_state *);
void loli_builtin_List_map(loli_state *);
void loli_builtin_List_pop(loli_state *);
void loli_builtin_List_push(loli_state *);
void loli_builtin_List_reject(loli_state *);
void loli_builtin_List_repeat(loli_state *);
void loli_builtin_List_select(loli_state *);
void loli_builtin_List_size(loli_state *);
void loli_builtin_List_shift(loli_state *);
void loli_builtin_List_slice(loli_state *);
void loli_builtin_List_unshift(loli_state *);
void loli_builtin_List_zip(loli_state *);
void loli_builtin_Option_and(loli_state *);
void loli_builtin_Option_and_then(loli_state *);
void loli_builtin_Option_is_none(loli_state *);
void loli_builtin_Option_is_some(loli_state *);
void loli_builtin_Option_map(loli_state *);
void loli_builtin_Option_or(loli_state *);
void loli_builtin_Option_or_else(loli_state *);
void loli_builtin_Option_unwrap(loli_state *);
void loli_builtin_Option_unwrap_or(loli_state *);
void loli_builtin_Option_unwrap_or_else(loli_state *);
void loli_builtin_Result_failure(loli_state *);
void loli_builtin_Result_is_failure(loli_state *);
void loli_builtin_Result_is_success(loli_state *);
void loli_builtin_Result_success(loli_state *);
void loli_builtin_RuntimeError_new(loli_state *);
void loli_builtin_String_format(loli_state *);
void loli_builtin_String_ends_with(loli_state *);
void loli_builtin_String_find(loli_state *);
void loli_builtin_String_is_alnum(loli_state *);
void loli_builtin_String_is_alpha(loli_state *);
void loli_builtin_String_is_digit(loli_state *);
void loli_builtin_String_is_space(loli_state *);
void loli_builtin_String_lower(loli_state *);
void loli_builtin_String_char_at(loli_state *);
void loli_builtin_String_len(loli_state *);
void loli_builtin_String_lstrip(loli_state *);
void loli_builtin_String_to_i(loli_state *);
void loli_builtin_String_to_d(loli_state *);
void loli_builtin_String_replace(loli_state *);
void loli_builtin_String_rstrip(loli_state *);
void loli_builtin_String_slice(loli_state *);
void loli_builtin_String_split(loli_state *);
void loli_builtin_String_starts_with(loli_state *);
void loli_builtin_String_strip(loli_state *);
void loli_builtin_String_to_bytestring(loli_state *);
void loli_builtin_String_trim(loli_state *);
void loli_builtin_String_upper(loli_state *);
void loli_builtin_ValueError_new(loli_state *);
void loli_builtin__sayln(loli_state *);
void loli_builtin__say(loli_state *);
void loli_builtin_input(loli_state *);
void loli_builtin_range(loli_state *);
void loli_builtin__calltrace(loli_state *);
void loli_builtin_var_stdin(loli_state *);
void loli_builtin_var_stderr(loli_state *);
void loli_builtin_var_stdout(loli_state *);
loli_call_entry_func loli_builtin_call_table[] = {
    NULL,
    NULL,
    loli_builtin_Boolean_to_i,
    loli_builtin_Boolean_to_s,
    NULL,
    loli_builtin_Byte_to_i,
    loli_builtin_Byte_to_s,
    NULL,
    loli_builtin_ByteString_each_byte,
    loli_builtin_ByteString_to_s,
    loli_builtin_ByteString_size,
    loli_builtin_ByteString_slice,
    NULL,
    loli_builtin_DivisionByZeroError_new,
    NULL,
    loli_builtin_Coroutine_build,
    loli_builtin_Coroutine_build_with_value,
    loli_builtin_Coroutine_is_done,
    loli_builtin_Coroutine_is_failed,
    loli_builtin_Coroutine_is_waiting,
    loli_builtin_Coroutine_is_running,
    loli_builtin_Coroutine_receive,
    loli_builtin_Coroutine_resume,
    loli_builtin_Coroutine_resume_with,
    loli_builtin_Coroutine_yield,
    NULL,
    loli_builtin_Double_to_i,
    NULL,
    loli_builtin_Exception_new,
    NULL,
    NULL,
    NULL,
    loli_builtin_File_close,
    loli_builtin_File_each_line,
    loli_builtin_File_flush,
    loli_builtin_File_open,
    loli_builtin_File_print,
    loli_builtin_File_read,
    loli_builtin_File_read_line,
    loli_builtin_File_write,
    NULL,
    NULL,
    loli_builtin_Hash_clear,
    loli_builtin_Hash_delete,
    loli_builtin_Hash_each_pair,
    loli_builtin_Hash_get,
    loli_builtin_Hash_has_key,
    loli_builtin_Hash_keys,
    loli_builtin_Hash_map_values,
    loli_builtin_Hash_merge,
    loli_builtin_Hash_reject,
    loli_builtin_Hash_select,
    loli_builtin_Hash_size,
    NULL,
    loli_builtin_IndexError_new,
    NULL,
    loli_builtin_Integer_to_bool,
    loli_builtin_Integer_to_byte,
    loli_builtin_Integer_to_d,
    loli_builtin_Integer_to_s,
    NULL,
    loli_builtin_IOError_new,
    NULL,
    loli_builtin_KeyError_new,
    NULL,
    loli_builtin_List_append,
    loli_builtin_List_clear,
    loli_builtin_List_count,
    loli_builtin_List_delete_at,
    loli_builtin_List_each,
    loli_builtin_List_each_index,
    loli_builtin_List_fold,
    loli_builtin_List_fill,
    loli_builtin_List_get,
    loli_builtin_List_insert,
    loli_builtin_List_join,
    loli_builtin_List_map,
    loli_builtin_List_pop,
    loli_builtin_List_push,
    loli_builtin_List_reject,
    loli_builtin_List_repeat,
    loli_builtin_List_select,
    loli_builtin_List_size,
    loli_builtin_List_shift,
    loli_builtin_List_slice,
    loli_builtin_List_unshift,
    loli_builtin_List_zip,
    NULL,
    loli_builtin_Option_and,
    loli_builtin_Option_and_then,
    loli_builtin_Option_is_none,
    loli_builtin_Option_is_some,
    loli_builtin_Option_map,
    loli_builtin_Option_or,
    loli_builtin_Option_or_else,
    loli_builtin_Option_unwrap,
    loli_builtin_Option_unwrap_or,
    loli_builtin_Option_unwrap_or_else,
    NULL,
    NULL,
    NULL,
    loli_builtin_Result_failure,
    loli_builtin_Result_is_failure,
    loli_builtin_Result_is_success,
    loli_builtin_Result_success,
    NULL,
    NULL,
    NULL,
    loli_builtin_RuntimeError_new,
    NULL,
    loli_builtin_String_format,
    loli_builtin_String_ends_with,
    loli_builtin_String_find,    
    loli_builtin_String_is_alnum,
    loli_builtin_String_is_alpha,
    loli_builtin_String_is_digit,
    loli_builtin_String_is_space,
    loli_builtin_String_lower,
    loli_builtin_String_char_at,
    loli_builtin_String_len,
    loli_builtin_String_lstrip,
    loli_builtin_String_to_i,
    loli_builtin_String_to_d,
    loli_builtin_String_replace,
    loli_builtin_String_rstrip,
    loli_builtin_String_slice,
    loli_builtin_String_split,
    loli_builtin_String_starts_with,
    loli_builtin_String_strip,
    loli_builtin_String_to_bytestring,
    loli_builtin_String_trim,
    loli_builtin_String_upper,
    NULL,
    NULL,
    loli_builtin_ValueError_new,
    loli_builtin__sayln,
    loli_builtin__say,
    loli_builtin_input,
    loli_builtin_range,
    loli_builtin__calltrace,
    loli_builtin_var_stdin,
    loli_builtin_var_stderr,
    loli_builtin_var_stdout,
};

const loli_gc_entry loli_gc_stopper =
{
    1,
    1,
    {.integer = 1},
    NULL
};

static const loli_class raw_self =
{
    NULL,
    ITEM_TYPE_CLASS,
    0,
    LOLI_ID_SELF,
    0,
    (loli_type *)&raw_self,
    "self",
    0,
    NULL,
    NULL,
    0,
    0,
    {0},
    0,
    NULL,
    NULL,
};

const loli_class *loli_self_class = &raw_self;

static const loli_class raw_unit =
{
    NULL,
    ITEM_TYPE_CLASS,
    0,
    LOLI_ID_UNIT,
    0,
    (loli_type *)&raw_unit,
    "Unit",
    0,
    NULL,
    NULL,
    0,
    0,
    {0},
    0,
    NULL,
    NULL,
};

const loli_type *loli_unit_type = (loli_type *)&raw_unit;

static const loli_class raw_question =
{
    NULL,
    ITEM_TYPE_CLASS,
    TYPE_IS_INCOMPLETE,
    LOLI_ID_QUESTION,
    0,
    (loli_type *)&raw_question,
    "?",
    0,
    NULL,
    NULL,
    0,
    0,
    {0},
    0,
    NULL,
    NULL,
};

const loli_type *loli_question_type = (loli_type *)&raw_question;

static const loli_class raw_unset =
{
    NULL,
    ITEM_TYPE_CLASS,
    0,
    LOLI_ID_UNSET,
    0,
    (loli_type *)&raw_question,
    "",
    0,
    NULL,
    NULL,
    0,
    0,
    {0},
    0,
    NULL,
    NULL,
};

const loli_type *loli_unset_type = (loli_type *)&raw_unset;






static void return_exception(loli_state *s, uint16_t id)
{
    loli_container_val *result = loli_push_super(s, id, 2);

    loli_con_set(result, 0, loli_arg_value(s, 0));

    loli_push_list(s, 0);
    loli_con_set_from_stack(s, result, 1);
    loli_return_super(s);
}


void loli_builtin_Boolean_to_i(loli_state *s)
{
    loli_return_integer(s, loli_arg_boolean(s, 0));
}

void loli_builtin_Boolean_to_s(loli_state *s)
{
    int input = loli_arg_boolean(s, 0);
    char *to_copy;

    if (input == 0)
        to_copy = "false";
    else
        to_copy = "true";

    loli_push_string(s, to_copy);
    loli_return_top(s);
}


void loli_builtin_Byte_to_i(loli_state *s)
{
    loli_return_integer(s, loli_arg_byte(s, 0));
}

void loli_builtin_Byte_to_s(loli_state *s)
{
    uint8_t integer_val = (uint8_t)loli_arg_byte(s, 0);
    char e = integer_val;
    char str[2] = "\0";
    str[0] = e;
    loli_push_string(s, str);
    loli_return_top(s);
}


void loli_builtin_ByteString_each_byte(loli_state *s)
{
    loli_bytestring_val *sv = loli_arg_bytestring(s, 0);
    const char *input = loli_bytestring_raw(sv);
    int len = loli_bytestring_length(sv);
    int i;

    loli_call_prepare(s, loli_arg_function(s, 1));

    for (i = 0;i < len;i++) {
        loli_push_byte(s, (uint8_t)input[i]);
        loli_call(s, 1);
    }
}

void loli_builtin_ByteString_to_s(loli_state *s)
{
    loli_bytestring_val *input_bytestring = loli_arg_bytestring(s, 0);

    char *byte_buffer = NULL;

    byte_buffer = loli_bytestring_raw(input_bytestring);
    int byte_buffer_size = loli_bytestring_length(input_bytestring);

    if (loli_is_valid_sized_utf8(byte_buffer, byte_buffer_size) == 0) {
        loli_ValueError(s, "Not a valid utf-8 string");
        return;
    }
    
    loli_push_string(s, byte_buffer);
    loli_return_top(s);
}

void loli_builtin_ByteString_size(loli_state *s)
{
    loli_return_integer(s, loli_arg_bytestring(s, 0)->size);
}

static const char follower_table[256] =
{
      
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
 -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
 -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
 -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
 -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
 -1,-1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  4, 4, 4, 4, 4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};

void do_str_slice(loli_state *s, int is_bytestring)
{
    loli_string_val *sv = loli_arg_string(s, 0);
    int start = 0;
    int stop = sv->size;

    switch (loli_arg_count(s)) {
        case 3: stop = loli_arg_integer(s, 2);
        case 2: start = loli_arg_integer(s, 1);
    }

    if (stop < 0)
        stop = sv->size + stop;
    if (start < 0)
        start = sv->size + start;

    if (stop > sv->size ||
        start > sv->size ||
        start > stop) {
        if (is_bytestring == 0)
            loli_IndexError(s, "String index out of range");
        else
            loli_IndexError(s, "ByteString index out of range");

        return;
    }

    char *raw = loli_string_raw(sv);
    if (is_bytestring == 0) {
        if (follower_table[(unsigned char)raw[start]] == -1 ||
            follower_table[(unsigned char)raw[stop]] == -1) {
            loli_ValueError(s, "Not a valid string");
            return;
        }
    }

    if (is_bytestring == 0)
        loli_push_string_sized(s, raw + start, stop - start);
    else
        loli_push_bytestring(s, raw + start, stop - start);

    loli_return_top(s);
}

void loli_builtin_ByteString_slice(loli_state *s)
{
    do_str_slice(s, 1);
}

void loli_builtin_DivisionByZeroError_new(loli_state *s)
{
    return_exception(s, LOLI_ID_DBZERROR);
}





#define CORO_IS(name, to_check) \
void loli_builtin_Coroutine_is_##name(loli_state *s) \
{ \
    loli_coroutine_val *co_val = loli_arg_coroutine(s, 0); \
    loli_return_boolean(s, co_val->status == to_check); \
} \

CORO_IS(done, co_done)

CORO_IS(failed, co_failed)

CORO_IS(waiting, co_waiting)

CORO_IS(running, co_running)






void loli_builtin_Double_to_i(loli_state *s)
{
    int64_t integer_val = (int64_t)loli_arg_double(s, 0);

    loli_return_integer(s, integer_val);
}

void loli_builtin_Exception_new(loli_state *s)
{
    return_exception(s, LOLI_ID_EXCEPTION);
}


void loli_builtin_File_close(loli_state *s)
{
    loli_file_val *filev = loli_arg_file(s, 0);

    if (filev->inner_file != NULL) {
        if (filev->is_builtin == 0)
            fclose(filev->inner_file);
        filev->inner_file = NULL;
    }

    loli_return_unit(s);
}

static int read_file_line(loli_msgbuf *msgbuf, FILE *source)
{
    char read_buffer[128];
    int ch = 0, pos = 0, total_pos = 0;

     
    while (1) {
        ch = fgetc(source);

        if (ch == EOF)
            break;

        if (pos == sizeof(read_buffer)) {
            loli_mb_add_slice(msgbuf, read_buffer, 0, sizeof(read_buffer));
            total_pos += pos;
            pos = 0;
        }

        read_buffer[pos] = (char)ch;
        pos++;

         
        if (ch == '\n')
            break;
    }

    if (pos != 0) {
        loli_mb_add_slice(msgbuf, read_buffer, 0, pos);
        total_pos += pos;
    }

    return total_pos;
}

void loli_builtin_File_each_line(loli_state *s)
{
    loli_file_val *filev = loli_arg_file(s, 0);
    loli_msgbuf *vm_buffer = loli_msgbuf_get(s);
    FILE *f = loli_file_for_read(s, filev);

    loli_call_prepare(s, loli_arg_function(s, 1));

    while (1) {
        int total_bytes = read_file_line(vm_buffer, f);

        if (total_bytes == 0)
            break;

        const char *text = loli_mb_raw(vm_buffer);
        loli_push_bytestring(s, text, total_bytes);
        loli_call(s, 1);
        loli_mb_flush(vm_buffer);
    }

    loli_return_unit(s);
}

void loli_builtin_File_flush(loli_state *s)
{
    loli_file_val *filev = loli_arg_file(s, 0);
    FILE *f = loli_file_for_write(s, filev);

    fflush(f);

    loli_return_unit(s);
}

void loli_builtin_File_open(loli_state *s)
{
    char *path = loli_arg_string_raw(s, 0);
    char *mode = loli_arg_string_raw(s, 1);

    errno = 0;
    int ok;

    {
        char *mode_ch = mode;
        if (*mode_ch == 'r' || *mode_ch == 'w' || *mode_ch == 'a') {
            mode_ch++;
            if (*mode_ch == 'b')
                mode_ch++;

            if (*mode_ch == '+')
                mode_ch++;

            ok = (*mode_ch == '\0');
        }
        else
            ok = 0;
    }

    if (ok == 0)
        loli_IOError(s, "Invalid mode '%s' given.", mode);

    FILE *f = fopen(path, mode);
    if (f == NULL) {
         
        char buffer[128];
#ifdef _WIN32
        strerror_s(buffer, sizeof(buffer), errno);
#else
        strerror_r(errno, buffer, sizeof(buffer));
#endif
        loli_IOError(s, "Errno %d: %s (%s).", errno, buffer, path);
    }

    loli_push_file(s, f, mode);
    loli_return_top(s);
}

void loli_builtin_File_write(loli_state *);

void loli_builtin_File_print(loli_state *s)
{
    loli_builtin_File_write(s);
    fputc('\n', loli_file_for_write(s, loli_arg_file(s, 0)));
    loli_return_unit(s);
}

void loli_builtin_File_read(loli_state *s)
{
    loli_file_val *filev = loli_arg_file(s,0);
    FILE *raw_file = loli_file_for_read(s, filev);
    int need = -1;
    if (loli_arg_count(s) == 2)
        need = loli_arg_integer(s, 1);

     
    if (need < -1)
        need = -1;

    size_t bufsize = 64;
    char *buffer = loli_malloc(bufsize * sizeof(*buffer));
    int pos = 0, nread;
    int nbuf = bufsize/2;

    while (1) {
        int to_read;
         
        if (need == -1 || need > nbuf)
            to_read = nbuf;
        else
            to_read = need;

        nread = fread(buffer+pos, 1, to_read, raw_file);
        pos += nread;

        if (pos >= bufsize) {
            nbuf = bufsize;
            bufsize *= 2;
            buffer = loli_realloc(buffer, bufsize * sizeof(*buffer));
        }

         
        if (nread < to_read || (pos >= need && need != -1)) {
            buffer[pos] = '\0';
            break;
        }
        else if (to_read != -1)
            to_read -= nread;
    }

    loli_push_bytestring(s, buffer, pos);
    loli_free(buffer);
    loli_return_top(s);
}

void loli_builtin_File_read_line(loli_state *s)
{
    loli_file_val *filev = loli_arg_file(s, 0);
    loli_msgbuf *vm_buffer = loli_msgbuf_get(s);
    FILE *f = loli_file_for_read(s, filev);
    int byte_count = read_file_line(vm_buffer, f);
    const char *text = loli_mb_raw(vm_buffer);

    loli_push_bytestring(s, text, byte_count);
    loli_return_top(s);
}

void loli_builtin_File_write(loli_state *s)
{
    loli_file_val *filev = loli_arg_file(s, 0);
    loli_value *to_write = loli_arg_value(s, 1);

    FILE *inner_file = loli_file_for_write(s, filev);

    if (to_write->flags & V_STRING_FLAG)
        fputs(to_write->value.string->string, inner_file);
    else {
        loli_msgbuf *msgbuf = loli_msgbuf_get(s);
        loli_mb_add_value(msgbuf, s, to_write);
        fputs(loli_mb_raw(msgbuf), inner_file);
    }

    loli_return_unit(s);
}



static inline void remove_key_check(loli_state *s, loli_hash_val *hash_val)
{
    if (hash_val->iter_count)
        loli_RuntimeError(s, "Cannot remove key from hash during iteration.");
}

static void destroy_hash_elems(loli_hash_val *hash_val)
{
    int i;
    for (i = 0;i < hash_val->num_bins;i++) {
        loli_hash_entry *entry = hash_val->bins[i];
        loli_hash_entry *next_entry;
        while (entry) {
            loli_deref(entry->boxed_key);
            loli_free(entry->boxed_key);

            loli_deref(entry->record);
            loli_free(entry->record);

            next_entry = entry->next;
            loli_free(entry);
            entry = next_entry;
        }

        hash_val->bins[i] = NULL;
    }
}

void loli_destroy_hash(loli_value *v)
{
    loli_hash_val *hv = v->value.hash;

    destroy_hash_elems(hv);

    loli_free(hv->bins);
    loli_free(hv);
}

void loli_builtin_Hash_clear(loli_state *s)
{
    loli_hash_val *hash_val = loli_arg_hash(s, 0);

    remove_key_check(s, hash_val);
    destroy_hash_elems(hash_val);

    hash_val->num_entries = 0;

    loli_return_unit(s);
}

void loli_builtin_Hash_delete(loli_state *s)
{
    loli_hash_val *hash_val = loli_arg_hash(s, 0);

    remove_key_check(s, hash_val);

    loli_value *key = loli_arg_value(s, 1);

    if (loli_hash_take(s, hash_val, key)) {
        loli_stack_drop_top(s);
        loli_stack_drop_top(s);
    } else {
        loli_KeyError(s, "Key is not exists");
    }

    loli_return_unit(s);
}

static void hash_iter_callback(loli_state *s)
{
    loli_hash_val *hash_val = loli_arg_hash(s, 0);
    hash_val->iter_count--;
}

void loli_builtin_Hash_each_pair(loli_state *s)
{
    loli_hash_val *hash_val = loli_arg_hash(s, 0);

    loli_error_callback_push(s, hash_iter_callback);
    loli_call_prepare(s, loli_arg_function(s, 1));
    hash_val->iter_count++;

    int i;
    for (i = 0;i < hash_val->num_bins;i++) {
        loli_hash_entry *entry = hash_val->bins[i];
        while (entry) {
            loli_push_value(s, entry->boxed_key);
            loli_push_value(s, entry->record);
            loli_call(s, 2);

            entry = entry->next;
        }
    }

    loli_error_callback_pop(s);
    hash_val->iter_count--;
}

void loli_builtin_Hash_get(loli_state *s)
{
    loli_hash_val *hash_val = loli_arg_hash(s, 0);
    loli_value *key = loli_arg_value(s, 1);
    loli_value *record = loli_hash_get(s, hash_val, key);

    if (record) {
        loli_container_val *variant = loli_push_some(s);
        loli_con_set(variant, 0, record);
        loli_return_top(s);
    }
    else
        loli_return_none(s);
}

void loli_builtin_Hash_has_key(loli_state *s)
{
    loli_hash_val *hash_val = loli_arg_hash(s, 0);
    loli_value *key = loli_arg_value(s, 1);

    loli_value *entry = loli_hash_get(s, hash_val, key);

    loli_return_boolean(s, entry != NULL);
}

void loli_builtin_Hash_keys(loli_state *s)
{
    loli_hash_val *hash_val = loli_arg_hash(s, 0);
    loli_container_val *result_lv = loli_push_list(s, hash_val->num_entries);
    int i, list_i;

    for (i = 0, list_i = 0;i < hash_val->num_bins;i++) {
        loli_hash_entry *entry = hash_val->bins[i];
        while (entry) {
            loli_con_set(result_lv, list_i, entry->boxed_key);
            list_i++;
            entry = entry->next;
        }
    }

    loli_return_top(s);
}

void loli_builtin_Hash_map_values(loli_state *s)
{
    loli_hash_val *hash_val = loli_arg_hash(s, 0);

    loli_call_prepare(s, loli_arg_function(s, 1));
    loli_value *result = loli_call_result(s);

    loli_error_callback_push(s, hash_iter_callback);

    loli_hash_val *h = loli_push_hash(s, hash_val->num_entries);

    int i;
    for (i = 0;i < hash_val->num_bins;i++) {
        loli_hash_entry *entry = hash_val->bins[i];
        while (entry) {
            loli_push_value(s, entry->record);
            loli_call(s, 1);

            loli_hash_set(s, h, entry->boxed_key, result);
            entry = entry->next;
        }
    }

    hash_val->iter_count--;
    loli_error_callback_pop(s);
    loli_return_top(s);
}

void loli_builtin_Hash_merge(loli_state *s)
{
    loli_hash_val *hash_val = loli_arg_hash(s, 0);

    loli_hash_val *result_hash = loli_push_hash(s, hash_val->num_entries);

    int i, j;

    for (i = 0;i < hash_val->num_bins;i++) {
        loli_hash_entry *entry = hash_val->bins[i];
        while (entry) {
            loli_hash_set(s, result_hash, entry->boxed_key,
                    entry->record);
            entry = entry->next;
        }
    }

    loli_container_val *to_merge = loli_arg_container(s, 1);
    for (i = 0;i < to_merge->num_values;i++) {
        loli_hash_val *merging_hash = to_merge->values[i]->value.hash;
        for (j = 0;j < merging_hash->num_bins;j++) {
            loli_hash_entry *entry = merging_hash->bins[j];
            while (entry) {
                loli_hash_set(s, result_hash, entry->boxed_key,
                        entry->record);
                entry = entry->next;
            }
        }
    }

    loli_return_top(s);
}

static void hash_select_reject_common(loli_state *s, int expect)
{
    loli_hash_val *hash_val = loli_arg_hash(s, 0);
    loli_call_prepare(s, loli_arg_function(s, 1));
    loli_value *result = loli_call_result(s);
    loli_hash_val *h = loli_push_hash(s, hash_val->num_entries);

    loli_error_callback_push(s, hash_iter_callback);

    hash_val->iter_count++;

    int i;
    for (i = 0;i < hash_val->num_bins;i++) {
        loli_hash_entry *entry = hash_val->bins[i];
        while (entry) {
            loli_push_value(s, entry->boxed_key);
            loli_push_value(s, entry->record);

            loli_push_value(s, entry->boxed_key);
            loli_push_value(s, entry->record);

            loli_call(s, 2);
            if (loli_as_boolean(result) != expect) {
                loli_stack_drop_top(s);
                loli_stack_drop_top(s);
            }
            else
                loli_hash_set_from_stack(s, h);

            entry = entry->next;
        }
    }

    hash_val->iter_count--;
    loli_error_callback_pop(s);
    loli_return_top(s);
}

void loli_builtin_Hash_reject(loli_state *s)
{
    hash_select_reject_common(s, 0);
}

void loli_builtin_Hash_select(loli_state *s)
{
    hash_select_reject_common(s, 1);
}

void loli_builtin_Hash_size(loli_state *s)
{
    loli_hash_val *hash_val = loli_arg_hash(s, 0);

    loli_return_integer(s, hash_val->num_entries);
}

void loli_builtin_IndexError_new(loli_state *s)
{
    return_exception(s, LOLI_ID_INDEXERROR);
}


void loli_builtin_Integer_to_bool(loli_state *s)
{
     
    loli_return_boolean(s, !!loli_arg_integer(s, 0));
}

void loli_builtin_Integer_to_byte(loli_state *s)
{
    loli_return_byte(s, loli_arg_integer(s, 0) & 0xFF);
}

void loli_builtin_Integer_to_d(loli_state *s)
{
    double doubleval = (double)loli_arg_integer(s, 0);

    loli_return_double(s, doubleval);
}

void loli_builtin_Integer_to_s(loli_state *s)
{
    int64_t integer_val = loli_arg_integer(s, 0);

    char buffer[32];
    snprintf(buffer, 32, "%"PRId64, integer_val);

    loli_push_string(s, buffer);
    loli_return_top(s);
}

void loli_builtin_IOError_new(loli_state *s)
{
    return_exception(s, LOLI_ID_IOERROR);
}

void loli_builtin_KeyError_new(loli_state *s)
{
    return_exception(s, LOLI_ID_KEYERROR);
}

void loli_builtin_List_append(loli_state *s)
{
    loli_value *list_arg = loli_arg_value(s, 0);
    loli_value *list_arg_a = loli_arg_value(s, 1);
    
    loli_container_val *list_val = loli_as_container(list_arg);
    loli_container_val *list_val_a = loli_as_container(list_arg_a);
    
    int i;
    
    for (i = 0;i < list_val_a->num_values;i++) {
        loli_list_insert(list_val, loli_con_size(list_val), list_val_a->values[i]);
    }

    loli_return_value(s, list_arg);
}

void loli_builtin_List_clear(loli_state *s)
{
    loli_container_val *list_val = loli_arg_container(s, 0);
    int i;

    for (i = 0;i < list_val->num_values;i++) {
        loli_deref(list_val->values[i]);
        loli_free(list_val->values[i]);
    }

    list_val->extra_space += list_val->num_values;
    list_val->num_values = 0;

    loli_return_unit(s);
}

void loli_builtin_List_count(loli_state *s)
{
    loli_container_val *list_val = loli_arg_container(s, 0);
    loli_call_prepare(s, loli_arg_function(s, 1));
    loli_value *result = loli_call_result(s);
    int count = 0;

    int i;
    for (i = 0;i < list_val->num_values;i++) {
        loli_push_value(s, list_val->values[i]);
        loli_call(s, 1);

        if (loli_as_boolean(result) == 1)
            count++;
    }

    loli_return_integer(s, count);
}

static int64_t get_relative_index(loli_state *s, loli_container_val *list_val,
        int64_t pos)
{
    if (pos < 0) {
        uint64_t unsigned_pos = -(int64_t)pos;
        if (unsigned_pos > list_val->num_values)
            loli_IndexError(s, "Index %ld is too small for list (minimum: %ld)",
                    pos, -(int64_t)list_val->num_values);

        pos = list_val->num_values - unsigned_pos;
    }
    else if (pos > list_val->num_values) {
        loli_IndexError(s, "Index %ld is too large for list (maximum: %ld)",
                pos, (uint64_t)list_val->num_values);
    }

    return pos;
}

void loli_builtin_List_delete_at(loli_state *s)
{
    loli_container_val *list_val = loli_arg_container(s, 0);
    int64_t pos = loli_arg_integer(s, 1);

    if (list_val->num_values == 0)
        loli_IndexError(s, "Cannot delete from an empty list.");

    pos = get_relative_index(s, list_val, pos);

    loli_list_take(s, list_val, pos);
    loli_return_top(s);
}

void loli_builtin_List_each(loli_state *s)
{
    loli_container_val *list_val = loli_arg_container(s, 0);
    loli_call_prepare(s, loli_arg_function(s, 1));
    int i;

    for (i = 0;i < list_val->num_values;i++) {
        loli_push_value(s, loli_con_get(list_val, i));
        loli_call(s, 1);
    }

    loli_return_value(s, loli_arg_value(s, 0));
}

void loli_builtin_List_each_index(loli_state *s)
{
    loli_container_val *list_val = loli_arg_container(s, 0);
    loli_call_prepare(s, loli_arg_function(s, 1));

    int i;
    for (i = 0;i < list_val->num_values;i++) {
        loli_push_integer(s, i);
        loli_call(s, 1);
    }

    loli_return_value(s, loli_arg_value(s, 0));
}

void loli_builtin_List_fold(loli_state *s)
{
    loli_container_val *list_val = loli_arg_container(s, 0);
    loli_value *start = loli_arg_value(s, 1);

    if (list_val->num_values == 0)
        loli_return_value(s, start);
    else {
        loli_call_prepare(s, loli_arg_function(s, 2));
        loli_value *result = loli_call_result(s);
        loli_push_value(s, start);
        int i = 0;
        while (1) {
            loli_push_value(s, loli_con_get(list_val, i));
            loli_call(s, 2);

            if (i == list_val->num_values - 1)
                break;

            loli_push_value(s, result);

            i++;
        }

        loli_return_value(s, result);
    }
}

void loli_builtin_List_fill(loli_state *s)
{
    int64_t stop = loli_arg_integer(s, 0);

    if (stop <= 0) {
        loli_push_list(s, 0);
        loli_return_top(s);
        return;
    }

    loli_call_prepare(s, loli_arg_function(s, 1));
    loli_container_val *con = loli_push_list(s, stop);
    loli_value *result = loli_call_result(s);
    int64_t i;

    for (i = 0;i < stop;i++) {
        loli_push_integer(s, i);
        loli_call(s, 1);
        loli_con_set(con, i, result);
    }

    loli_return_top(s);
}


void loli_builtin_List_get(loli_state *s)
{
    loli_container_val *list_val = loli_arg_container(s, 0);
    int64_t pos = loli_arg_integer(s, 1);

     
    if (pos < 0) {
        uint64_t unsigned_pos = -(int64_t)pos;
        if (unsigned_pos > list_val->num_values)
            pos = -1;
        else
            pos = list_val->num_values - unsigned_pos;
    }

    if (pos >= list_val->num_values)
        pos = -1;

    if (pos != -1) {
        loli_container_val *variant = loli_push_some(s);
        loli_con_set(variant, 0, loli_con_get(list_val, pos));
        loli_return_top(s);
    }
    else
        loli_return_none(s);
}

void loli_builtin_List_insert(loli_state *s)
{
    loli_container_val *list_val = loli_arg_container(s, 0);
    int64_t insert_pos = loli_arg_integer(s, 1);
    loli_value *insert_value = loli_arg_value(s, 2);

    insert_pos = get_relative_index(s, list_val, insert_pos);

    loli_list_insert(list_val, insert_pos, insert_value);
    loli_return_unit(s);
}

void loli_builtin_List_join(loli_state *s)
{
    loli_container_val *lv = loli_arg_container(s, 0);
    const char *delim = "";
    if (loli_arg_count(s) == 2)
        delim = loli_arg_string_raw(s, 1);

    loli_msgbuf *vm_buffer = loli_msgbuf_get(s);

    if (lv->num_values) {
        int i, stop = lv->num_values - 1;
        loli_value **values = lv->values;
        for (i = 0;i < stop;i++) {
            loli_mb_add_value(vm_buffer, s, values[i]);
            loli_mb_add(vm_buffer, delim);
        }
        if (stop != -1)
            loli_mb_add_value(vm_buffer, s, values[i]);
    }

    loli_push_string(s, loli_mb_raw(vm_buffer));
    loli_return_top(s);
}

void loli_builtin_List_map(loli_state *s)
{
    loli_container_val *list_val = loli_arg_container(s, 0);

    loli_call_prepare(s, loli_arg_function(s, 1));
    loli_container_val *con = loli_push_list(s, 0);
    loli_list_reserve(con, list_val->num_values);

    int i;
    for (i = 0;i < list_val->num_values;i++) {
        loli_value *e = list_val->values[i];
        loli_push_value(s, e);
        loli_call(s, 1);
        loli_list_push(con, loli_call_result(s));
    }

    loli_return_top(s);
}

void loli_builtin_List_pop(loli_state *s)
{
    loli_container_val *list_val = loli_arg_container(s, 0);

    if (list_val->num_values == 0)
        loli_IndexError(s, "Pop from an empty list.");

    loli_list_take(s, list_val, loli_con_size(list_val) - 1);
    loli_return_top(s);
}

void loli_builtin_List_push(loli_state *s)
{
    loli_value *list_arg = loli_arg_value(s, 0);
    loli_container_val *list_val = loli_as_container(list_arg);
    loli_value *insert_value = loli_arg_value(s, 1);

    loli_list_insert(list_val, loli_con_size(list_val), insert_value);
    loli_return_value(s, list_arg);
}

static void list_select_reject_common(loli_state *s, int expect)
{
    loli_container_val *list_val = loli_arg_container(s, 0);
    loli_call_prepare(s, loli_arg_function(s, 1));
    loli_value *result = loli_call_result(s);
    loli_container_val *con = loli_push_list(s, 0);

    int i;
    for (i = 0;i < list_val->num_values;i++) {
        loli_push_value(s, list_val->values[i]);
        loli_call(s, 1);

        int ok = loli_as_boolean(result) == expect;

        if (ok)
            loli_list_push(con, list_val->values[i]);
    }

    loli_return_top(s);
}

void loli_builtin_List_reject(loli_state *s)
{
    list_select_reject_common(s, 0);
}

void loli_builtin_List_repeat(loli_state *s)
{
    int n = loli_arg_integer(s, 0);
    if (n < 0)
        loli_ValueError(s, "Repeat count must be >= 0 (%ld given).",
                (int64_t)n);

    loli_value *to_repeat = loli_arg_value(s, 1);
    loli_container_val *lv = loli_push_list(s, n);

    int i;
    for (i = 0;i < n;i++)
        loli_con_set(lv, i, to_repeat);

    loli_return_top(s);
}

void loli_builtin_List_select(loli_state *s)
{
    list_select_reject_common(s, 1);
}

void loli_builtin_List_size(loli_state *s)
{
    loli_container_val *list_val = loli_arg_container(s, 0);

    loli_return_integer(s, list_val->num_values);
}

void loli_builtin_List_shift(loli_state *s)
{
    loli_container_val *list_val = loli_arg_container(s, 0);

    if (loli_con_size(list_val) == 0)
        loli_IndexError(s, "Shift on an empty list.");

    loli_list_take(s, list_val, 0);
    loli_return_top(s);
    return;
}

void loli_builtin_List_slice(loli_state *s)
{
    loli_container_val *lv = loli_arg_container(s, 0);
    int start = 0;
    int size = loli_con_size(lv);
    int stop = size;

    switch (loli_arg_count(s)) {
        case 3: stop = loli_arg_integer(s, 2);
        case 2: start = loli_arg_integer(s, 1);
    }

    if (stop < 0)
        stop = size + stop;
    if (start < 0)
        start = size + start;

    if (stop > size ||
        start > size ||
        start > stop) {
        loli_IndexError(s, "List index out of range");
        return;
    }

    int new_size = (stop - start);
    loli_container_val *new_lv = loli_push_list(s, new_size);
    int i, j;

    for (i = 0, j = start;i < new_size;i++, j++) {
        loli_con_set(new_lv, i, loli_con_get(lv, j));
    }

    loli_return_top(s);
}

void loli_builtin_List_unshift(loli_state *s)
{
    loli_container_val *list_val = loli_arg_container(s, 0);
    loli_value *input_reg = loli_arg_value(s, 1);

    loli_list_insert(list_val, 0, input_reg);
}

void loli_builtin_List_zip(loli_state *s)
{
    loli_container_val *list_val = loli_arg_container(s, 0);
    loli_container_val *all_others = loli_arg_container(s, 1);
    int other_list_count = loli_con_size(all_others);
    int result_size = loli_con_size(list_val);
    int row_i, column_i;

     
    for (row_i = 0;row_i < other_list_count;row_i++) {
        loli_value *other_value = loli_con_get(all_others, row_i);
        loli_container_val *other_elem = loli_as_container(other_value);
        int elem_size = loli_con_size(other_elem);

        if (result_size > elem_size)
            result_size = elem_size;
    }

    loli_container_val *result_list = loli_push_list(s, result_size);
    int result_width = other_list_count + 1;

    for (row_i = 0;row_i < result_size;row_i++) {
         
        loli_container_val *tup = loli_push_tuple(s, result_width);

        loli_con_set(tup, 0, loli_con_get(list_val, row_i));

        for (column_i = 0;column_i < other_list_count;column_i++) {
             
            loli_value *other_value = loli_con_get(all_others, column_i);
            loli_container_val *other_elem = loli_as_container(other_value);
            loli_con_set(tup, column_i + 1, loli_con_get(other_elem, row_i));
        }

        loli_con_set_from_stack(s, result_list, row_i);
    }

    loli_return_top(s);
}


void loli_builtin_Option_and(loli_state *s)
{
    if (loli_arg_is_some(s, 0))
        loli_return_value(s, loli_arg_value(s, 1));
    else
        loli_return_value(s, loli_arg_value(s, 0));
}

void loli_builtin_Option_and_then(loli_state *s)
{
    if (loli_arg_is_some(s, 0)) {
        loli_container_val *con = loli_arg_container(s, 0);
        loli_call_prepare(s, loli_arg_function(s, 1));
        loli_push_value(s, loli_con_get(con, 0));
        loli_call(s, 1);
        loli_return_value(s, loli_call_result(s));
    }
    else
        loli_return_none(s);
}

void loli_builtin_Option_is_none(loli_state *s)
{
    loli_return_boolean(s, loli_arg_is_some(s, 0) == 0);
}

void loli_builtin_Option_is_some(loli_state *s)
{
    loli_return_boolean(s, loli_arg_is_some(s, 0));
}

void loli_builtin_Option_map(loli_state *s)
{
    if (loli_arg_is_some(s, 0)) {
        loli_container_val *con = loli_arg_container(s, 0);
        loli_call_prepare(s, loli_arg_function(s, 1));
        loli_push_value(s, loli_con_get(con, 0));
        loli_call(s, 1);

        loli_container_val *variant = loli_push_some(s);
        loli_con_set(variant, 0, loli_call_result(s));
        loli_return_top(s);
    }
    else
        loli_return_none(s);
}

void loli_builtin_Option_or(loli_state *s)
{
    if (loli_arg_is_some(s, 0))
        loli_return_value(s, loli_arg_value(s, 0));
    else
        loli_return_value(s, loli_arg_value(s, 1));
}

void loli_builtin_Option_or_else(loli_state *s)
{
    if (loli_arg_is_some(s, 0))
        loli_return_value(s, loli_arg_value(s, 0));
    else {
        loli_call_prepare(s, loli_arg_function(s, 1));
        loli_call(s, 0);

        loli_return_value(s, loli_call_result(s));
    }
}

void loli_builtin_Option_unwrap(loli_state *s)
{
    if (loli_arg_is_some(s, 0)) {
        loli_container_val *con = loli_arg_container(s, 0);
        loli_return_value(s, loli_con_get(con, 0));
    }
    else
        loli_ValueError(s, "unwrap called on None.");
}

void loli_builtin_Option_unwrap_or(loli_state *s)
{
    loli_value *source;

    if (loli_arg_is_some(s, 0)) {
        loli_container_val *con = loli_arg_container(s, 0);
        source = loli_con_get(con, 0);
    }
    else
        source = loli_arg_value(s, 1);

    loli_return_value(s, source);
}

void loli_builtin_Option_unwrap_or_else(loli_state *s)
{
    if (loli_arg_is_some(s, 0)) {
        loli_container_val *con = loli_arg_container(s, 0);
        loli_return_value(s, loli_con_get(con, 0));
    }
    else {
        loli_call_prepare(s, loli_arg_function(s, 1));
        loli_call(s, 0);

        loli_return_value(s, loli_call_result(s));
    }
}

static void result_optionize(loli_state *s, int expect)
{
    if (loli_arg_is_success(s, 0) == expect) {
        loli_container_val *con = loli_arg_container(s, 0);

        loli_container_val *variant = loli_push_some(s);
        loli_con_set(variant, 0, loli_con_get(con, 0));
        loli_return_top(s);
    }
    else
        loli_return_none(s);
}

void loli_builtin_Result_failure(loli_state *s)
{
    result_optionize(s, 0);
}

static void result_is_success_or_failure(loli_state *s, int expect)
{
    loli_return_boolean(s, loli_arg_is_success(s, 0) == expect);
}

void loli_builtin_Result_is_failure(loli_state *s)
{
    result_is_success_or_failure(s, 0);
}

void loli_builtin_Result_is_success(loli_state *s)
{
    result_is_success_or_failure(s, 1);
}

void loli_builtin_Result_success(loli_state *s)
{
    result_optionize(s, 1);
}

void loli_builtin_RuntimeError_new(loli_state *s)
{
    return_exception(s, LOLI_ID_RUNTIMEERROR);
}


static int char_index(const char *s, int idx, char ch)
{
    const char *P = strchr(s + idx,ch);
    if (P == NULL)
        return -1;
    else
        return (int)((uintptr_t)P - (uintptr_t)s);
}

void loli_builtin_String_format(loli_state *s)
{
    const char *fmt = loli_arg_string_raw(s, 0);
    loli_container_val *lv = loli_arg_container(s, 1);

    int lsize = loli_con_size(lv);
    loli_msgbuf *msgbuf = loli_msgbuf_get(s);

    int idx, last_idx = 0;

    while (1) {
        idx = char_index(fmt, last_idx, '{');
        if (idx > -1) {
            if (idx > last_idx)
                loli_mb_add_slice(msgbuf, fmt, last_idx, idx);

            char ch;
            int i, total = 0;
            int start = idx + 1;

             
            do {
                idx++;
                ch = fmt[idx];
            } while (ch == '0');

            for (i = 0;i < 2;i++) {
                if (isdigit(ch) == 0)
                    break;

                total = (total * 10) + (ch - '0');
                idx++;
                ch = fmt[idx];
            }

            if (isdigit(ch))
                loli_ValueError(s, "Format must be between 0...99.");
            else if (start == idx) {
                if (ch == '}' || ch == '\0')
                    loli_ValueError(s, "Format specifier is empty.");
                else
                    loli_ValueError(s, "Format specifier is not numeric.");
            }
            else if (total >= lsize)
                loli_IndexError(s, "Format specifier is too large.");

            idx++;
            last_idx = idx;

            loli_value *v = loli_con_get(lv, total);
            loli_mb_add_value(msgbuf, s, v);
        }
        else {
            loli_mb_add_slice(msgbuf, fmt, last_idx, strlen(fmt));
            break;
        }
    }

    loli_push_string(s, loli_mb_raw(msgbuf));
    loli_return_top(s);
}

void loli_builtin_String_len(loli_state *s)
{
    loli_value *input_arg = loli_arg_value(s, 0);
    int input_size = input_arg->value.string->size;
    loli_return_integer(s, input_size);
}

void loli_builtin_String_char_at(loli_state *s)
{
    loli_value *input_arg = loli_arg_value(s, 0);
    loli_value *pos_arg = loli_arg_value(s, 1);
    
    char *input_raw_str = input_arg->value.string->string;
    char e;
        
    int pos_raw_arg = pos_arg->value.integer;
    int input_size = input_arg->value.string->size;
  
    if(pos_raw_arg > input_size || pos_raw_arg == input_size){
        loli_IndexError(s, "String index out of range");
        loli_return_byte(s, 0);
    } else if(pos_raw_arg < 0){
        if(pos_raw_arg+input_size < 0 || pos_raw_arg+input_size > input_size){
            loli_IndexError(s, "String index out of range");
            loli_return_byte(s, 0);
            return;      
        }
        e = input_raw_str[pos_raw_arg+input_size];
        loli_return_byte(s, (uint8_t)e);
    } else {
        e = input_raw_str[pos_raw_arg];
        loli_return_byte(s, (uint8_t)e);
    }
}

void loli_builtin_String_ends_with(loli_state *s)
{
    loli_value *input_arg = loli_arg_value(s, 0);
    loli_value *suffix_arg = loli_arg_value(s, 1);

    char *input_raw_str = input_arg->value.string->string;
    char *suffix_raw_str = suffix_arg->value.string->string;
    int input_size = input_arg->value.string->size;
    int suffix_size = suffix_arg->value.string->size;

    if (suffix_size > input_size) {
        loli_return_boolean(s, 0);
        return;
    }

    int input_i, suffix_i, ok = 1;
    for (input_i = input_size - 1, suffix_i = suffix_size - 1;
         suffix_i >= 0;
         input_i--, suffix_i--) {
        if (input_raw_str[input_i] != suffix_raw_str[suffix_i]) {
            ok = 0;
            break;
        }
    }

    loli_return_boolean(s, ok);
}


void loli_builtin_String_find(loli_state *s)
{
    loli_value *input_arg = loli_arg_value(s, 0);
    loli_value *find_arg = loli_arg_value(s, 1);
    int start = 0;
    if (loli_arg_count(s) == 3)
        start = loli_arg_integer(s, 2);

    char *input_str = input_arg->value.string->string;
    int input_length = input_arg->value.string->size;

    char *find_str = find_arg->value.string->string;
    int find_length = find_arg->value.string->size;

    if (find_length > input_length ||
        find_length == 0 ||
        start > input_length ||
        follower_table[(unsigned char)input_str[start]] == -1) {
        loli_return_none(s);
        return;
    }

    char find_ch;
    int i, j, k, length_diff, match;

    length_diff = input_length - find_length;
    find_ch = find_str[0];
    match = 0;

     
    for (i = start;i <= length_diff;i++) {
        if (input_str[i] == find_ch) {
            match = 1;
             
            for (j = i + 1, k = 1;k < find_length;j++, k++) {
                if (input_str[j] != find_str[k]) {
                    match = 0;
                    break;
                }
            }
            if (match == 1)
                break;
        }
    }

    if (match) {
        loli_container_val *variant = loli_push_some(s);

        loli_push_integer(s, i);
        loli_con_set_from_stack(s, variant, 0);

        loli_return_top(s);
    }
    else
        loli_return_none(s);
}

#define CTYPE_WRAP(WRAP_NAME, WRAPPED_CALL) \
void loli_builtin_String_##WRAP_NAME(loli_state *s) \
{ \
    loli_string_val *input = loli_arg_string(s, 0); \
    int length = loli_string_length(input); \
\
    if (length == 0) { \
        loli_return_boolean(s, 0); \
        return; \
    } \
\
    const char *loop_str = loli_string_raw(input); \
    int i = 0; \
    int ok = 1; \
\
    for (i = 0;i < length;i++) { \
        if (WRAPPED_CALL(loop_str[i]) == 0) { \
            ok = 0; \
            break; \
        } \
    } \
\
    loli_return_boolean(s, ok); \
}

CTYPE_WRAP(is_alnum, isalnum)

CTYPE_WRAP(is_alpha, isalpha)

CTYPE_WRAP(is_digit, isdigit)

CTYPE_WRAP(is_space, isspace)

void loli_builtin_String_lower(loli_state *s)
{
    loli_value *input_arg = loli_arg_value(s, 0);
    int input_length = input_arg->value.string->size;
    int i;

    loli_push_string(s, loli_as_string_raw(input_arg));
    char *raw_out = loli_as_string_raw(loli_stack_get_top(s));

    for (i = 0;i < input_length;i++) {
        char ch = raw_out[i];
        if (isupper(ch))
            raw_out[i] = tolower(ch);
    }

    loli_return_top(s);
}

static int lstrip_utf8_start(loli_value *input_arg, loli_string_val *strip_sv)
{
    char *input_str = input_arg->value.string->string;
    int input_length = input_arg->value.string->size;

    char *strip_str = strip_sv->string;
    int strip_length = strip_sv->size;
    int i = 0, j = 0, match = 1;

    char ch = strip_str[0];
    if (follower_table[(unsigned char)ch] == strip_length) {
         
        char strip_start_ch = ch;
        int char_width = follower_table[(unsigned char)ch];
        while (i < input_length) {
            if (input_str[i] == strip_start_ch) {
                 
                for (j = 1;j < char_width;j++) {
                    if (input_str[i + j] != strip_str[j]) {
                        match = 0;
                        break;
                    }
                }
                if (match == 0)
                    break;

                i += char_width;
            }
            else
                break;
        }
    }
    else {
         
        char input_ch;
        int char_width, k;
        while (1) {
            input_ch = input_str[i];
            if (input_ch == strip_str[j]) {
                char_width = follower_table[(unsigned char)strip_str[j]];
                match = 1;
                 
                for (k = 1;k < char_width;k++) {
                    if (input_str[i + k] != strip_str[j + k]) {
                        match = 0;
                        break;
                    }
                }
                if (match == 1) {
                     
                    i += char_width;
                    if (i >= input_length)
                        break;
                    else {
                        j = 0;
                        continue;
                    }
                }
            }

             
            j += follower_table[(unsigned char)strip_str[j]];

             
            if (j == strip_length) {
                match = 0;
                break;
            }
        }
    }

    return i;
}

static int lstrip_ascii_start(loli_value *input_arg, loli_string_val *strip_sv)
{
    int i;
    char *input_str = input_arg->value.string->string;
    int input_length = input_arg->value.string->size;

    if (strip_sv->size == 1) {
         
        char strip_ch;
        strip_ch = strip_sv->string[0];
        for (i = 0;i < input_length;i++) {
            if (input_str[i] != strip_ch)
                break;
        }
    }
    else {
         
        char *strip_str = strip_sv->string;
        int strip_length = strip_sv->size;
        for (i = 0;i < input_length;i++) {
            char ch = input_str[i];
            int found = 0;
            int j;
            for (j = 0;j < strip_length;j++) {
                if (ch == strip_str[j]) {
                    found = 1;
                    break;
                }
            }
            if (found == 0)
                break;
        }
    }

    return i;
}

void loli_builtin_String_lstrip(loli_state *s)
{
    loli_value *input_arg = loli_arg_value(s, 0);
    loli_value *strip_arg = loli_arg_value(s, 1);

    char *strip_str;
    unsigned char ch;
    int copy_from, i, has_multibyte_char;
    size_t strip_str_len;
    loli_string_val *strip_sv;

     
    if (input_arg->value.string->size == 0 ||
        strip_arg->value.string->size == 0) {
        loli_return_value(s, input_arg);
        return;
    }

    strip_sv = strip_arg->value.string;
    strip_str = strip_sv->string;
    strip_str_len = strlen(strip_str);
    has_multibyte_char = 0;

    for (i = 0;i < strip_str_len;i++) {
        ch = (unsigned char)strip_str[i];
        if (ch > 127) {
            has_multibyte_char = 1;
            break;
        }
    }

    if (has_multibyte_char == 0)
        copy_from = lstrip_ascii_start(input_arg, strip_sv);
    else
        copy_from = lstrip_utf8_start(input_arg, strip_sv);

    const char *raw = input_arg->value.string->string + copy_from;
    int size = input_arg->value.string->size;

    loli_push_string_sized(s, raw, size - copy_from);
    loli_return_top(s);
}

void loli_builtin_String_to_d(loli_state *s)
{
    double number;
    int exponent;
    int negative;
    char *p = loli_arg_string_raw(s, 0);
    double p10;
    int n;
    int num_digits;
    int num_decimals;

    while (isspace(*p)) p++;

    negative = 0;
    switch (*p) {
      case '-': negative = 1;
      case '+': p++;
    }

    number = 0.;
    exponent = 0;
    num_digits = 0;
    num_decimals = 0;

    while (isdigit(*p)) {
        number = number * 10. + (*p - '0');
        p++;
        num_digits++;
    }

    if (*p == '.') {
        p++;

        while (isdigit(*p)) {
            number = number * 10. + (*p - '0');
            p++;
            num_digits++;
            num_decimals++;
        }

        exponent -= num_decimals;
    }

    if (num_digits == 0) {
        loli_ValueError(s, "Invalid Double literal: '%s'", p);
        return;
    }

    if (negative) number = -number;

    if (*p == 'e' || *p == 'E') {
        negative = 0;
        switch (*++p) {
            case '-': negative = 1;   // Fall through to increment pos
            case '+': p++;
        }

        n = 0;
        while (isdigit(*p)) {
            n = n * 10 + (*p - '0');
            p++;
        }

        if (negative) {
            exponent -= n;
        } else {
            exponent += n;
        }
    }

    if (exponent < DBL_MIN_EXP  || exponent > DBL_MAX_EXP) {
        loli_ValueError(s, "Invalid Double literal: '%s'", p);
        return;
    }

    p10 = 10.;
    n = exponent;
    if (n < 0) n = -n;
    while (n) {
        if (n & 1) {
            if (exponent < 0) {
                number /= p10;
            } else {
                number *= p10;
            }
        }
        n >>= 1;
        p10 *= p10;
    }

    if (number == HUGE_VAL) {
        loli_ValueError(s, "Invalid Double literal: '%s'", p);
        return;
    }
    
    if (*p != '\0') {
        loli_ValueError(s, "Invalid Double literal: '%s'", p);
        return;        
    }

    loli_return_double(s, number);
}

void loli_builtin_String_to_i(loli_state *s)
{
    char *input = loli_arg_string_raw(s, 0);
    uint64_t value = 0;
    int is_negative = 0;
    unsigned int rounds = 0;
    int leading_zeroes = 0;

    if (*input == '-') {
        is_negative = 1;
        ++input;
    }
    else if (*input == '+')
        ++input;

    if (*input == '0') {
        ++input;
        leading_zeroes = 1;
        while (*input == '0')
            ++input;
    }

     
    while (*input >= '0' && *input <= '9' && rounds != 20) {
        value = (value * 10) + (*input - '0');
        ++input;
        rounds++;
    }

     
    if (value > ((uint64_t)INT64_MAX + is_negative) ||
        *input != '\0' ||
        (rounds == 0 && leading_zeroes == 0)) {
        loli_ValueError(s, "Invalid Integer literal: '%s'", input);
    }
    else {
        int64_t signed_value;

        if (is_negative == 0)
            signed_value = (int64_t)value;
        else
            signed_value = -(int64_t)value;

        loli_push_integer(s, signed_value);

        loli_return_top(s);
    }
}

void loli_builtin_String_replace(loli_state *s)
{
    loli_string_val *source_sv = loli_arg_string(s, 0);
    loli_string_val *needle_sv = loli_arg_string(s, 1);
    int source_len = loli_string_length(source_sv);
    int needle_len = loli_string_length(needle_sv);

    if (needle_len > source_len) {
        loli_return_value(s, loli_arg_value(s, 0));
        return;
    }

    loli_msgbuf *msgbuf = loli_msgbuf_get(s);
    char *source = loli_string_raw(source_sv);
    char *needle = loli_string_raw(needle_sv);
    char *replace_with = loli_arg_string_raw(s, 2);
    char needle_first = *needle;
    char ch;
    int start = 0;
    int i;

    for (i = 0;i < source_len;i++) {
        ch = source[i];
        if (ch == needle_first &&
            (i + needle_len) <= source_len) {
            int match = 1;
            int j;
            for (j = 1;j < needle_len;j++) {
                if (needle[j] != source[i + j])
                    match = 0;
            }

            if (match) {
                if (i != start)
                    loli_mb_add_slice(msgbuf, source, start, i);

                loli_mb_add(msgbuf, replace_with);
                i += needle_len - 1;
                start = i + 1;
            }
        }
    }

    if (i != start)
        loli_mb_add_slice(msgbuf, source, start, i);

    loli_push_string(s, loli_mb_raw(msgbuf));
    loli_return_top(s);
}

static int rstrip_ascii_stop(loli_value *input_arg, loli_string_val *strip_sv)
{
    int i;
    char *input_str = input_arg->value.string->string;
    int input_length = input_arg->value.string->size;

    if (strip_sv->size == 1) {
        char strip_ch = strip_sv->string[0];
        for (i = input_length - 1;i >= 0;i--) {
            if (input_str[i] != strip_ch)
                break;
        }
    }
    else {
        char *strip_str = strip_sv->string;
        int strip_length = strip_sv->size;
        for (i = input_length - 1;i >= 0;i--) {
            char ch = input_str[i];
            int found = 0;
            int j;
            for (j = 0;j < strip_length;j++) {
                if (ch == strip_str[j]) {
                    found = 1;
                    break;
                }
            }
            if (found == 0)
                break;
        }
    }

    return i + 1;
}

static int rstrip_utf8_stop(loli_value *input_arg, loli_string_val *strip_sv)
{
    char *input_str = input_arg->value.string->string;
    int input_length = input_arg->value.string->size;

    char *strip_str = strip_sv->string;
    int strip_length = strip_sv->size;
    int i, j;

    i = input_length - 1;
    j = 0;
    while (i >= 0) {
         
        int follow_count = follower_table[(unsigned char)strip_str[j]];
         
        char last_strip_byte = strip_str[j + (follow_count - 1)];
         
        if (input_str[i] == last_strip_byte &&
            i + 1 >= follow_count) {
            int match = 1;
            int input_i, strip_i, k;
             
            for (input_i = i - 1, strip_i = j + (follow_count - 2), k = 1;
                 k < follow_count;
                 input_i--, strip_i--, k++) {
                if (input_str[input_i] != strip_str[strip_i]) {
                    match = 0;
                    break;
                }
            }

            if (match == 1) {
                i -= follow_count;
                j = 0;
                continue;
            }
        }

         
        j += follow_count;
        if (j == strip_length)
            break;

        continue;
    }

    return i + 1;
}

void loli_builtin_String_rstrip(loli_state *s)
{
    loli_value *input_arg = loli_arg_value(s, 0);
    loli_value *strip_arg = loli_arg_value(s, 1);

    char *strip_str;
    unsigned char ch;
    int copy_to, i, has_multibyte_char;
    size_t strip_str_len;
    loli_string_val *strip_sv;

     
    if (input_arg->value.string->size == 0 ||
        strip_arg->value.string->size == 0) {
        loli_return_value(s, input_arg);
        return;
    }

    strip_sv = strip_arg->value.string;
    strip_str = strip_sv->string;
    strip_str_len = strlen(strip_str);
    has_multibyte_char = 0;

    for (i = 0;i < strip_str_len;i++) {
        ch = (unsigned char)strip_str[i];
        if (ch > 127) {
            has_multibyte_char = 1;
            break;
        }
    }

    if (has_multibyte_char == 0)
        copy_to = rstrip_ascii_stop(input_arg, strip_sv);
    else
        copy_to = rstrip_utf8_stop(input_arg, strip_sv);

    const char *raw = input_arg->value.string->string;

    loli_push_string_sized(s, raw, copy_to);
    loli_return_top(s);
}

static const char move_table[256] =
{
      
  0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static void string_split_by_val(loli_state *s, char *input, char *splitby)
{
    char *input_ch = &input[0];
    char *splitby_ch = &splitby[0];
    int values_needed = 0;

    while (move_table[(unsigned char)*input_ch] != 0) {
        if (*input_ch == *splitby_ch) {
            char *restore_ch = input_ch;
            int is_match = 1;
            while (*input_ch == *splitby_ch) {
                splitby_ch++;
                input_ch++;
                if (*splitby_ch == '\0')
                    break;

                if (*input_ch != *splitby_ch) {
                    is_match = 0;
                    input_ch = restore_ch + 1;
                    break;
                }
            }

            splitby_ch = &splitby[0];
            values_needed += is_match;
        }
        else
            input_ch += move_table[(unsigned char)*input_ch];
    }

    values_needed++;
    input_ch = &input[0];
    loli_container_val *list_val = loli_push_list(s, values_needed);
    int i = 0;
    char *last_start = input_ch;

    while (1) {
        char *match_start = input_ch;
        int is_match = 0;
        if (*input_ch == *splitby_ch) {
            is_match = 1;
            while (*input_ch == *splitby_ch) {
                splitby_ch++;
                if (*splitby_ch == '\0')
                    break;

                input_ch++;
                if (*input_ch != *splitby_ch) {
                    is_match = 0;
                    input_ch = match_start;
                    break;
                }
            }
            splitby_ch = &splitby[0];
        }

         
        if (is_match || *input_ch == '\0') {
            int size = match_start - last_start;
            loli_push_string_sized(s, last_start, size);
            loli_con_set_from_stack(s, list_val, i);

            i++;
            if (*input_ch == '\0')
                break;

            last_start = input_ch + 1;
        }

        input_ch++;
    }
}

void loli_builtin_String_slice(loli_state *s)
{
    do_str_slice(s, 0);
}

void loli_builtin_String_split(loli_state *s)
{
    loli_string_val *input_strval = loli_arg_string(s, 0);
    loli_string_val *split_strval;
    loli_string_val fake_sv;

    if (loli_arg_count(s) == 2) {
        split_strval = loli_arg_string(s, 1);
        if (split_strval->size == 0)
            loli_ValueError(s, "Cannot split by empty string.");
    }
    else {
        fake_sv.string = " ";
        fake_sv.size = 1;
        split_strval = &fake_sv;
    }

    string_split_by_val(s, input_strval->string, split_strval->string);
    loli_return_top(s);
}

void loli_builtin_String_starts_with(loli_state *s)
{
    loli_value *input_arg = loli_arg_value(s, 0);
    loli_value *prefix_arg = loli_arg_value(s, 1);

    char *input_raw_str = input_arg->value.string->string;
    char *prefix_raw_str = prefix_arg->value.string->string;
    int prefix_size = prefix_arg->value.string->size;

    if (input_arg->value.string->size < prefix_size) {
        loli_return_boolean(s, 0);
        return;
    }

    int i, ok = 1;
    for (i = 0;i < prefix_size;i++) {
        if (input_raw_str[i] != prefix_raw_str[i]) {
            ok = 0;
            break;
        }
    }

    loli_return_boolean(s, ok);
}

void loli_builtin_String_strip(loli_state *s)
{
    loli_value *input_arg = loli_arg_value(s, 0);
    loli_value *strip_arg = loli_arg_value(s, 1);

     
    if (input_arg->value.string->size == 0 ||
        strip_arg->value.string->size == 0) {
        loli_return_value(s, input_arg);
        return;
    }

    unsigned char ch;
    loli_string_val *strip_sv = strip_arg->value.string;
    char *strip_str = strip_sv->string;
    size_t strip_str_len = strlen(strip_str);
    int has_multibyte_char = 0;
    int copy_from, copy_to, i;

    for (i = 0;i < strip_str_len;i++) {
        ch = (unsigned char)strip_str[i];
        if (ch > 127) {
            has_multibyte_char = 1;
            break;
        }
    }

    if (has_multibyte_char == 0)
        copy_from = lstrip_ascii_start(input_arg, strip_sv);
    else
        copy_from = lstrip_utf8_start(input_arg, strip_sv);

    if (copy_from != input_arg->value.string->size) {
        if (has_multibyte_char)
            copy_to = rstrip_ascii_stop(input_arg, strip_sv);
        else
            copy_to = rstrip_utf8_stop(input_arg, strip_sv);
    }
    else
         
        copy_to = copy_from;

    const char *raw = input_arg->value.string->string + copy_from;
    loli_push_string_sized(s, raw, copy_to - copy_from);
    loli_return_top(s);
}

void loli_builtin_String_to_bytestring(loli_state *s)
{
     
    loli_string_val *sv = loli_arg_string(s, 0);
    loli_push_bytestring(s, loli_string_raw(sv), loli_string_length(sv));
    loli_return_top(s);
}

void loli_builtin_String_trim(loli_state *s)
{
    loli_value *input_arg = loli_arg_value(s, 0);

    char fake_buffer[5] = " \t\r\n";
    loli_string_val fake_sv;
    fake_sv.string = fake_buffer;
    fake_sv.size = strlen(fake_buffer);

    int copy_from = lstrip_ascii_start(input_arg, &fake_sv);

    if (copy_from != input_arg->value.string->size) {
        int copy_to = rstrip_ascii_stop(input_arg, &fake_sv);
        const char *raw = input_arg->value.string->string;
        loli_push_string_sized(s, raw + copy_from, copy_to - copy_from);
    }
    else {
         
        loli_push_string(s, "");
    }

    loli_return_top(s);
}

void loli_builtin_String_upper(loli_state *s)
{
    loli_value *input_arg = loli_arg_value(s, 0);
    int input_length = input_arg->value.string->size;
    int i;

    loli_push_string(s, loli_as_string_raw(input_arg));
    char *raw_out = loli_as_string_raw(loli_stack_get_top(s));

    for (i = 0;i < input_length;i++) {
        char ch = raw_out[i];
        if (islower(ch))
            raw_out[i] = toupper(ch);
    }

    loli_return_top(s);
}


void loli_builtin_ValueError_new(loli_state *s)
{
    return_exception(s, LOLI_ID_VALUEERROR);
}

static void new_builtin_file(loli_state *s, FILE *source, const char *mode)
{
    loli_push_file(s, source, mode);
    loli_file_val *file_val = loli_as_file(loli_stack_get_top(s));
    file_val->is_builtin = 1;
}

void loli_builtin_input(loli_state *s)
{
    if(loli_arg_count(s) >= 1)
        loli_builtin__say(s);
    
    char input_buffer[256];    
    
    fgets(input_buffer, 256, stdin);
    
    size_t input_size = strlen(input_buffer);
    if (*input_buffer && input_buffer[input_size - 1] == '\n') 
        input_buffer[input_size - 1] = '\0';
    input_size -= 1;
    
    loli_push_bytestring(s, input_buffer, input_size);
    loli_return_top(s);
}

void loli_builtin_range(loli_state *s) 
{
    int64_t start;
    int64_t end;
    if(loli_arg_count(s) == 2) {
        start = loli_arg_integer(s, 0);
        end = loli_arg_integer(s, 1);
    } else {
        start = 0;
        end = loli_arg_integer(s, 0);
    }

    if(end <= 0) {
        loli_ValueError(s, "End should be positive non-zero integer");
        return;
    }
    
    if(start > end) {
        loli_ValueError(s, "Start should not be bigger than end");
        return;
    }

    loli_container_val *con = loli_push_list(s, start == 0 ? end+1 : end);
    int64_t i = start;
    int64_t c = 0;
    
    while (i <= end) {
        loli_push_integer(s, i);
        loli_con_set_from_stack(s, con, c);
        i++;
        c++;
    }
    
    loli_return_top(s);
}

void loli_builtin_var_stdin(loli_state *s)
{
    new_builtin_file(s, stdin, "r");
}

void loli_builtin_var_stdout(loli_state *s)
{
    new_builtin_file(s, stdout, "w");
}

void loli_builtin_var_stderr(loli_state *s)
{
    new_builtin_file(s, stderr, "w");
}

static loli_class *build_class(loli_symtab *symtab, const char *name,
        int generic_count, int dyna_start)
{
    loli_class *result = loli_new_class(symtab, name);
    result->dyna_start = dyna_start;
    result->generic_count = generic_count;
    result->flags |= CLS_IS_BUILTIN;

    return result;
}

static loli_class *build_special(loli_symtab *symtab, const char *name,
        int generic_count, int id)
{
    loli_class *result = loli_new_class(symtab, name);
    result->id = id;
    result->generic_count = generic_count;
    result->flags |= CLS_IS_BUILTIN;

    symtab->active_module->class_chain = result->next;
    symtab->next_class_id--;

    result->next = symtab->old_class_chain;
    symtab->old_class_chain = result;

    return result;
}

void loli_init_pkg_builtin(loli_symtab *symtab)
{
    symtab->integer_class    = build_class(symtab, "Integer",     0, Integer_OFFSET);
    symtab->double_class     = build_class(symtab, "Double",      0, Double_OFFSET);
    symtab->string_class     = build_class(symtab, "String",      0, String_OFFSET);
    symtab->byte_class       = build_class(symtab, "Byte",        0, Byte_OFFSET);
    symtab->bytestring_class = build_class(symtab, "ByteString",  0, ByteString_OFFSET);
    symtab->boolean_class    = build_class(symtab, "Boolean",     0, Boolean_OFFSET);
    symtab->function_class   = build_class(symtab, "Function",   -1, Function_OFFSET);
    symtab->list_class       = build_class(symtab, "List",        1, List_OFFSET);
    symtab->hash_class       = build_class(symtab, "Hash",        2, Hash_OFFSET);
    symtab->tuple_class      = build_class(symtab, "Tuple",      -1, Tuple_OFFSET);
                               build_class(symtab, "File",        0, File_OFFSET);
    loli_class *co_class     = build_class(symtab, "Coroutine",   2, Coroutine_OFFSET);

     
    co_class->id = LOLI_ID_COROUTINE;

    symtab->optarg_class   = build_special(symtab, "*", 1, LOLI_ID_OPTARG);
    loli_class *scoop      = build_special(symtab, "$1", 0, LOLI_ID_SCOOP);

    scoop->self_type->flags |= TYPE_HAS_SCOOP;

    symtab->integer_class->flags |= CLS_VALID_HASH_KEY;
    symtab->string_class->flags  |= CLS_VALID_HASH_KEY;

     
    symtab->function_class->flags |= CLS_GC_TAGGED;
     
    symtab->next_class_id = START_CLASS_ID;
}
