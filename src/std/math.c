/**
 * Copyright (c) 2017 emekoi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

/*===========================================================================
 * Math related funcs
 *===========================================================================*/

#undef PI
#define PI (3.141592653589793238462643383279502884)


#define NUM_COMPARE_FUNC(NAME, OP)                                \
  static ar_Value *NAME(ar_State *S, ar_Value *args) {            \
    return ( ar_check_number(S, ar_car(args)) OP                  \
             ar_check_number(S, ar_nth(args, 1)) ) ? S->t : NULL; \
  }

NUM_COMPARE_FUNC( f_lt,  <  )
NUM_COMPARE_FUNC( f_gt,  >  )
NUM_COMPARE_FUNC( f_lte, <= )
NUM_COMPARE_FUNC( f_gte, >= )


#define NUM_ARITH_FUNC(NAME, OP)                        \
  static ar_Value *NAME(ar_State *S, ar_Value *args) {  \
    long double res = ar_check_number(S, ar_car(args));      \
    while ( (args = ar_cdr(args)) ) {                   \
      res = res OP ar_check_number(S, ar_car(args));    \
    }                                                   \
    return ar_new_number(S, res);                       \
  }

NUM_ARITH_FUNC( f_add, + )
NUM_ARITH_FUNC( f_sub, - )
NUM_ARITH_FUNC( f_mul, * )
NUM_ARITH_FUNC( f_div, / )


static ar_Value *f_mod(ar_State *S, ar_Value *args) {
  long double a = ar_check_number(S, ar_nth(args, 0));
  long double b = ar_check_number(S, ar_nth(args, 1));
  if (b == 0.) ar_error_str(S, "expected a non-zero divisor");
  return ar_new_number(S, fmod(a, b));
}

#define NUM_MATH_FUNC1(NAME, func)                      \
  static ar_Value *NAME(ar_State *S, ar_Value *args) {  \
    return ar_new_number(S, func(ar_check_number(S,     \
      ar_nth(args, 0))));                               \
  }

#define NUM_MATH_FUNC2(NAME, func)                      \
  static ar_Value *NAME(ar_State *S, ar_Value *args) {  \
    return ar_new_number(S, func(ar_check_number(S,     \
      ar_nth(args, 0)), ar_check_number(S,              \
      ar_nth(args, 1))));                               \
  }


NUM_MATH_FUNC1(f_acos, acos)
NUM_MATH_FUNC1(f_asin, asin)
NUM_MATH_FUNC1(f_ceil, ceil)
NUM_MATH_FUNC1(f_cos, cos)
NUM_MATH_FUNC1(f_exp, exp)
NUM_MATH_FUNC1(f_floor, floor)
NUM_MATH_FUNC1(f_sin, sin)
NUM_MATH_FUNC1(f_sqrt, sqrt)
NUM_MATH_FUNC1(f_tan, tan)
NUM_MATH_FUNC2(f_pow, pow)


static ar_Value *f_atan(ar_State *S, ar_Value *args) {
  long double a, b;
  a = ar_check_number(S, ar_nth(args, 0));
  b = ar_nth(args, 1) ? ar_check_number(S, ar_nth(args, 1)) : 1;
  if (b == 0.0) ar_error_str(S, "expected a non-zero divisor");
  return ar_new_number(S, atan2(a, b));
}


static ar_Value *f_deg(ar_State *S, ar_Value *args) {
  long double a;
  a = ar_check_number(S, ar_nth(args, 0));
  return ar_new_number(S, a * (180.0 / PI));
}


static ar_Value *f_rad(ar_State *S, ar_Value *args) {
  long double a;
  a = ar_check_number(S, ar_nth(args, 0));
  return ar_new_number(S, a * (PI / 180.0));
}


static ar_Value *f_modf(ar_State *S, ar_Value *args) {
  int b;
  long double a;
  a = ar_check_number(S, ar_nth(args, 0)); b = a;
  return ar_new_list(S, 2, ar_new_number(S, b), ar_new_number(S, a - b));
}


static ar_Value *f_log(ar_State *S, ar_Value *args) {
  long double a, b, res;
  a = ar_check_number(S, ar_nth(args, 0));
  if (!ar_nth(args, 1))
    res = log(a);
  else {
    b = ar_check_number(S, ar_nth(args, 1));
    if (b == 10.0) res = log10(a);
    else res = log(a) / log(b);
  }
  return ar_new_number(S, res);
}


void register_math(ar_State *S) {
  int i;
  /* Functions */
  struct { const char *name; ar_CFunc fn; } funcs[] = {
    { "<",        f_lt      },
    { ">",        f_gt      },
    { "<=",       f_lte     },
    { ">=",       f_gte     },
    { "+",        f_add     },
    { "-",        f_sub     },
    { "*",        f_mul     },
    { "/",        f_div     },
    { "pow*",     f_pow     },
    { "mod",      f_mod     },
    { "acos",     f_acos    },
    { "asin",     f_asin    },
    { "atan",     f_atan    },
    { "ceil",     f_ceil    },
    { "cos",      f_cos     },
    { "exp",      f_exp     },
    { "deg",      f_deg     },
    { "floor",    f_floor   },
    { "log",      f_log     },
    { "modf",     f_modf    },
    { "rad",      f_rad     },
    { "sin",      f_sin     },
    { "sqrt",     f_sqrt    },
    { "tan",      f_tan     },
    { NULL,       NULL      }
  };
  /* Math Constants */
  struct { const char *name; double val; } math_constants[] = {
    { "math-huge",  HUGE_VAL  },
    { "-math-huge", -HUGE_VAL },
    { "math-pi",    PI        },
    { "-math-pi",   -PI       },
    { NULL,         0         }
  };
  /* Register */
  for (i = 0; funcs[i].name; i++) {
    ar_bind_global(S, funcs[i].name, ar_new_cfunc(S, funcs[i].fn));
  }
  for (i = 0; math_constants[i].name; i++) {
    ar_bind_global(S, math_constants[i].name, ar_new_number(S, math_constants[i].val));
  }
}
