#ifndef LOLI_API_VALUE_FLAGS_H
# define LOLI_API_VALUE_FLAGS_H


#define VAL_IS_GC_TAGGED        0x0010000
#define VAL_IS_GC_SPECULATIVE   0x0020000
#define VAL_HAS_SWEEP_FLAG      (VAL_IS_GC_TAGGED | VAL_IS_GC_SPECULATIVE)
#define VAL_IS_DEREFABLE        0x0040000

#define V_INTEGER_FLAG          0x0100000
#define V_DOUBLE_FLAG           0x0200000
#define V_STRING_FLAG           0x0400000
#define V_BYTE_FLAG             0x0800000
#define V_BYTESTRING_FLAG       0x1000000
#define V_FUNCTION_FLAG	        0x2000000

#define V_INTEGER_BASE          1
#define V_DOUBLE_BASE           2
#define V_STRING_BASE           3
#define V_BYTE_BASE             4
#define V_BYTESTRING_BASE       5
#define V_BOOLEAN_BASE          6
#define V_FUNCTION_BASE         7
#define V_LIST_BASE             8
#define V_HASH_BASE             9
#define V_TUPLE_BASE            10
#define V_FILE_BASE             11
#define V_COROUTINE_BASE        12
#define V_FOREIGN_BASE          13
#define V_INSTANCE_BASE         14
#define V_UNIT_BASE             15
#define V_VARIANT_BASE          16
#define V_EMPTY_VARIANT_BASE    17

#define FLAGS_TO_BASE(x)        (x->flags & 31)

#define VAL_FROM_CLS_GC_SHIFT   8

#endif
