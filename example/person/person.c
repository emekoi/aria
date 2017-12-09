/**
 * Copyright (c) 2017 emekoi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */


#include <stdlib.h>
#include <stdio.h>
// #include <aria/aria.h>
#include "aria.h"

#define UNUSED(x) ((void) x)

typedef struct {
  char *name; double age;
} person_t;


static ar_Value *person_gc(ar_State *S, ar_Value* args) {
  person_t *p = ar_check_udata(S, ar_car(args));
  printf("%s died at the age of %.0f.\n", p->name, p->age);
  ar_free(S, p->name); ar_free(S, p); return NULL;
}


static ar_Value *person_new(ar_State *S, ar_Value* args) {
  ar_Value *ptr; person_t *p;
  ar_Value *n = ar_car(args);
  char *name = (n ? (char *)ar_to_string(S, ar_check(S, n, AR_TSTRING)) : "a child");
  printf("%s has been born.\n", name);
  p = ar_alloc(S, NULL, sizeof(*p));
  int len = strlen(name);
  p->name = ar_alloc(S, p->name, sizeof(char) * len + 1);
  memcpy(p->name, name, len);
  p->name[len] = '\0';
  p->age = 0;

  ptr = ar_new_udata(S, p, person_gc, NULL);
  return ptr;
}


static ar_Value *person_name(ar_State *S, ar_Value* args) {
  person_t *p = ar_check_udata(S, ar_nth(args, 0));
  char *name = (char *)ar_to_string( S, ar_check(S, ar_nth(args, 1), AR_TSTRING));
  if (p->name == name) printf("the child has been named %s.\n", name);
  else printf("%s now goes by %s.\n", p->name, name);
  int len = strlen(name);
  p->name = ar_alloc(S, p->name, sizeof(char) * len + 1);
  memcpy(p->name, name, len); p->name[len] = '\0';
  return NULL;
}


static ar_Value *person_age(ar_State *S, ar_Value* args) {
  person_t *p = ar_check_udata(S, ar_nth(args, 0));
  double age = ar_to_number( S, ar_check(S, ar_nth(args, 1), AR_TNUMBER));
  printf("today %s is %.0f %s older than when we last saw them.\n", p->name, age, (age == 1 ? "year" : "years"));
  p->age += age;
  return NULL;
}


/* list of functions to register */
static const ar_Reg funcs[] = {
  { "person", person_new  },
  { "name",  	person_name },
  { "age",  	person_age  },
  { NULL,     NULL        }
};

ar_Value *ar_open_person(ar_State *S, ar_Value* env) {
  /* register functions */
  ar_lib_new(S, env, funcs);
	return NULL;
}
