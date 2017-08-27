/**
 * Copyright (c) 2017 emekoi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */


#include <stdlib.h>
#include <stdio.h>
#include "../../src/aria.h"

#ifdef _WIN32
#define EXAMPLE_DLL __declspec(dllexport)
#endif

// person_t *person_new(char *name);
static ar_Value *person_new(ar_State *S, ar_Value* args) {
	char log[BUFSIZ];
	sprintf(log, "(print \"%s has been born.\")", ar_to_string(S, ar_nth(args, 0)));
	ar_do_string(S, log);
	return NULL;
}


ar_Value *ar_open_person(ar_State *S, ar_Value* args) {
  /* list of functions to register */
  struct { const char *name; ar_CFunc fn; } funcs[] = {
		{ "person",  person_new  },
    { NULL, NULL }
  };
  /* register functions */
  for (int i = 0; funcs[i].name; i++) {
    ar_bind_global(S, funcs[i].name, ar_new_cfunc(S, funcs[i].fn));
  }
	return NULL;
}
