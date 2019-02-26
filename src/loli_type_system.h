# ifndef LOLI_TYPE_SYSTEM_H
# define LOLI_TYPE_SYSTEM_H

# include "loli_core_types.h"
# include "loli_type_maker.h"


typedef struct {
    uint16_t pos;
    uint16_t num_used;
    uint16_t scoop_count;
    uint16_t pad;
} loli_ts_save_point;

typedef struct {
    loli_type **types;
    loli_type **base;
    uint16_t pos;
    uint16_t num_used;
    uint16_t max_seen;
    uint16_t max;

    uint16_t scoop_count;
    uint16_t pad1;
    uint32_t pad2;

    loli_type_maker *tm;
} loli_type_system;

loli_type_system *loli_new_type_system(loli_type_maker *);

void loli_free_type_system(loli_type_system *);

int loli_ts_check(loli_type_system *, loli_type *, loli_type *);

loli_type *loli_ts_unify(loli_type_system *, loli_type *, loli_type *);

int loli_ts_type_greater_eq(loli_type_system *, loli_type *, loli_type *);

void loli_ts_reset_scoops(loli_type_system *);

loli_type *loli_ts_resolve(loli_type_system *, loli_type *);

loli_type *loli_ts_resolve_by_second(loli_type_system *, loli_type *, loli_type *);

loli_type *loli_ts_resolve_unscoop(loli_type_system *, loli_type *);

void loli_ts_scope_save(loli_type_system *, loli_ts_save_point *);

void loli_ts_scope_restore(loli_type_system *, loli_ts_save_point *);

void loli_ts_generics_seen(loli_type_system *, int);

int loli_func_type_num_optargs(loli_type *);

int loli_class_greater_eq(loli_class *, loli_class *);

int loli_class_greater_eq_id(int, loli_class *);

#endif
