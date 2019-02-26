#ifndef LOLI_KEYWORD_TABLE_H
# define LOLI_KEYWORD_TABLE_H

typedef struct {
    const char *name;
    uint64_t shorthash;
} keyword_entry;

keyword_entry constants[] = {
    {"true",         1702195828},
    {"self",         1718379891},
    {"unit",         1953066613},
    {"false",        435728179558},
    {"__file__",     6872323072689856351},
    {"__line__",     6872323081280184159},
    {"__function__", 7598807797348065119},
};

#define CONST_TRUE        0
#define CONST_SELF        1
#define CONST_UNIT        2
#define CONST_FALSE       3
#define CONST__FILE__     4
#define CONST__LINE__     5
#define CONST__FUNCTION__ 6
#define CONST_LAST_ID     6

keyword_entry keywords[] = {
    {"if",           26217},
    {"fn",           28262},
    {"do",           28516},
    {"sta",          6386803},
    {"pub",          6452592},
    {"pri",          6910576},
    {"pro",          7303792},
    {"var",          7496054},
    {"for",          7499622},
    {"try",          7959156},
    {"case",         1702060387},
    {"else",         1702063205},
    {"elif",         1718185061},
    {"enum",         1836412517},
    {"while",        435610544247},
    {"raise",        435727982962},
    {"match",        448345170285},
    {"break",        461195539042},
    {"class",        495857003619},
    {"scoped",       110386840822643},
    {"future",       111542271243622},
    {"return",       121437875889522},
    {"except",       128026086176869},
    {"import",       128034844732777},
    {"continue",     7310870969309884259},
};

#define KEY_IF 0
#define KEY_FN 1
#define KEY_DO 2
#define KEY_STATIC 3
#define KEY_PUBLIC 4
#define KEY_PRIVATE 5
#define KEY_PROTECTED 6
#define KEY_VAR 7
#define KEY_FOR 8
#define KEY_TRY 9
#define KEY_CASE 10
#define KEY_ELSE 11
#define KEY_ELIF 12
#define KEY_ENUM 13
#define KEY_WHILE 14
#define KEY_RAISE 15
#define KEY_MATCH 16
#define KEY_BREAK 17
#define KEY_CLASS 18
#define KEY_SCOPED 19
#define KEY_FUTURE 20
#define KEY_RETURN 21
#define KEY_EXCEPT 22
#define KEY_IMPORT 23
#define KEY_CONTINUE 24
#define KEY_LAST_ID  24


#endif
