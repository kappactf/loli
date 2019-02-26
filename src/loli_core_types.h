#ifndef LOLI_CORE_TYPES_H
# define LOLI_CORE_TYPES_H

# include <stdint.h>


struct loli_var_;
struct loli_type_;
struct loli_vm_state_;

typedef struct {
    struct loli_class_ *next;

    uint16_t item_kind;
    uint16_t flags;

    uint16_t cls_id;
    uint16_t pad;

    struct loli_type_ *build_type;

    char *name;
    uint64_t shorthash;

    struct loli_class_ *parent;

    char *arg_names;
} loli_variant_class;

typedef struct loli_class_ {
    struct loli_class_ *next;

    uint16_t item_kind;
    uint16_t flags;
    uint16_t id;

    uint16_t type_subtype_count;


    struct loli_type_ *self_type;

    char *name;

    uint64_t shorthash;

    struct loli_class_ *parent;

    struct loli_named_sym_ *members;

    uint16_t inherit_depth;

    int16_t generic_count;
    union {
        uint16_t prop_count;
        uint16_t variant_size;
    };
    uint16_t dyna_start;


    struct loli_module_entry_ *module;


    struct loli_type_ *all_subtypes;
} loli_class;

typedef struct loli_type_ {

    struct loli_type_ *next;

    uint16_t item_kind;
    uint16_t flags;

    uint16_t generic_pos;
    uint16_t subtype_count;

    loli_class *cls;


    struct loli_type_ **subtypes;
} loli_type;






typedef struct {
    void *pad;
    uint16_t item_kind;
    uint16_t flags;
    uint16_t id;
    uint16_t pad2;
} loli_item;

typedef struct loli_sym_ {
    void *pad;
    uint16_t item_kind;
    uint16_t flags;

    uint16_t reg_spot;
    uint16_t pad2;
    loli_type *type;
} loli_sym;

typedef struct loli_named_sym_ {
    struct loli_named_sym_ *next;
    uint16_t item_kind;
    uint16_t flags;
    union {
        uint16_t reg_spot;
        uint16_t id;
    };
    uint16_t pad;
    loli_type *type;
    char *name;
    uint64_t name_shorthash;
} loli_named_sym;

typedef struct loli_boxed_sym_ {
    struct loli_boxed_sym_ *next;
    uint64_t pad;
    loli_named_sym *inner_sym;
} loli_boxed_sym;

typedef struct loli_prop_entry_ {
    struct loli_prop_entry_ *next;
    uint16_t item_kind;
    uint16_t flags;
    uint16_t id;
    uint16_t pad;
    struct loli_type_ *type;
    char *name;
    uint64_t name_shorthash;
    loli_class *cls;
} loli_prop_entry;

typedef struct loli_storage_ {
    struct loli_storage_ *next;
    uint16_t item_kind;
    uint16_t flags;
    uint16_t reg_spot;
    uint16_t pad;

    loli_type *type;
    uint32_t expr_num;
} loli_storage;

typedef struct loli_var_ {
    struct loli_var_ *next;
    uint16_t item_kind;
    uint16_t flags;
    uint16_t reg_spot;
    uint16_t pad;
    loli_type *type;
    char *name;
    uint64_t shorthash;

    uint32_t line_num;

    uint32_t function_depth;

    struct loli_class_ *parent;
} loli_var;





typedef struct loli_module_link_ {
    struct loli_module_entry_ *module;
    char *as_name;
    struct loli_module_link_ *next_module;
} loli_module_link;

typedef struct loli_module_entry_ {

    struct loli_module_entry_ *root_next;


    uint16_t item_kind;

    uint16_t flags;

    uint16_t pad;

    uint16_t cmp_len;


    char *loadname;


    char *dirname;


    union {
        char *path;

        const char *const_path;
    };


    loli_module_link *module_chain;


    loli_class *class_chain;


    loli_var *var_chain;

    loli_boxed_sym *boxed_chain;

    const char *root_dirname;


    void *handle;


    const char **info_table;

    void (**call_table)(struct loli_vm_state_ *);

    uint16_t *cid_table;
} loli_module_entry;

typedef struct loli_proto_ {

    const char *module_path;

    char *name;

    uint16_t *locals;

    uint16_t *code;

    char *arg_names;
} loli_proto;




#define ITEM_TYPE_VAR      1
#define ITEM_TYPE_STORAGE  2
#define ITEM_TYPE_VARIANT  3
#define ITEM_TYPE_PROPERTY 4
#define ITEM_TYPE_MODULE   5
#define ITEM_TYPE_TYPE     6
#define ITEM_TYPE_CLASS    7



#define TYPE_IS_VARARGS    0x01
#define TYPE_IS_UNRESOLVED 0x02
#define TYPE_HAS_OPTARGS   0x04
#define TYPE_IS_INCOMPLETE 0x10
#define TYPE_HAS_SCOOP     0x20



#define CLS_VALID_HASH_KEY 0x0040
#define CLS_IS_ENUM        0x0080
#define CLS_GC_TAGGED      0x0100
#define CLS_GC_SPECULATIVE 0x0200
#define CLS_ENUM_IS_SCOPED 0x0400
#define CLS_EMPTY_VARIANT  0x0800
#define CLS_IS_BUILTIN     0x1000
#define CLS_VISITED        0x2000

#define CLS_GC_FLAGS       (CLS_GC_SPECULATIVE | CLS_GC_TAGGED)



#define SYM_NOT_INITIALIZED     0x01
#define SYM_NOT_ASSIGNABLE      0x02

#define SYM_SCOPE_PRIVATE       0x04

#define SYM_SCOPE_PROTECTED     0x08




#define VAR_IS_READONLY         0x020

#define VAR_NEEDS_CLOSURE       0x040

#define VAR_IS_GLOBAL           0x080

#define VAR_IS_FOREIGN_FUNC     0x100

#define VAR_IS_STATIC           0x200

#define VAR_IS_FUTURE          0x400

#define MODULE_IS_REGISTERED 0x1

#define MODULE_NOT_EXECUTED  0x2

#define MODULE_IN_EXECUTION  0x4

#define LOLI_LAST_ID       65528
#define LOLI_ID_SELF       65529
#define LOLI_ID_QUESTION   65530
#define LOLI_ID_GENERIC    65531
#define LOLI_ID_OPTARG     65532
#define LOLI_ID_SCOOP      65534
#endif
