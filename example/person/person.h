/**
 * Copyright (c) 2017 emekoi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */


#ifndef DLL_H
#define DLL_H

#include "../../src/aria.h"

typedef struct {
  char *name; double age;
} person_t;

static ar_Value *person_new(ar_State *S, ar_Value* args);
static ar_Value *person_name(ar_State *S, ar_Value* args);
static ar_Value *person_gc(ar_State *S, ar_Value* args);

#endif
