/**
 * Copyright (c) 2017 emekoi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */


#include <stdlib.h>
#include <stdio.h>
#include "person.h"
#include "../../src/aria.h"

#define AR_DEFINE(func) static ar_Value *func(ar_State *S, ar_Value* args)

AR_DEFINE(TCC_NEW);
AR_DEFINE(TCC_GC);

ar_Value *ar_open_person(ar_State *S, ar_Value* args) {
  /* list of functions to register */
  struct { const char *name; ar_CFunc fn; } funcs[] = {
        { "person", person_new  },
        { "name",   person_name },
        { "age",    person_age  },
    { NULL, NULL }
  };
  /* register functions */
  for (int i = 0; funcs[i].name; i++) {
    ar_bind_global(S, funcs[i].name, ar_new_cfunc(S, funcs[i].fn));
  }
    return NULL;
}


static ar_Value *person_new(ar_State *S, ar_Value* args) {
    ar_Value *ptr; TCCState *TCC;

    ptr = ar_new_udata(S, TCC, TCC_GC, NULL);
    return ptr;
}


static ar_Value *person_gc(ar_State *S, ar_Value* args) {
    TCCState *TCC = ar_check_udata(S, ar_car(args));
    if (TCC) tcc_delete(TCC);
    return NULL;
}
