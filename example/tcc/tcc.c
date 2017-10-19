/**
 * Copyright (c) 2017 emekoi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libtcc.h>
#include "../../src/aria.h"

#define UNUSED(x) ((void) x)

static void *read_file(const char *filename, size_t *len) {
  size_t len_ = 0;
  if (!len) len = &len_;
  FILE *fp = fopen(filename, "rb");
  if (!fp) goto end;
  /* Get file size */
  fseek(fp, 0, SEEK_END);
  *len = ftell(fp);
  /* Load file */
  fseek(fp, 0, SEEK_SET);
  char *res = malloc(*len + 1);
  if (!res) return NULL;
  res[*len] = '\0';
  if (fread(res, 1, *len, fp) != *len) {
    free(res);
    fclose(fp);
    return NULL;
  } else {
    fclose(fp);
    return res;
  }
  end:
    return NULL;
}

static int isFile(const char *path) {
  struct stat s;
  int res = stat(path, &s);
  return (res == 0) && S_ISREG(s.st_mode);
}


#define AR_DEFINE(func) static ar_Value *func(ar_State *S, ar_Value* args)

AR_DEFINE(TCC_NEW);
AR_DEFINE(TCC_GET);
AR_DEFINE(TCC_GC);

static ar_Value *TCC_NEW(ar_State *S, ar_Value* args) {
  ar_Value *ptr; TCCState *TCC = tcc_new();
  if(!TCC) return NULL;
  
  tcc_set_output_type(TCC, TCC_OUTPUT_MEMORY);

  char *data = (char *)ar_to_string(S, ar_car(args));
  
  if (isFile(data))
    if (tcc_add_file(TCC, data) < 0) return NULL;
  else
    if (tcc_compile_string(TCC, data) < 0) return NULL;

  printf("lit fam %d\n", isFile(data));

  ptr = ar_new_udata(S, TCC, TCC_GC, NULL);
  return ptr;
}


static ar_Value *TCC_GET(ar_State *S, ar_Value* args) {
  TCCState *TCC = ar_check_udata(S, ar_car(args));
  if (tcc_relocate(TCC, TCC_RELOCATE_AUTO) < 0) return NULL;
  
  int (*func)(void) = tcc_get_symbol(TCC, "main_func");
  
  // causes segfault
  // func();

  return NULL;
}


static ar_Value *TCC_GC(ar_State *S, ar_Value* args) {
  TCCState *TCC = ar_check_udata(S, ar_car(args));
  if (TCC) tcc_delete(TCC);
  return NULL;
}


ar_Value *ar_open_tcc(ar_State *S, ar_Value* args) {
  UNUSED(args);
  /* list of functions to register */
  struct { const char *name; ar_CFunc fn; } funcs[] = {
         { "tcc-new", TCC_NEW  },
         { "tcc-get", TCC_GET  },
         { NULL, NULL }
  };
  /* register functions */
  for (int i = 0; funcs[i].name; i++) {
    ar_bind_global(S, funcs[i].name, ar_new_cfunc(S, funcs[i].fn));
  }
  return NULL;
}
