#ifndef LOLI_VALUE_RAW_H
# define LOLI_VALUE_RAW_H

# include "loli_value_structs.h"

# include "loli.h"

loli_container_val *loli_new_list_raw(int);
loli_container_val *loli_new_tuple_raw(int);
loli_container_val *loli_new_instance_raw(uint16_t, int);
loli_bytestring_val *loli_new_bytestring_raw(const char *, int);
loli_string_val *loli_new_string_raw(const char *);
loli_hash_val *loli_new_hash_raw(int);

void loli_deref(loli_value *);
void loli_value_assign(loli_value *, loli_value *);
void loli_value_take(loli_state *, loli_value *);
int loli_value_compare(loli_state *, loli_value *, loli_value *);
loli_value *loli_value_copy(loli_value *);
loli_value *loli_stack_take(loli_state *);
void loli_stack_push_and_destroy(loli_state *, loli_value *);

#endif
