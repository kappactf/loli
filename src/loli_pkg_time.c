
#include <time.h>
#include <string.h>

#include "loli.h"

typedef struct loli_time_Time_ {
    LOLI_FOREIGN_HEADER
    struct tm local;
} loli_time_Time;
#define ARG_Time(state, index) \
(loli_time_Time *)loli_arg_generic(state, index)
#define ID_Time(state) loli_cid_at(state, 0)
#define INIT_Time(state)\
(loli_time_Time *) loli_push_foreign(state, ID_Time(state), (loli_destroy_func)destroy_Time, sizeof(loli_time_Time))

const char *loli_time_info_table[] = {
    "\01Time\0"
    ,"C\04Time\0"
    ,"m\0clock\0: Double"
    ,"m\0now\0: Time"
    ,"m\0to_s\0(Time, *String): String"
    ,"m\0since_epoch\0(Time): Integer"
    ,"Z"
};
void loli_time_Time_clock(loli_state *);
void loli_time_Time_now(loli_state *);
void loli_time_Time_to_s(loli_state *);
void loli_time_Time_since_epoch(loli_state *);
loli_call_entry_func loli_time_call_table[] = {
    NULL,
    NULL,
    loli_time_Time_clock,
    loli_time_Time_now,
    loli_time_Time_to_s,
    loli_time_Time_since_epoch,
};


void destroy_Time(loli_time_Time *t)
{
}

void loli_time_Time_clock(loli_state *s)
{
    loli_return_double(s, ((double)clock())/(double)CLOCKS_PER_SEC);
}

void loli_time_Time_now(loli_state *s)
{
    loli_time_Time *t = INIT_Time(s);

    time_t raw_time;
    struct tm *time_info;

    time(&raw_time);
    time_info = localtime(&raw_time);
    t->local = *time_info;

    loli_return_top(s);
}

void loli_time_Time_to_s(loli_state *s)
{
    loli_time_Time *t = ARG_Time(s, 0);
    char *format = "%Y-%m-%d %H:%M:%S %z";
    if(loli_arg_count(s) == 2)
        format = loli_arg_string_raw(s, 1);   
      
    if(strlen(format) >= 64)
        format[62] = '\0';
    
    char buf[64];

    strftime(buf, sizeof(buf), format, &t->local);

    loli_push_string(s, buf);
    loli_return_top(s);
}

void loli_time_Time_since_epoch(loli_state *s)
{
    loli_time_Time *t = ARG_Time(s, 0);

    loli_return_integer(s, (int64_t) mktime(&t->local));
}
