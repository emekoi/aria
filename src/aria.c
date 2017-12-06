/**
 * Copyright (c) 2017 emekoi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include "util.h"
#include "aria.h"

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

#ifdef _WIN32

struct { const char *path; uchar local; } ar_SearchPaths[] = {
  { "/usr/local/share/aria/" AR_VERSION "/%s.lsp", 0 },
  { "/usr/local/lib/aria/" AR_VERSION "/%s.dll",   0 },
  { "%s/%s.dll",                                   1 },
  { "%s/%s.lsp",                                   1 },
  { NULL,                                          0 }
};

#else

struct { const char *path; uchar local; } ar_SearchPaths[] = {
  { "/usr/local/share/aria/" AR_VERSION "/%s.lsp", 0 },
  { "/usr/local/lib/aria/" AR_VERSION "/%s.so",    0 },
  { "%s/%s.so",                                    1 },
  { "%s/%s.lsp",                                   1 },
  { NULL,                                          0 }
};

#endif

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
    if (!data || data == NULL) return NULL;
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
    UNUSED(global);
    HMODULE data;
    ar_Lib *l, *lib;
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
    if (!data || data == NULL) return NULL;
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


#ifdef _WIN32

char *getcwd(char *buf, int len) {
  UNUSED(buf); len = GetCurrentDirectory(0, NULL);
  char *str = calloc(1, sizeof(char) * len);
  GetCurrentDirectory(len, str);
  return str;
}

#endif

static ar_Value *p_import(ar_State *S, ar_Value *args, ar_Value *env) {
  ar_Value *res = ar_check(S, ar_eval(S, ar_nth(args, 0), env), AR_TSTRING);
  int i;
  for (i = 0; ar_nth(args, i); i++) {
    size_t found = 0;
    /* Check all search paths */
    size_t j;
    for (j = 0; ar_SearchPaths[j].path; j++) {
      char *r, cwd[1024];
      ar_Lib *lib;
      ar_CFunc open_lib;
      /* Check for C libaries */
      char path[4096];
      if (ar_SearchPaths[j].local) {
        sprintf(path, ar_SearchPaths[j].path, getcwd(cwd, sizeof(cwd)), skipDotSlash(res->u.str.s));
      } else {
        sprintf(path, ar_SearchPaths[j].path, skipDotSlash(res->u.str.s));
      }
      lib = ar_lib_load(S, path, 1);
      if (lib) {
        /* Try to run the library's open function */
        r = concat(AR_OFN, strtok(lib->name, "."), NULL);
        open_lib = ar_lib_sym(S, lib, r);
        open_lib(S, NULL); free(r);
        found = 1;
        break;
      }
      /* Check for aria libraries */
      ar_try(S, err, {
        if (ar_do_file(S, path)) found = 1;
      }, {
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
  UNUSED(args); ar_gc(S);
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
  clock_t target = clock() / CLOCKS_PER_SEC + ar_to_number(S, (ar_nth(args, 0)));
  while (clock() / CLOCKS_PER_SEC != target);
  return NULL;
}


static ar_Value *f_exit(ar_State *S, ar_Value *args) {
  exit(ar_opt_number(S, ar_car(args), EXIT_SUCCESS));
  return NULL;
}


static void register_builtin(ar_State *S) {
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
    { "deg",      f_deg     },
    { "exp",      f_exp     },
    { "floor",    f_floor   },
    { "log",      f_log     },
    { "modf",     f_modf    },
    { "rad",      f_rad     },
    { "sin",      f_sin     },
    { "sqrt",     f_sqrt    },
    { "tan",      f_tan     },
    { "now",      f_now     },
    { "clock",    f_clock   },
    { "sleep",    f_sleep   },
    { "exit",     f_exit    },
    { NULL,       NULL      }
  };
  /* String Globals */
  struct { const char *name; void *val; } str_globals[] = {
    { "VERSION", AR_VERSION },
    { NULL,      NULL       }
  };
  /* Math Globals */
  struct { const char *name; double val; } num_globals[] = {
    { "math-huge",  HUGE_VAL  },
    { "-math-huge", -HUGE_VAL },
    { "math-pi",    PI        },
    { "-math-pi",   -PI       },
    { NULL,         0         }
  };
  /* Register */
  for (i = 0; prims[i].name; i++) {
    ar_bind_global(S, prims[i].name, ar_new_prim(S, prims[i].fn));
  }
  for (i = 0; funcs[i].name; i++) {
    ar_bind_global(S, funcs[i].name, ar_new_cfunc(S, funcs[i].fn));
  }
  for (i = 0; str_globals[i].name; i++) {
    ar_bind_global(S, str_globals[i].name, ar_new_string(S, str_globals[i].val));
  }
  for (i = 0; num_globals[i].name; i++) {
    ar_bind_global(S, num_globals[i].name, ar_new_number(S, num_globals[i].val));
  }
}


/*===========================================================================
 * State
 *===========================================================================*/

static void *alloc_(void *udata, void *ptr, size_t size) {
  UNUSED(udata);
  if (ptr && size == 0) {
    free(ptr);
    return NULL;
  }
  return realloc(ptr, size);
}


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
  ar_close_state(S);
  free(line);
}


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
    /* Store arguments at global list `argv` */
    ar_Value *v = NULL, **last = &v;
    for (i = 1; i < argc; i++) {
      last = ar_append_tail(S, last, ar_new_string(S, argv[i]));
    }
    ar_bind_global(S, "argv", v);
    /* Load and do file from argv[1] */
    ar_do_file(S, argv[1]);
  }
  return EXIT_SUCCESS;
}

#endif
