
#define _USE_MATH_DEFINES 

#include <math.h>
#include <stdlib.h>

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

#include "loli.h"

const char *loli_math_info_table[] = {
    "\0\0"
    ,"F\0abs\0(Integer): Integer"
    ,"F\0acos\0(Double): Double"
    ,"F\0asin\0(Double): Double"
    ,"F\0atan\0(Double): Double"
    ,"F\0ceil\0(Double): Double"
    ,"F\0cos\0(Double): Double"
    ,"F\0cosh\0(Double): Double"
    ,"F\0exp\0(Double): Double"
    ,"F\0fabs\0(Double): Double"
    ,"F\0floor\0(Double): Double"
    ,"F\0fmod\0(Double,Double): Double"
    ,"F\0is_infinity\0(Double): Boolean"
    ,"F\0is_nan\0(Double): Boolean"
    ,"F\0ldexp\0(Double,Integer): Double"
    ,"F\0log\0(Double): Double"
    ,"F\0log10\0(Double): Double"
    ,"F\0modf\0(Double): Tuple[Double,Double]"
    ,"F\0pow\0(Double,Double): Double"
    ,"F\0sin\0(Double): Double"
    ,"F\0sinh\0(Double): Double"
    ,"F\0sqrt\0(Double): Double"
    ,"F\0tan\0(Double): Double"
    ,"F\0tanh\0(Double): Double"
    ,"F\0to_deg\0(Double): Double"
    ,"F\0to_rad\0(Double): Double"
    ,"R\0huge\0Double"
    ,"R\0infinity\0Double"
    ,"R\0nan\0Double"
    ,"R\0pi\0Double"
    ,"Z"
};
#define toplevel_OFFSET 1
void loli_math__abs(loli_state *);
void loli_math__acos(loli_state *);
void loli_math__asin(loli_state *);
void loli_math__atan(loli_state *);
void loli_math__ceil(loli_state *);
void loli_math__cos(loli_state *);
void loli_math__cosh(loli_state *);
void loli_math__exp(loli_state *);
void loli_math__fabs(loli_state *);
void loli_math__floor(loli_state *);
void loli_math__fmod(loli_state *);
void loli_math__is_infinity(loli_state *);
void loli_math__is_nan(loli_state *);
void loli_math__ldexp(loli_state *);
void loli_math__log(loli_state *);
void loli_math__log10(loli_state *);
void loli_math__modf(loli_state *);
void loli_math__pow(loli_state *);
void loli_math__sin(loli_state *);
void loli_math__sinh(loli_state *);
void loli_math__sqrt(loli_state *);
void loli_math__tan(loli_state *);
void loli_math__tanh(loli_state *);
void loli_math__to_deg(loli_state *);
void loli_math__to_rad(loli_state *);
void loli_math_var_huge(loli_state *);
void loli_math_var_infinity(loli_state *);
void loli_math_var_nan(loli_state *);
void loli_math_var_pi(loli_state *);
void (*loli_math_call_table[])(loli_state *s) = {
    NULL,
    loli_math__abs,
    loli_math__acos,
    loli_math__asin,
    loli_math__atan,
    loli_math__ceil,
    loli_math__cos,
    loli_math__cosh,
    loli_math__exp,
    loli_math__fabs,
    loli_math__floor,
    loli_math__fmod,
    loli_math__is_infinity,
    loli_math__is_nan,
    loli_math__ldexp,
    loli_math__log,
    loli_math__log10,
    loli_math__modf,
    loli_math__pow,
    loli_math__sin,
    loli_math__sinh,
    loli_math__sqrt,
    loli_math__tan,
    loli_math__tanh,
    loli_math__to_deg,
    loli_math__to_rad,
    loli_math_var_huge,
    loli_math_var_infinity,
    loli_math_var_nan,
    loli_math_var_pi,
};

void loli_math__abs(loli_state *s)
{
    int64_t x = loli_arg_integer(s, 0);

    loli_return_integer(s, abs(x));
}

void loli_math__acos(loli_state *s)
{
    double x = loli_arg_double(s, 0);

    loli_return_double(s, acos(x));
}

void loli_math__asin(loli_state *s)
{
    double x = loli_arg_double(s, 0);

    loli_return_double(s, asin(x));
}

void loli_math__atan(loli_state *s)
{
    double x = loli_arg_double(s, 0);

    loli_return_double(s, atan(x));
}

void loli_math__ceil(loli_state *s)
{
    double x = loli_arg_double(s, 0);

    loli_return_double(s, ceil(x));
}

void loli_math__cos(loli_state *s)
{
    double x = loli_arg_double(s, 0);

    loli_return_double(s, cos(x));
}

void loli_math__cosh(loli_state *s)
{
    double x = loli_arg_double(s, 0);

    loli_return_double(s, cosh(x));
}

void loli_math__exp(loli_state *s)
{
    double x = loli_arg_double(s, 0);

    loli_return_double(s, exp(x));
}

void loli_math__fabs(loli_state *s)
{
    double x = loli_arg_double(s, 0);

    loli_return_double(s, fabs(x));
}

void loli_math__floor(loli_state *s)
{
    double x = loli_arg_double(s, 0);

    loli_return_double(s, floor(x));
}

void loli_math__fmod(loli_state *s)
{
    double x = loli_arg_double(s, 0);
    double y = loli_arg_double(s, 1);

    loli_return_double(s, fmod(x, y));
}

void loli_math__is_infinity(loli_state *s)
{
    double x = loli_arg_double(s, 0);

    loli_return_boolean(s, isinf(x));
}

void loli_math__is_nan(loli_state *s)
{
    double x = loli_arg_double(s, 0);

    loli_return_boolean(s, isnan(x));
}

void loli_math__ldexp(loli_state *s)
{
    double x = loli_arg_double(s, 0);
    double y = loli_arg_integer(s, 1);

    loli_return_double(s, ldexp(x, y));
}

void loli_math__log(loli_state *s)
{
    double x = loli_arg_double(s, 0);

    loli_return_double(s, log(x));
}

void loli_math__log10(loli_state *s)
{
    double x = loli_arg_double(s, 0);

    loli_return_double(s, log10(x));
}

void loli_math__modf(loli_state *s)
{
    double i, f;
    double x = loli_arg_double(s, 0);

    f = modf(x, &i);

    loli_container_val *tpl = loli_push_tuple(s, 2);
    loli_push_double(s, i);
    loli_con_set_from_stack(s, tpl, 0);
    loli_push_double(s, f);
    loli_con_set_from_stack(s, tpl, 1);

    loli_return_top(s);
}

void loli_math__pow(loli_state *s)
{
    double x = loli_arg_double(s, 0);
    double y = loli_arg_double(s, 1);

    loli_return_double(s, pow(x, y));
}

void loli_math__sin(loli_state *s)
{
    double x = loli_arg_double(s, 0);

    loli_return_double(s, sin(x));
}

void loli_math__sinh(loli_state *s)
{
    double x = loli_arg_double(s, 0);

    loli_return_double(s, sinh(x));
}

void loli_math__sqrt(loli_state *s)
{
    double x = loli_arg_double(s, 0);

    loli_return_double(s, sqrt(x));
}

void loli_math__tan(loli_state *s)
{
    double x = loli_arg_double(s, 0);

    loli_return_double(s, tan(x));
}

void loli_math__tanh(loli_state *s)
{
    double x = loli_arg_double(s, 0);

    loli_return_double(s, tanh(x));
}

void loli_math__to_deg(loli_state *s)
{
    double x = loli_arg_double(s, 0);

    loli_return_double(s, x * (180 / M_PI));
}

void loli_math__to_rad(loli_state *s)
{
    double x = loli_arg_double(s, 0);

    loli_return_double(s, x * (M_PI / 180));
}



void loli_math_var_huge(loli_state *s)
{
    loli_push_double(s, HUGE_VAL);
}

void loli_math_var_infinity(loli_state *s)
{
    loli_push_double(s, INFINITY);
}

void loli_math_var_nan(loli_state *s)
{
    loli_push_double(s, NAN);
}

void loli_math_var_pi(loli_state *s)
{
    loli_push_double(s, M_PI);
}
