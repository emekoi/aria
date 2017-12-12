/**
 * Copyright (c) 2017 emekoi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include "util.h"
#include "aria.h"
#include "dmt/dmt.h"

#define MAX_STACK 1024
#define CHUNK_LEN 1024

#define AR_POF "ar_open"
#define AR_OFSEP "_"
#define AR_OFN AR_POF AR_OFSEP
#define AR_ESC '%'

struct ar_Chunk {
  ar_Value values[CHUNK_LEN];
  struct ar_Chunk *next;
};


struct ar_Lib {
  char *name;
  void *data;
  struct ar_Lib *next;
};


struct { const char *path; unsigned char local; } ar_SearchPaths[] = {
  #ifdef _WIN32
    { "/usr/local/share/aria/" AR_VERSION "/%s.lsp", 0 },
    { "/usr/local/lib/aria/" AR_VERSION "/%s.dll",   0 },
    { "%s/%s.dll",                                   1 },
    { "%s/%s.lsp",                                   1 },
    { NULL,                                          0 }
  #else
    { "/usr/local/share/aria/" AR_VERSION "/%s.lsp", 0 },
    { "/usr/local/lib/aria/" AR_VERSION "/%s.so",    0 },
    { "%s/%s.so",                                    1 },
    { "%s/%s.lsp",                                   1 },
    { NULL,                                          0 }
  #endif
};


void *ar_alloc(ar_State *S, void *ptr, size_t n) {
  void *p = S->alloc(S->udata, ptr, n);
  if (!p) ar_error(S, S->oom_error);
  return p;
}

void ar_free(ar_State *S, void *ptr) {
  S->alloc(S, ptr, 0);
}


/*===========================================================================
 * Value
 *===========================================================================*/

static void push_value_to_stack(ar_State *S, ar_Value *v) {
  /* Expand stack's capacity? */
  if (S->gc_stack_idx == S->gc_stack_cap) {
    int n = (S->gc_stack_cap << 1) | !S->gc_stack_cap;
    S->gc_stack = ar_alloc(S, S->gc_stack, n * sizeof(*S->gc_stack));
    S->gc_stack_cap = n;
  }
  /* Push value */
  S->gc_stack[S->gc_stack_idx++] = v;
}


static ar_Value *new_value(ar_State *S, size_t type) {
  ar_Value *v;
  /* Run garbage collector? */
  S->gc_count--;
  if (!S->gc_pool && S->gc_count < 0) {
    ar_gc(S);
  }
  /* No values in pool? Create and init new chunk */
  if (!S->gc_pool) {
    int i;
    ar_Chunk *c = ar_alloc(S, NULL, sizeof(*c));
    c->next = S->gc_chunks;
    S->gc_chunks = c;
    /* Init all chunk's values and link them together, set the currently-empty
     * pool to point to this new list */
    for (i = 0; i < CHUNK_LEN; i++) {
      c->values[i].type = AR_TNIL;
      c->values[i].u.pair.cdr = (c->values + i + 1);
    }
    c->values[CHUNK_LEN - 1].u.pair.cdr = NULL;
    S->gc_pool = c->values;
  }
  /* Get value from pool */
  v = S->gc_pool;
  S->gc_pool = v->u.pair.cdr;
  /* Init */
  v->type = type;
  v->mark = 0;
  push_value_to_stack(S, v);
  return v;
}


ar_Value *ar_new_env(ar_State *S, ar_Value *parent) {
  ar_Value *res = new_value(S, AR_TENV);
  res->u.env.parent = parent;
  res->u.env.map = NULL;
  return res;
}


ar_Value *ar_new_pair(ar_State *S, ar_Value *car, ar_Value *cdr) {
  ar_Value *res = new_value(S, AR_TPAIR);
  res->u.pair.car = car;
  res->u.pair.cdr = cdr;
  res->u.pair.dbg = NULL;
  return res;
}


ar_Value *ar_new_list(ar_State *S, size_t n, ...) {
  va_list args;
  ar_Value *res = NULL, **last = &res;
  va_start(args, n);
  while (n--) {
    last = ar_append_tail(S, last, va_arg(args, ar_Value*));
  }
  va_end(args);
  return res;
}


ar_Value *ar_new_number(ar_State *S, long double n) {
  ar_Value *res = new_value(S, AR_TNUMBER);
  res->u.num.n = n;
  return res;
}


ar_Value *ar_new_udata(ar_State *S, void *ptr, ar_CFunc gc, ar_CFunc mark) {
  ar_Value *res = new_value(S, AR_TUDATA);
  res->u.udata.ptr = ptr;
  res->u.udata.gc = gc;
  res->u.udata.mark = mark;
  return res;
}


ar_Value *ar_new_stringl(ar_State *S, const char *str, size_t len) {
  ar_Value *v = new_value(S, AR_TSTRING);
  v->u.str.s = NULL;
  v->u.str.s = ar_alloc(S, NULL, len + 1);
  v->u.str.s[len] = '\0';
  if (str) {
    memcpy(v->u.str.s, str, len);
  }
  v->u.str.len = len;
  return v;
}


ar_Value *ar_new_stringf(ar_State *S, const char *str, ...) {
  va_list arg, tmp;
  ar_Value *v;
  int len;
  if (str == NULL) return NULL;
  va_start(arg, str);
  v = ar_new_stringl(S, NULL, 0);
  /* create a copy of the list of args */
  __va_copy(tmp, arg);
  /* get length string should be */
  len = vsnprintf(v->u.str.s, 0, str, tmp);
  /* toss temp copy */
  va_end(tmp);
  /* something is wrong... */
  if (len < 0) return NULL;
  /* resize the string */
  v->u.str.s = ar_alloc(S, v->u.str.s, len + 1);
  v->u.str.s[len] = '\0';
  v->u.str.len = len;
  /* format the string string in ar_Value *v */
  vsnprintf(v->u.str.s, len + 1, str, arg);
  /* toss args */
  va_end(arg);
  return v;
}


ar_Value *ar_new_string(ar_State *S, const char *str) {
  if (str == NULL) return NULL;
  return ar_new_stringl(S, str, strlen(str));
}


ar_Value *ar_new_symbol(ar_State *S, const char *name) {
  ar_Value *v;
  /* Build hash of string */
  unsigned hash = 0;
  const char *p = name;
  while (*p) hash ^= (hash << 5) + (hash >> 2) + *p++;
  /* Create and init symbol */
  v = ar_new_string(S, name);
  v->type = AR_TSYMBOL;
  v->u.str.hash = hash;
  return v;
}


ar_Value *ar_new_cfunc(ar_State *S, ar_CFunc fn) {
  ar_Value *v = new_value(S, AR_TCFUNC);
  v->u.cfunc.fn = fn;
  return v;
}


ar_Value *ar_new_prim(ar_State *S, ar_Prim fn) {
  ar_Value *v = new_value(S, AR_TPRIM);
  v->u.prim.fn = fn;
  return v;
}


int ar_type(ar_Value *v) {
  return v ? v->type : AR_TNIL;
}


const char *ar_type_str(int type) {
  switch (type) {
    case AR_TNIL    : return "nil";
    case AR_TPAIR   : return "pair";
    case AR_TNUMBER : return "number";
    case AR_TSTRING : return "string";
    case AR_TSYMBOL : return "symbol";
    case AR_TFUNC   : return "function";
    case AR_TMACRO  : return "macro";
    case AR_TPRIM   : return "primitive";
    case AR_TCFUNC  : return "cfunction";
    case AR_TENV    : return "env";
    case AR_TUDATA  : return "udata";
  }
  return "?";
}


ar_Value *ar_check(ar_State *S, ar_Value *v, int type) {
  if (ar_type(v) != type) {
    ar_error_str(S, "expected %s, got %s",
                 ar_type_str(type), ar_type_str(ar_type(v)));
  }
  return v;
}


ar_Value *ar_car(ar_Value *v) {
  return (ar_type(v) == AR_TPAIR) ? v->u.pair.car : v;
}


ar_Value *ar_cdr(ar_Value *v) {
  return (ar_type(v) == AR_TPAIR) ? v->u.pair.cdr : NULL;
}


ar_Value *ar_nth(ar_Value *v, int idx) {
  while (v) {
    if (idx-- == 0) return ar_car(v);
    v = ar_cdr(v);
  }
  return NULL;
}


ar_Value **ar_append_tail(ar_State *S, ar_Value **last, ar_Value *v) {
  *last = ar_new_pair(S, v, NULL);
  return &(*last)->u.pair.cdr;
}


static ar_Value *join_list_of_strings(ar_State *S, ar_Value *list) {
  ar_Value *res;
  /* Get combined length of strings */
  ar_Value *v = list;
  size_t len = 0;
  while (v) {
    len += v->u.pair.car->u.str.len;
    v = v->u.pair.cdr;
  }
  /* Join list of strings */
  res = ar_new_stringl(S, NULL, len);
  v = list;
  len = 0;
  while (v) {
    ar_Value *x = v->u.pair.car;
    memcpy(res->u.str.s + len, x->u.str.s, x->u.str.len);
    len += x->u.str.len;
    v = v->u.pair.cdr;
  }
  return res;
}


static int escape_char(int chr) {
  switch (chr) {
    case '\t' : return 't';
    case '\n' : return 'n';
    case '\r' : return 'r';
    case '\\' :
    case '"'  : return chr;
  }
  return 0;
}


ar_Value *ar_to_string_value(ar_State *S, ar_Value *v, int quotestr) {
  ar_Value *res, **last;
  char buf[128];
  char *p, *q;
  size_t len, sz;
  switch (ar_type(v)) {
    case AR_TNIL:
      return ar_new_string(S, "nil");

    case AR_TSYMBOL:
      return ar_new_string(S, v->u.str.s);

    case AR_TPAIR:
      /* Handle empty pair */
      if (!ar_car(v) && !ar_cdr(v)) {
        return ar_new_string(S, "()");
      }
      /* Build list of strings */
      res = NULL;
      last = ar_append_tail(S, &res, ar_new_string(S, "("));
      while (v) {
        if (v->type == AR_TPAIR) {
          last = ar_append_tail(S, last, ar_to_string_value(S, ar_car(v), 1));
          if (ar_cdr(v)) {
            last = ar_append_tail(S, last, ar_new_string(S, " "));
          }
        } else {
          last = ar_append_tail(S, last, ar_new_string(S, ". "));
          last = ar_append_tail(S, last, ar_to_string_value(S, v, 1));
        }
        v = ar_cdr(v);
      }
      last = ar_append_tail(S, last, ar_new_string(S, ")"));
      return join_list_of_strings(S, res);

    case AR_TNUMBER:
      sprintf(buf, "%.14Lg", v->u.num.n);
      return ar_new_string(S, buf);

    case AR_TSTRING:
      if (quotestr) {
        /* Get string length + escapes and quotes */
        len = 2;
        p = v->u.str.s;
        sz = v->u.str.len;
        while (sz--) {
          len += escape_char(*p++) ? 2 : 1;
        }
        /* Build quoted string */
        res = ar_new_stringl(S, NULL, len);
        p = v->u.str.s;
        sz = v->u.str.len;
        q = res->u.str.s;
        *q++ = '"';
        while (sz--) {
          if (escape_char(*p)) {
            *q++ = '\\';
            *q++ = escape_char(*p);
          } else {
            *q++ = *p;
          }
          p++;
        }
        *q = '"';
        return res;
      }
      return v;

    default:
      sprintf(buf, "[%s %p]", ar_type_str(ar_type(v)), (void*) v);
      return ar_new_string(S, buf);
  }
}


const char *ar_to_stringl(ar_State *S, ar_Value *v, size_t *len) {
  v = ar_to_string_value(S, v, 0);
  if (len) *len = v->u.str.len;
  return v->u.str.s;
}


const char *ar_to_string(ar_State *S, ar_Value *v) {
  return ar_to_stringl(S, v, NULL);
}


void *ar_to_udata(ar_State *S, ar_Value *v) {
  UNUSED(S);
  return (ar_type(v) == AR_TUDATA) ? v->u.udata.ptr : NULL;
}


long double ar_to_number(ar_State *S, ar_Value *v) {
  UNUSED(S);
  switch (ar_type(v)) {
    case AR_TNUMBER : return v->u.num.n;
    case AR_TSTRING : return strtod(v->u.str.s, NULL);
  }
  return 0;
}


#define OPT_FUNC(NAME, CTYPE, TYPE, FIELD)          \
  CTYPE NAME(ar_State *S, ar_Value *v, CTYPE def) { \
    if (!v) return def;                             \
    return ar_check(S, v, TYPE)->FIELD;             \
  }

OPT_FUNC( ar_opt_string,  const char*,  AR_TSTRING, u.str.s     )
OPT_FUNC( ar_opt_udata,   void*,        AR_TUDATA,  u.udata.ptr )
OPT_FUNC( ar_opt_number,  long double,  AR_TNUMBER, u.num.n     )


static int is_equal(ar_Value *v1, ar_Value *v2) {
  int v1type, v2type;
  if (v1 == v2) return 1;
  v1type = ar_type(v1);
  v2type = ar_type(v2);
  if (v1type != v2type) return 0;
  switch (v1type) {
    case AR_TNUMBER : return v1->u.num.n == v2->u.num.n;
    case AR_TSYMBOL :
    case AR_TSTRING : return (v1->u.str.len == v2->u.str.len) &&
                             !memcmp(v1->u.str.s, v2->u.str.s, v1->u.str.len);
  }
  return 0;
}


static ar_Value *debug_location(ar_State *S, ar_Value *v) {
  if (ar_type(v) != AR_TPAIR || !v->u.pair.dbg) {
    return ar_new_string(S, "?");
  }
  return join_list_of_strings(S, ar_new_list(S, 3,
    v->u.pair.dbg->u.dbg.name,
    ar_new_string(S, ":"),
    ar_to_string_value(S, ar_new_number(S, v->u.pair.dbg->u.dbg.line), 0)));
}


/*===========================================================================
 * Garbage collector
 *===========================================================================*/

static void gc_free(ar_State *S, ar_Value *v) {
  /* Deinit value */
  switch (v->type) {
    case AR_TSYMBOL:
    case AR_TSTRING:
      ar_free(S, v->u.str.s);
      break;
    case AR_TUDATA:
      if (v->u.udata.gc) v->u.udata.gc(S, v);
      break;
  }
  /* Set type to nil (ignored by GC) and add to dead values pool */
  v->type = AR_TNIL;
  v->u.pair.cdr = S->gc_pool;
  S->gc_pool = v;
}


static void gc_deinit(ar_State *S) {
  int i;
  ar_Chunk *c, *next;
  /* Free all values in all chunks and free the chunks themselves */
  c = S->gc_chunks;
  while (c) {
    next = c->next;
    for (i = 0; i < CHUNK_LEN; i++) {
      gc_free(S, c->values + i);
    }
    ar_free(S, c);
    c = next;
  }
  /* Free stack */
  ar_free(S, S->gc_stack);
}


void ar_mark(ar_State *S, ar_Value *v) {
begin:
  if ( !v || v->mark ) return;
  v->mark = 1;
  switch (v->type) {
    case AR_TDBGINFO:
      v = v->u.dbg.name;
      goto begin;
    case AR_TMAPNODE:
      ar_mark(S, v->u.map.pair);
      ar_mark(S, v->u.map.left);
      v = v->u.map.right;
      goto begin;
    case AR_TPAIR:
      ar_mark(S, v->u.pair.dbg);
      ar_mark(S, v->u.pair.car);
      v = v->u.pair.cdr;
      goto begin;
    case AR_TMACRO:
    case AR_TFUNC:
      ar_mark(S, v->u.func.params);
      ar_mark(S, v->u.func.body);
      v = v->u.func.env;
      goto begin;
    case AR_TENV:
      ar_mark(S, v->u.env.map);
      v = v->u.env.parent;
      goto begin;
    case AR_TUDATA:
      if (v->u.udata.mark) v->u.udata.mark(S, v);
      break;
  }
}


void ar_gc(ar_State *S) {
  int i, count;
  ar_Chunk *c;
  /* Mark roots */
  for (i = 0; i < S->gc_stack_idx; i++) ar_mark(S, S->gc_stack[i]);
  ar_mark(S, S->global);
  ar_mark(S, S->oom_error);
  ar_mark(S, S->oom_args);
  ar_mark(S, S->t);
  /* Sweep: free still-unmarked values, unmark and count remaining values */
  count = 0;
  c = S->gc_chunks;
  while (c) {
    for (i = 0; i < CHUNK_LEN; i++) {
      if (c->values[i].type != AR_TNIL) {
        if (!c->values[i].mark) {
          gc_free(S, c->values + i);
        } else {
          c->values[i].mark = 0;
          count++;
        }
      }
    }
    c = c->next;
  }
  /* Reset gc counter */
  S->gc_count = count;
}


/*===========================================================================
 * Environment
 *===========================================================================*/

static ar_Value *new_mapnode(ar_State *S, ar_Value *k, ar_Value *v) {
  /* The pair for the node is created *first* as this may trigger garbage
   * collection which expects all values to be in an intialised state */
  ar_Value *p = ar_new_pair(S, k, v);
  ar_Value *x = new_value(S, AR_TMAPNODE);
  x->u.map.left = x->u.map.right = NULL;
  x->u.map.pair = p;
  return x;
}


static ar_Value **get_map_ref(ar_Value **m, ar_Value *k) {
  unsigned h = k->u.str.hash;
  while (*m) {
    ar_Value *k2 = (*m)->u.map.pair->u.pair.car;
    if (k2->u.str.hash == h && is_equal(k, k2)) {
      return m;
    } else if (k2->u.str.hash < h) {
      m = &(*m)->u.map.left;
    } else {
      m = &(*m)->u.map.right;
    }
  }
  return m;
}


static ar_Value *get_bound_value(ar_Value *sym, ar_Value *env) {
  do {
    ar_Value *x = *get_map_ref(&env->u.env.map, sym);
    if (x) return x->u.map.pair->u.pair.cdr;
    env = env->u.env.parent;
  } while (env);
  return NULL;
}


ar_Value *ar_bind(ar_State *S, ar_Value *sym, ar_Value *v, ar_Value *env) {
  ar_Value **x = get_map_ref(&env->u.env.map, sym);
  if (*x) {
    (*x)->u.map.pair->u.pair.cdr = v;
  } else {
    *x = new_mapnode(S, sym, v);
  }
  return v;
}


ar_Value *ar_set(ar_State *S, ar_Value *sym, ar_Value *v, ar_Value *env) {
  for (;;) {
    ar_Value *x = *get_map_ref(&env->u.env.map, sym);
    if (x) return x->u.map.pair->u.pair.cdr = v;
    if (!env->u.env.parent) return ar_bind(S, sym, v, env);
    env = env->u.env.parent;
  }
}


/*===========================================================================
 * Parser
 *===========================================================================*/

#define WHITESPACE  " \n\t\r"
#define DELIMITER   (WHITESPACE "();")

static ar_Value parse_end;

static ar_Value *parse(ar_State *S, const char **str) {
  ar_Value *res, **last, *v;
  char buf[512];
  size_t i;
  char *q;
  const char *p = *str;

  /* Skip whitespace */
  while (*p && strchr(WHITESPACE, *p)) {
    if (*p++ == '\n') S->parse_line++;
  }

  switch (*p) {
    case '\0':
      return &parse_end;

    case '(':
      res = NULL;
      last = &res;
      *str = p + 1;
      while ((v = parse(S, str)) != &parse_end) {
        if (ar_type(v) == AR_TSYMBOL && !strcmp(v->u.str.s, ".")) {
          /* Handle dotted pair */
          *last = parse(S, str);
        } else {
          /* Handle proper pair */
          int first = !res;
          *last = ar_new_pair(S, v, NULL);
          if (first) {
            /* This is the first pair in the list, attach debug info */
            ar_Value *dbg = new_value(S, AR_TDBGINFO);
            dbg->u.dbg.name = S->parse_name;
            dbg->u.dbg.line = S->parse_line;
            (*last)->u.pair.dbg = dbg;
          }
          last = &(*last)->u.pair.cdr;
        }
      }
      return res ? res : ar_new_pair(S, NULL, NULL);

    case '\'':
      *str = p + 1;
      return ar_new_list(S, 2, ar_new_symbol(S, "quote"), parse(S, str));

    case ')':
      *str = p + 1;
      return &parse_end;

    case ';':
      *str = p + strcspn(p, "\n");
      return parse(S, str);

    case '.': case '-':
    case '1': case '2': case '3': case '4': case '5':
    case '6': case '7': case '8': case '9': case '0':
      res = ar_new_number(S, strtod(p, &q));
      /* Not a valid number? treat as symbol */
      if ( *q && !strchr(DELIMITER, *q) ) {
        goto parse_symbol;
      }
      break;

    case '"':
      /* Get string length */
      p++;
      *str = p;
      i = 0;
      while (*p && *p != '"') {
        if (*p == '\\') p++;
        i++, p++;
      }
      /* Copy string */
      res = ar_new_stringl(S, NULL, i);
      p = *str;
      q = res->u.str.s;
      while (*p && *p != '"') {
        if (*p == '\\') {
          switch (*(++p)) {
            case 'r' : { *q++ = '\r'; p++; continue; }
            case 'n' : { *q++ = '\n'; p++; continue; }
            case 't' : { *q++ = '\t'; p++; continue; }
          }
        }
        if (*p == '\n') S->parse_line++;
        *q++ = *p++;
      }
      *str = p;
      break;

    default:
parse_symbol:
      *str = p + strcspn(p, DELIMITER);
      i = *str - p;
      if (i >= sizeof(buf)) i = sizeof(buf) - 1;
      memcpy(buf, p, i);
      buf[i] = '\0';
      if (!strcmp(buf, "nil")) return NULL;
      return ar_new_symbol(S, buf);
  }

  *str = p + strcspn(p, DELIMITER);
  return res;
}


ar_Value *ar_parse(ar_State *S, const char *str, const char *name) {
  ar_Value *res;
  S->parse_name = ar_new_string(S, name ? name : "?");
  S->parse_line = 1;
  res = parse(S, &str);
  return (res == &parse_end) ? NULL : res;
}


/*===========================================================================
 * Eval
 *===========================================================================*/

static ar_Value *eval_list(ar_State *S, ar_Value *list, ar_Value *env) {
  ar_Value *res = NULL, **last = &res;
  while (list) {
    last = ar_append_tail(S, last, ar_eval(S, ar_car(list), env));
    list = ar_cdr(list);
  }
  return res;
}


static ar_Value *args_to_env(
  ar_State *S, ar_Value *params, ar_Value *args, ar_Value *env
) {
  ar_Value *e = ar_new_env(S, env);
  /* No params? */
  if (ar_car(params) == AR_TNIL) {
    return e;
  }
  /* Handle arg list */
  while (params) {
    /* Symbol instead of pair? Bind remaining args to symbol */
    if (ar_type(params) == AR_TSYMBOL) {
      ar_bind(S, params, args, e);
      return e;
    }
    /* Handle normal param */
    ar_bind(S, ar_car(params), ar_car(args), e);
    params = ar_cdr(params);
    args = ar_cdr(args);
  }
  return e;
}


static void push_frame(ar_State *S, ar_Frame *f, ar_Value *caller) {
  if (S->frame_idx == MAX_STACK) {
    ar_error_str(S, "call stack overflow");
  }
  S->frame_idx++;
  f->parent = S->frame;
  f->caller = caller;
  f->stack_idx = S->gc_stack_idx;
  f->err_env = NULL;
  S->frame = f;
}


static void pop_frame(ar_State *S, ar_Value *rtn) {
  S->gc_stack_idx = S->frame->stack_idx;
  S->frame = S->frame->parent;
  S->frame_idx--;
  /* Reached the base frame? Clear protected-value-stack of all values */
  if (S->frame == &S->base_frame) S->gc_stack_idx = 0;
  if (rtn) push_value_to_stack(S, rtn);
}


static ar_Value *raw_call(
  ar_State *S, ar_Value *caller, ar_Value *fn, ar_Value *args, ar_Value *env
) {
  ar_Value *e, *res;
  ar_Frame frame;
  push_frame(S, &frame, caller);

  switch (ar_type(fn)) {
    case AR_TCFUNC:
      res = fn->u.cfunc.fn(S, args);
      break;

    case AR_TPRIM:
      res = fn->u.prim.fn(S, args, env);
      break;

    case AR_TFUNC:
      e = args_to_env(S, fn->u.func.params, args, fn->u.func.env);
      res = ar_do_list(S, fn->u.func.body, e);
      break;

    case AR_TMACRO:
      e = args_to_env(S, fn->u.func.params, args, fn->u.func.env);
      res = ar_eval(S, ar_do_list(S, fn->u.func.body, e), env);
      break;

    default:
      ar_error_str(S, "expected primitive, function or macro; got %s",
                   ar_type_str(ar_type(fn)));
      res = NULL;
  }
  pop_frame(S, res);
  return res;
}


ar_Value *ar_eval(ar_State *S, ar_Value *v, ar_Value *env) {
  ar_Value *fn, *args;

  switch (ar_type(v)) {
    case AR_TPAIR   : break;
    case AR_TSYMBOL : return get_bound_value(v, env);
    default         : return v;
  }

  fn = ar_eval(S, v->u.pair.car, env);
  switch (ar_type(fn)) {
    case AR_TCFUNC  :
    case AR_TFUNC   : args = eval_list(S, v->u.pair.cdr, env);  break;
    default         : args = v->u.pair.cdr;                     break;
  }
  return raw_call(S, v, fn, args, env);
}


ar_Value *ar_call(ar_State *S, ar_Value *fn, ar_Value *args) {
  int t = ar_type(fn);
  if (t != AR_TFUNC && t != AR_TCFUNC) {
    ar_error_str(S, "expected function got %s", ar_type_str(t));
  }
  return raw_call(S, ar_new_pair(S, fn, args), fn, args, NULL);
}


ar_Value *ar_do_list(ar_State *S, ar_Value *body, ar_Value *env) {
  ar_Value *res = NULL;
  while (body) {
    res = ar_eval(S, ar_car(body), env);
    body = ar_cdr(body);
  }
  return res;
}


ar_Value *ar_do_string(ar_State *S, const char *str) {
  return ar_eval(S, ar_parse(S, str, "(string)"), S->global);
}


ar_Value *ar_do_file(ar_State *S, const char *filename) {
  ar_Value *args = ar_new_list(S, 1, ar_new_string(S, filename));
  ar_Value *str = ar_call_global(S, "loads", args);
  return ar_eval(S, ar_parse(S, str->u.str.s, filename), S->global);
}


/*===========================================================================
 * Dynamic library loading
 *===========================================================================*/

 #if __GNUC__
   #define cast_func(t, p) (__extension__ (t) (p))
 #else
   #define cast_func(t, p) ((t) (p))
 #endif

#ifdef AR_DL_DLOPEN
  #define _GNU_SOURCE
  #include <dlfcn.h>

  void ar_lib_close(ar_State *S, ar_Lib *lib) {
    UNUSED(S);
    dlclose(lib->data);
  }

  ar_Lib *ar_lib_load(ar_State *S, const char *path, int global) {
    ar_Lib *l, *lib;
    void *data;
    /* Check if library has already been loaded */
    l = S->libs;
    while (l) {
      if (!strcmp(l->name, path)) return NULL;
      l = l->next;
    }
    /* Create and name library */
    lib = ar_alloc(S, NULL, sizeof(*lib));
    lib->name = basename(path);
    /* Open the library */
    data = dlopen(path, RTLD_NOW | (global ? RTLD_GLOBAL : RTLD_LOCAL));
    if (!data || data == NULL) {
      ar_free(S, lib);
      return NULL;
    }
    lib->data = data;
    /* Add library to library list and return it */
    lib->next = S->libs; S->libs = lib;
    return lib;
  }

  ar_CFunc ar_lib_sym(ar_State *S, ar_Lib *lib, const char *sym) {
    char *err;
    ar_CFunc fn;
    dlerror();
    fn = cast_func(ar_CFunc, dlsym(lib->data, sym));
    if ((err = dlerror()) != NULL) ar_error_str(S, err);
    return fn;
  }

#elif AR_DL_DLL
  #include <windows.h>
  /* Optional flags for LoadLibraryEx */
  #ifndef AR_LLE_FLAGS
  #define AR_LLE_FLAGS 0
  #endif

  static void pusherror (ar_State *S) {
    int error;
    char buffer[128];
    error = GetLastError();
    if (FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL, error, 0, buffer, sizeof(buffer)/sizeof(char), NULL))
      ar_error_str(S, buffer);
    else
      ar_error_str(S, "system error %d", error);
  }

  void ar_lib_close(ar_State *S, ar_Lib *lib) {
    UNUSED(S);
    FreeLibrary((HMODULE) lib->data);
  }

  ar_Lib *ar_lib_load(ar_State *S, const char *path, int global) {
    HMODULE data;
    ar_Lib *l, *lib;
    UNUSED(global);
    /* Check if library has already been loaded */
    l = S->libs;
    while (l) {
      if (!strcmp(l->name, path)) return NULL;
      l = l->next;
    }
    lib = ar_alloc(S, NULL, sizeof(*lib));
    lib->name = basename(path);
    /* Opening the library */
    data = LoadLibraryEx(path, NULL, AR_LLE_FLAGS);
    if (!data || data == NULL) {
      ar_free(S, lib);
      return NULL;
    }
    lib->data = data;
    /* Add library to library list and return it */
    lib->next = S->libs; S->libs = lib;
    return lib;
  }

  ar_CFunc ar_lib_sym(ar_State *S, ar_Lib *lib, const char *sym) {
    ar_CFunc fn;
    fn = cast_func(ar_CFunc, GetProcAddress((HMODULE)lib->data, sym));
    if (!fn || fn == NULL) pusherror(S);
    return fn;
  }

#else

  #define DLMSG "dynamic libraries not enabled; check your installation"

  void ar_lib_close(ar_State *S, ar_Lib *lib) {
    UNUSED(lib); UNUSED(S);
    ar_error_str(S, DLMSG);
  }

  ar_Lib *ar_lib_load(ar_State *S, const char *path, int global) {
    UNUSED(path); UNUSED(global);
    ar_error_str(S, DLMSG);
    return NULL;
  }

  ar_CFunc ar_lib_sym(ar_State *S, ar_Lib *lib, const char *sym) {
    UNUSED(lib); UNUSED(sym);
    ar_error_str(S, DLMSG);
    return NULL;
  }

#endif


/*===========================================================================
 * State
 *===========================================================================*/

static void *alloc_(void *udata, void *ptr, size_t size) {
  UNUSED(udata);
  if (size == 0) {
    dmt_free(ptr);
    return NULL;
  }
  if (ptr) return dmt_realloc(ptr, size);
  return dmt_calloc(1, size);
}


#include "builtin.h"

ar_State *ar_new_state(ar_Alloc alloc, void *udata) {
  ar_State *volatile S;
  if (!alloc) {
    alloc = alloc_;
  }
  S = alloc(udata, NULL, sizeof(*S));
  if (!S) return NULL;
  memset(S, 0, sizeof(*S));
  S->alloc = alloc;
  S->udata = udata;
  S->frame = &S->base_frame;  
  /* We use the ar_try macro in case an out-of-memory error occurs -- you
   * shouldn't usually return from inside the ar_try macro */
  ar_try(S, err, {
    /* Init global env; add constants, primitives and funcs */
    S->global = ar_new_env(S, NULL);
    S->oom_error = ar_new_string(S, "out of memory");
    S->oom_args = ar_new_pair(S, S->oom_error, NULL);
    S->t = ar_new_symbol(S, "t");
    ar_bind(S, S->t, S->t, S->global);
    ar_bind_global(S, "global", S->global);
    register_builtin(S);
  }, {
    UNUSED(err);
    ar_close_state(S);
    return NULL;
  });
  return S;
}


void ar_close_state(ar_State *S) {
  ar_Lib *lib, *next;
  gc_deinit(S);
  /* Close all open libaries */
  lib = S->libs;
  while (lib) {
    next = lib->next;
    ar_lib_close(S, lib);
    ar_free(S, lib);
    lib = next;
  }
  ar_free(S, S);
}


ar_CFunc ar_at_panic(ar_State *S, ar_CFunc fn) {
  ar_CFunc old = S->panic;
  S->panic = fn;
  return old;
}


static ar_Value *traceback(ar_State *S, ar_Frame *until) {
  ar_Value *res = NULL, **last = &res;
  ar_Frame *f = S->frame;
  while (f != until) {
    last = ar_append_tail(S, last, f->caller);
    f = f->parent;
  }
  return res;
}


void ar_error(ar_State *S, ar_Value *err) {
  ar_Frame *f;
  ar_Value *args;
  /* Create arguments to pass to error handler */
  if (err == S->oom_error) {
    args = S->oom_args;
  } else {
    /* String error? Add debug location string to start */
    if (ar_type(err) == AR_TSTRING) {
      err = join_list_of_strings(S, ar_new_list(S, 3,
        debug_location(S, S->frame->caller),
        ar_new_string(S, ": "),
        err));
    }
    args = ar_new_list(S, 2, err, NULL);
  }
  /* Unwind stack, create traceback list and jump to error env if it exists */
  f = S->frame;
  while (f) {
    if (f->err_env) {
      if (err != S->oom_error) {
        ar_cdr(args)->u.pair.car = traceback(S, f);
      }
      S->err_args = args;
      while (S->frame != f) pop_frame(S, args);
      if (err == S->oom_error) ar_gc(S);
      longjmp(*f->err_env, -1);
    }
    f = f->parent;
  }
  /* No error env found -- if we have a panic callback we unwind the stack and
   * call it else the error and traceback is printed */
  if (S->panic) {
    while (S->frame != &S->base_frame) pop_frame(S, args);
    S->panic(S, args);
  } else {
    printf("error: %s\n", ar_to_string(S, err));
    if (err != S->oom_error) {
      ar_Value *v = traceback(S, &S->base_frame);
      printf("traceback:\n");
      while (v) {
        printf("  [%s] %.50s\n", ar_to_string(S, debug_location(S, ar_car(v))),
                                 ar_to_string(S, ar_car(v)));
        v = ar_cdr(v);
      }
    }
  }
  exit(EXIT_FAILURE);
}


void ar_error_str(ar_State *S, const char *fmt, ...) {
  char buf[512];
  va_list args;
  va_start(args, fmt);
  vsprintf(buf, fmt, args);
  va_end(args);
  ar_error(S, ar_new_string(S, buf));
}


/*===========================================================================
 * Standalone
 *===========================================================================*/

#ifdef AR_STANDALONE
// #include "lib/commander/commander.h"

char *line;
ar_State *S;

#ifndef _WIN32
#include "lib/linenoise/linenoise.h"

static ar_Value *f_readline(ar_State *S, ar_Value *args) {
  UNUSED(args);
  line = linenoise("> ");
  if (!line) ar_do_string(S, "(exit)");
  linenoiseHistoryAdd(line);
  return ar_new_string(S, line);
}
#else
static ar_Value *f_readline(ar_State *S, ar_Value *args) {
  char buf[4096];
  UNUSED(args);
  printf("> ");
  return ar_new_string(S, fgets(buf, sizeof(buf) - 1, stdin));
}
#endif


static void shut_down(void) {
  FILE *file;
  ar_close_state(S);
  free(line);
#ifdef DEBUG
  file = fopen("memory.log", "wb");
  dmt_dump(file);
  fclose(file);
#else
  UNUSED(file);
#endif
}

#define HELP_TEXT \
"Usage: aria [options]... [file [args]...].\n"\
"options:\n"\
"    -v  show version information\n"\
"    --  execute stdin and stop handling options\n"


int main(int argc, char **argv) {
  atexit(shut_down);
  S = ar_new_state(NULL, NULL);
  if (!S) {
    printf("out of memory\n");
    return EXIT_FAILURE;
  }
  /* Enable single line buffering for Windows */
#if _WIN32
  setvbuf(stdout, NULL, _IONBF, BUFSIZ);
#endif

  /* Embed standard library */
  #include "core_lsp.h"
  #include "class_lsp.h"

  struct { const char *name, *data; } items[] = {
    { "core.lsp",  core_lsp  },
    { "class.lsp", class_lsp },
    { NULL, NULL }
  };
  int i;
  for (i = 0; items[i].name; i++) {
    ar_eval(S, ar_parse(S, items[i].data, items[i].name), S->global);
  }
  if (argc < 2) {
    /* Init REPL */
    ar_bind_global(S, "readline", ar_new_cfunc(S, f_readline));
    #include "repl_lsp.h"
    ar_do_string(S, repl_lsp);
  } else {
    int i;

    char *p = argv[1];

    if (*(p++) == '-') {
      switch (*p) {
        case 'v': {
          fprintf(stdout, "aria verson %s\n", AR_VERSION); return EXIT_SUCCESS;
        }
        case '-': {
          ar_Value *v = NULL, **last = &v;
          long nr = 0;
          do {
            char *str = calloc(BUFSIZ, sizeof(*str));
            nr = fread(str, sizeof(char), BUFSIZ, stdin);
            last = ar_append_tail(S, last, ar_new_string(S, str));
            free(str - nr);
          } while (nr ==  BUFSIZ);
          ar_do_string(S, join_list_of_strings(S, v)->u.str.s);
          return EXIT_SUCCESS;
          }
        }
        fprintf(stdout, HELP_TEXT);
        return EXIT_SUCCESS;
    } else {  
      /* Store arguments at global list `argv` */
      ar_Value *v = NULL, **last = &v;
      for (i = 1; i < argc; i++) {
        last = ar_append_tail(S, last, ar_new_string(S, argv[i]));
      }
      ar_bind_global(S, "argv", v);
      /* Load and do file from argv[1] */
      ar_do_file(S, argv[1]);
    }
  }
  return EXIT_SUCCESS;
}

#endif
