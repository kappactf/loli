#ifndef LOLI_INT_OPCODE_H
# define LOLI_INT_OPCODE_H

typedef enum {
     
    o_assign,

     
    o_assign_noref,

     
    o_int_add,
    o_int_minus,
    o_int_modulo,
    o_int_multiply,
    o_int_divide,
    o_int_left_shift,
    o_int_right_shift,
    o_int_bitwise_and,
    o_int_bitwise_or,
    o_int_bitwise_xor,

     
    o_number_add,
    o_number_minus,
    o_number_multiply,
    o_number_divide,

     
    o_compare_eq,
    o_compare_not_eq,
    o_compare_greater,
    o_compare_greater_eq,

     
    o_unary_not,
    o_unary_minus,
    o_unary_bitwise_not,

     
    o_jump,
     
    o_jump_if,
     
    o_jump_if_not_class,
     
    o_jump_if_set,

     
    o_for_integer,
     
    o_for_setup,

     
    o_call_foreign,
     
    o_call_native,
     
    o_call_register,

     
    o_return_value,
     
    o_return_unit,

     
    o_build_list,
     
    o_build_tuple,
     
    o_build_hash,
     
    o_build_variant,

     
    o_subscript_get,
     
    o_subscript_set,

     
    o_global_get,
     
    o_global_set,

     
    o_load_readonly,
     
    o_load_integer,
     
    o_load_boolean,
     
    o_load_byte,
     
    o_load_empty_variant,

     
    o_instance_new,
     
    o_property_get,
     
    o_property_set,

     
    o_catch_push,
     
    o_catch_pop,
     
    o_exception_catch,
     
    o_exception_store,
     
    o_exception_raise,

     
    o_closure_get,
     
    o_closure_set,
     
    o_closure_new,
     
    o_closure_function,

     
    o_interpolation,

     
    o_vm_exit
} loli_opcode;

#endif
