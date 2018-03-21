/**
 * Copyright (c) 2017 emekoi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

/*===========================================================================
 * Built-in primitives and funcs
 *===========================================================================*/

static ar_Value *p_do(ar_State *S, ar_Value *args, ar_Value *env) {
  return ar_do_list(S, args, env);
}


static ar_Value *p_set(ar_State *S, ar_Value *args, ar_Value *env) {
  ar_Value *sym, *v;
  do {
    sym = ar_check(S, ar_car(args), AR_TSYMBOL);
    v = ar_eval(S, ar_car(args = ar_cdr(args)), env);
    ar_set(S, sym, v, env);
  } while ( (args = ar_cdr(args)) );
  return v;
}


static ar_Value *p_quote(ar_State *S, ar_Value *args, ar_Value *env) {
  UNUSED(S);
  UNUSED(env);
  return ar_car(args);
}


static ar_Value *p_eval(ar_State *S, ar_Value *args, ar_Value *env) {
  ar_Value *e = ar_eval(S, ar_nth(args, 1), env);
  e = e ? ar_check(S, e, AR_TENV) : env;
  return ar_eval(S, ar_eval(S, ar_car(args), env), e);
}


static ar_Value *p_fn(ar_State *S, ar_Value *args, ar_Value *env) {
  ar_Value *v = ar_car(args);
  int t = ar_type(v);
  /* Type check */
  if (t != AR_TPAIR && t != AR_TSYMBOL) {
    ar_error_str(S, "expected pair or symbol, got %s", ar_type_str(t));
  }
  if (t == AR_TPAIR && (ar_car(v) || ar_cdr(v))) {
    while (v) {
      ar_check(S, ar_car(v), AR_TSYMBOL);
      v = ar_cdr(v);
    }
  }
  /* Init function */
  v = new_value(S, AR_TFUNC);
  v->u.func.params = ar_car(args);
  v->u.func.body = ar_cdr(args);
  v->u.func.env = env;
  return v;
}


static ar_Value *p_macro(ar_State *S, ar_Value *args, ar_Value *env) {
  ar_Value *v = p_fn(S, args, env);
  v->type = AR_TMACRO;
  return v;
}


static ar_Value *p_apply(ar_State *S, ar_Value *args, ar_Value *env) {
  ar_Value *fn = ar_eval(S, ar_car(args), env);
  return ar_call(S, fn, ar_eval(S, ar_nth(args, 1), env));
}


static ar_Value *p_if(ar_State *S, ar_Value *args, ar_Value *env) {
  ar_Value *cond, *next, *v = args;
  while (v) {
    cond = ar_eval(S, ar_car(v), env);
    next = ar_cdr(v);
    if (cond) {
      return next ? ar_eval(S, ar_car(next), env) : cond;
    }
    v = ar_cdr(next);
  }
  return NULL;
}


static ar_Value *p_and(ar_State *S, ar_Value *args, ar_Value *env) {
  ar_Value *res = NULL;
  while (args) {
    if ( !(res = ar_eval(S, ar_car(args), env)) ) return NULL;
    args = ar_cdr(args);
  }
  return res;
}


static ar_Value *p_or(ar_State *S, ar_Value *args, ar_Value *env) {
  ar_Value *res;
  while (args) {
    if ( (res = ar_eval(S, ar_car(args), env)) ) return res;
    args = ar_cdr(args);
  }
  return NULL;
}


static ar_Value *p_let(ar_State *S, ar_Value *args, ar_Value *env) {
  ar_Value *vars = ar_check(S, ar_car(args), AR_TPAIR);
  env = ar_new_env(S, env);
  while (vars) {
    ar_Value *sym = ar_check(S, ar_car(vars), AR_TSYMBOL);
    vars = ar_cdr(vars);
    ar_bind(S, sym, ar_eval(S, ar_car(vars), env), env);
    vars = ar_cdr(vars);
  }
  return ar_do_list(S, ar_cdr(args), env);
}


static ar_Value *p_while(ar_State *S, ar_Value *args, ar_Value *env) {
  ar_Value *cond = ar_car(args);
  ar_Value *body = ar_cdr(args);
  int orig_stack_idx = S->gc_stack_idx;
  while ( ar_eval(S, cond, env) ) {
    ar_do_list(S, body, env);
    /* Truncate stack so we don't accumulate protected values */
    S->gc_stack_idx = orig_stack_idx;
  }
  return NULL;
}


static ar_Value *p_pcall(ar_State *S, ar_Value *args, ar_Value *env) {
  ar_Value *res;
  ar_try(S, err, {
    res = ar_call(S, ar_eval(S, ar_car(args), env), NULL);
  }, {
    res = ar_call(S, ar_eval(S, ar_nth(args, 1), env), err);
  });
  return res;
}


/* I don't need this on MinGW. I don't know about MSCV.
#if defined(_WIN32)
static inline char *getcwd(char *buf, int len) {
  GetCurrentDirectory(len, buf);
  return buf;
}
#endif */

static ar_Value *p_import(ar_State *S, ar_Value *args, ar_Value *env) {
  ar_Value *res = ar_check(S, ar_eval(S, ar_nth(args, 0), env), AR_TSTRING);
  size_t i, j, found;
  i = 0, j = 0, found = 0;
  for (i = 0; ar_nth(args, i); i++) {
    /* Check all search paths */
    for (j = 0; ar_SearchPaths[j].path; j++) {
      char *r, *skip, cwd[BUFSIZ], path[BUFSIZ];
      ar_Lib *lib;
      ar_CFunc open_lib;
      /* Check for C libaries */
      skip = (char*)skipDotSlash(res->u.str.s);
      if (ar_SearchPaths[j].local) {
        sprintf(path, ar_SearchPaths[j].path, getcwd(cwd, sizeof(cwd)), skip);
      } else {
        sprintf(path, ar_SearchPaths[j].path, skip);
      }
      lib = ar_lib_load(S, path, 1);
      if (lib) {
        /* Try to run the library's open function */
        r = concat(S, AR_OFN, strtok(lib->name, "."), NULL);
        open_lib = ar_lib_sym(S, lib, r);
        dmt_free(r);
        if (open_lib(S, env)) {
          found = 1;
        } else {
            ar_error_str(S, "error loading module '%s' from '%s'", skip, path);
        }

        break;
      }
      /* Check for aria libraries */
      ar_try(S, err, { if (ar_do_file(S, path)) found = 1; }, {
        found = 0;
        UNUSED(err);
      });

      if (found) break;
    }
    if (!found)
      ar_error_str(S, "module \"%s\" not found", res->u.str.s);
  }
  if (ar_nth(args, 1) != NULL) p_import(S, ar_cdr(args), env);
  return S->t;
}


static ar_Value *f_gc(ar_State *S, ar_Value *args) {
  if (args) {
    if (ar_car(args)) {
      S->gc_active = 1;
    } else {
      S->gc_active = 0;
    }
  } else {
    ar_gc(S);
  }
  return NULL;
}


static ar_Value *f_list(ar_State *S, ar_Value *args) {
  UNUSED(S);
  return args;
}


static ar_Value *f_type(ar_State *S, ar_Value *args) {
  return ar_new_symbol(S, ar_type_str(ar_type(ar_car(args))));
}


static ar_Value *f_number(ar_State *S, ar_Value *args) {
  return ar_new_number(S, ar_to_number(S, ar_nth(args, 0)));
}


static ar_Value *f_print(ar_State *S, ar_Value *args) {
  while (args) {
    size_t len;
    const char *str = ar_to_stringl(S, ar_car(args), &len);
    fwrite(str, len, 1, stdout);
    if (!ar_cdr(args)) break;
    printf(" ");
    args = ar_cdr(args);
  }
  fwrite("\n", 1, 1, stdout);
  return ar_car(args);
}


#define formats(S, c, v) \
  buf[0] = '%'; buf[1] = c; buf[2] = '\0'; \
  return ar_new_stringf(S, buf, v)

#define formatn(S, c, v) \
  buf[0] = '%'; buf[1] = 'L'; buf[2] = c; buf[3] = '\0';\
  return ar_new_stringf(S, buf, v)

#define round(x) ((x >= 0 && floor(x + .5)) || ceil(x - .5))

static ar_Value *parse_format(ar_State *S, const char c, ar_Value *args) {
  int num;
  char buf[4];
  num = round(ar_to_number(S, ar_car(args)));
  switch (c) {
    case 'c': case 'u': {
      ar_check_number(S, ar_car(args));
      formatn(S, c, (unsigned int)num);
    }
    case 'i': case 'd': case 'x':
    case 'X': case 'o':{
      ar_check_number(S, ar_car(args));
      formatn(S, c, num);
    }
    case 'e': case 'E': case 'F': case 'g':{
      ar_check_number(S, ar_car(args));
      formatn(S, c, ar_to_number(S, ar_car(args)));
    }
    case 'p':{
      formats(S, c, ar_car(args));
    }
    case 'q':{
      formats(S, 's', ar_to_string_value(S, ar_car(args), 1)->u.str.s);
    }
    case 's':{
      formats(S, c, ar_to_string_value(S, ar_car(args), 0)->u.str.s);
    }
    default:
      if (is_alpha(c))
        ar_error_str(S, "invalid option '%c'", c);
      else
        ar_error_str(S, "expected option");
  }
  return NULL;
}


static ar_Value *f_format(ar_State *S, ar_Value *args) {
  size_t len;
  const char *str = ar_to_stringl(S, ar_car(args), &len);
  const char *str_end = str + len;
  ar_Value *res = NULL, **last = &res;
  while (str < str_end) {
    if (*str != AR_ESC) {
      char buf[2]; buf[0] = *str++; buf[1] = '\0';
      last = ar_append_tail(S, last, ar_new_string(S, buf));
    } else if (*++str == AR_ESC) {
      char buf[2]; buf[0] = *str++; buf[1] = '\0';
      last = ar_append_tail(S, last, ar_new_string(S, buf));
    } else {
      last = ar_append_tail(S, last, parse_format(S, *str++, ar_cdr(args)));
      args = ar_cdr(args);
    }
  }
  return join_list_of_strings(S, res);
}


static ar_Value *f_printf(ar_State *S, ar_Value *args) {
  return f_print(S, f_format(S, args));
}


static ar_Value *f_read(ar_State *S, ar_Value *args) {
  char str[BUFSIZ];
  switch (ar_type(ar_car(args))) {
    case AR_TNUMBER:
      return ar_new_string(S, fgets(str, ar_to_number(S, ar_car(args)) + 1, stdin));
    default:
      return ar_new_string(S, fgets(str, BUFSIZ, stdin));
  }
}


static ar_Value *f_parse(ar_State *S, ar_Value *args) {
  return ar_parse(S, ar_check_string(S, ar_car(args)),
                     ar_opt_string(S, ar_nth(args, 1), "(string)"));
}


static ar_Value *f_error(ar_State *S, ar_Value *args) {
  ar_error(S, ar_car(args));
  return NULL;
}


static ar_Value *f_dbgloc(ar_State *S, ar_Value *args) {
  return debug_location(S, ar_car(args));
}


static ar_Value *f_cons(ar_State *S, ar_Value *args) {
  return ar_new_pair(S, ar_car(args), ar_nth(args, 1));
}


static ar_Value *f_car(ar_State *S, ar_Value *args) {
  ar_Value *v = ar_car(args);
  if (!v) return NULL;
  return ar_check(S, v, AR_TPAIR)->u.pair.car;
}


static ar_Value *f_cdr(ar_State *S, ar_Value *args) {
  ar_Value *v = ar_car(args);
  if (!v) return NULL;
  return ar_check(S, v, AR_TPAIR)->u.pair.cdr;
}


static ar_Value *f_setcar(ar_State *S, ar_Value *args) {
  return ar_check(S, ar_car(args), AR_TPAIR)->u.pair.car = ar_nth(args, 1);
}


static ar_Value *f_setcdr(ar_State *S, ar_Value *args) {
  return ar_check(S, ar_car(args), AR_TPAIR)->u.pair.cdr = ar_nth(args, 1);
}


static ar_Value *f_string(ar_State *S, ar_Value *args) {
  ar_Value *res = NULL, **last = &res;
  ar_Value *v = args;
  while (v) {
    last = ar_append_tail(S, last, ar_to_string_value(S, ar_car(v), 0));
    v = ar_cdr(v);
  }
  return join_list_of_strings(S, res);
}


static ar_Value *f_substr(ar_State *S, ar_Value *args) {
  ar_Value *str = ar_check(S, ar_car(args), AR_TSTRING);
  int slen = str->u.str.len;
  int start = ar_opt_number(S, ar_nth(args, 1), 0);
  int len = ar_opt_number(S, ar_nth(args, 2), str->u.str.len);
  if (start < 0) start = slen + start;
  if (start < 0) len += start, start = 0;
  if (start + len > slen) len = slen - start;
  if (len < 0) len = 0;
  return ar_new_stringl(S, &str->u.str.s[start], len);
}


static ar_Value *f_strlen(ar_State *S, ar_Value *args) {
  return ar_new_number(S, ar_check(S, ar_car(args), AR_TSTRING)->u.str.len);
}


static ar_Value *f_strpos(ar_State *S, ar_Value *args) {
  ar_Value *haystack = ar_check(S, ar_car(args),  AR_TSTRING);
  ar_Value *needle = ar_check(S, ar_nth(args, 1), AR_TSTRING);
  unsigned offset = ar_opt_number(S, ar_nth(args, 2), 0);
  const char *p;
  if (offset >= haystack->u.str.len) return NULL;
  p = strstr(haystack->u.str.s + offset, needle->u.str.s);
  return p ? ar_new_number(S, p - haystack->u.str.s) : NULL;
}


static ar_Value *f_chr(ar_State *S, ar_Value *args) {
  char c = ar_check_number(S, ar_car(args));
  return ar_new_stringl(S, &c, 1);
}


static ar_Value *f_ord(ar_State *S, ar_Value *args) {
  return ar_new_number(S, *ar_check_string(S, ar_car(args)));
}


#define STRING_MAP_FUNC(NAME, FUNC)                           \
  static ar_Value *NAME(ar_State *S, ar_Value *args) {        \
    ar_Value *str = ar_check(S, ar_car(args), AR_TSTRING);    \
    ar_Value *res = ar_new_stringl(S, NULL, str->u.str.len);  \
    size_t i;                                                 \
    for (i = 0; i < res->u.str.len; i++) {                    \
      res->u.str.s[i] = FUNC(str->u.str.s[i]);                \
    }                                                         \
    return res;                                               \
  }

STRING_MAP_FUNC( f_lower, tolower )
STRING_MAP_FUNC( f_upper, toupper )


static ar_Value *f_loads(ar_State *S, ar_Value *args) {
  ar_Value *res;
  int r, size;
  FILE *fp = fopen(ar_check_string(S, ar_car(args)), "rb");
  if (!fp) ar_error_str(S, "could not open file");
  /* Get size */
  fseek(fp, 0, SEEK_END);
  size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  /* Load file into string value */
  res = ar_new_stringl(S, NULL, size);
  r = fread(res->u.str.s, 1, size, fp);
  fclose(fp);
  if (r != size) ar_error_str(S, "could not read file");
  return res;
}


static ar_Value *f_dumps(ar_State *S, ar_Value *args) {
  const char *name, *data;
  int r;
  size_t len;
  FILE *fp;
  name = ar_to_string( S, ar_check(S, ar_nth(args, 0), AR_TSTRING));
  data = ar_to_stringl(S, ar_check(S, ar_nth(args, 1), AR_TSTRING), &len);
  fp = fopen(name, ar_nth(args, 2) ? "ab" : "wb");
  if (!fp) ar_error_str(S, "could not open file");
  r = fwrite(data, len, 1, fp);
  fclose(fp);
  if (r != 1) ar_error_str(S, "could not write file");
  return NULL;
}


static ar_Value *f_is(ar_State *S, ar_Value *args) {
  return is_equal(ar_car(args), ar_nth(args, 1)) ?  S->t : NULL;
}


static ar_Value *f_now(ar_State *S, ar_Value *args) {
  long double t;
  #ifdef _WIN32
    FILETIME ft;
  #else
    struct timeval tv;
  #endif
  UNUSED(args);
  #ifdef _WIN32
    GetSystemTimeAsFileTime(&ft);
    t = (ft.dwHighDateTime * 4294967296.0 / 1e7) + ft.dwLowDateTime / 1e7;
    t -= 11644473600.0;
    return ar_new_number(S, t);
  #else
    gettimeofday(&tv, NULL);
    t = tv.tv_sec + tv.tv_usec / 1e6;
  #endif
  return ar_new_number(S, t);
}


static ar_Value *f_clock(ar_State *S, ar_Value *args) {
  UNUSED(args);
  return ar_new_number(S, (long double) clock() / (long double) CLOCKS_PER_SEC);
}


static ar_Value *f_sleep(ar_State *S, ar_Value *args) {
  clock_t target = clock() / CLOCKS_PER_SEC + ar_to_number(S, ar_nth(args, 0));
  while (clock() / CLOCKS_PER_SEC != target);
  return NULL;
}


static ar_Value *f_exit(ar_State *S, ar_Value *args) {
  exit(ar_opt_number(S, ar_car(args), EXIT_SUCCESS));
  return NULL;
}


static ar_Value** pairs(ar_State *S, ar_Value *node, ar_Value **stack) {
  if (node) {
    stack = ar_append_tail(S, stack, node->u.map.pair);
    stack = pairs(S, node->u.map.left, stack);
    stack = pairs(S, node->u.map.right, stack);
  }
  return stack;
}


static ar_Value *f_pairs(ar_State *S, ar_Value *args) {
  ar_Value *env = ar_check(S, ar_nth(args, 0), AR_TENV)->u.env.map;
  ar_Value *res = NULL;
  pairs(S, env, &res);
  return res;
}


void register_stdlib(ar_State *S) {
  int i;
  /* Primitives */
  struct { const char *name; ar_Prim fn; } prims[] = {
    { "=",        p_set     },
    { "do",       p_do      },
    { "quote",    p_quote   },
    { "eval",     p_eval    },
    { "fn",       p_fn      },
    { "macro",    p_macro   },
    { "apply",    p_apply   },
    { "if",       p_if      },
    { "and",      p_and     },
    { "or",       p_or      },
    { "let",      p_let     },
    { "while",    p_while   },
    { "pcall",    p_pcall   },
    { "import",   p_import  },
    { NULL, NULL }
  };
  /* Functions */
  struct { const char *name; ar_CFunc fn; } funcs[] = {
    { "gc",       f_gc      },
    { "list",     f_list    },
    { "type",     f_type    },
    { "number",   f_number  },
    { "print",    f_print   },
    { "format",   f_format  },
    { "printf",   f_printf  },
    { "read",     f_read    },
    { "parse",    f_parse   },
    { "error",    f_error   },
    { "dbgloc",   f_dbgloc  },
    { "cons",     f_cons    },
    { "car",      f_car     },
    { "cdr",      f_cdr     },
    { "setcar",   f_setcar  },
    { "setcdr",   f_setcdr  },
    { "string",   f_string  },
    { "substr",   f_substr  },
    { "strlen",   f_strlen  },
    { "strpos",   f_strpos  },
    { "chr",      f_chr     },
    { "ord",      f_ord     },
    { "lower",    f_lower   },
    { "upper",    f_upper   },
    { "loads",    f_loads   },
    { "dumps",    f_dumps   },
    { "is",       f_is      },
    { "now",      f_now     },
    { "clock",    f_clock   },
    { "sleep",    f_sleep   },
    { "exit",     f_exit    },
    { "pairs",    f_pairs   },
    { NULL,       NULL      }
  };
  /* String Constants */
  struct { const char *name; void *val; } str_constants[] = {
    { "VERSION", AR_VERSION },
    { NULL,      NULL       }
  };
  /* Register */
  for (i = 0; prims[i].name; i++) {
    ar_bind_global(S, prims[i].name, ar_new_prim(S, prims[i].fn));
  }
  for (i = 0; funcs[i].name; i++) {
    ar_bind_global(S, funcs[i].name, ar_new_cfunc(S, funcs[i].fn));
  }
  for (i = 0; str_constants[i].name; i++) {
    ar_bind_global(S, str_constants[i].name, ar_new_string(S, str_constants[i].val));
  }
}
