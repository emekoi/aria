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


static ar_Value *person_new(ar_State *S, ar_Value* args) {
	ar_Value *ptr; person_t *p;
	ar_Value *n = ar_car(args);
	char *name = (n ? (char *)ar_to_string(S, n) : "a child");
	printf("%s has been born.\n", name);
	p = calloc(1, sizeof(*p));
	int len = strlen(name);
	p->name = realloc(p->name, sizeof(char) * len + 1);
	memcpy(p->name, name, len);
	p->name[len] = '\0';
	p->age = 0;

	ptr = ar_new_udata(S, p, person_gc, NULL);
	return ptr;
}

static ar_Value *person_name(ar_State *S, ar_Value* args) {
	person_t *p = ar_check_udata(S, ar_nth(args, 0));
	char *name = (char *)ar_to_string(S, ar_nth(args, 1));
	if (p->name == name) printf("the child has been named %s.\n", name);
	else printf("%s now goes by %s.\n", p->name, name);
	int len = strlen(name);
	p->name = realloc(p->name, sizeof(char) * len + 1);
	memcpy(p->name, name, len); p->name[len] = '\0';
	return NULL;
}


static ar_Value *person_age(ar_State *S, ar_Value* args) {
	person_t *p = ar_check_udata(S, ar_nth(args, 0));
	double age = ar_to_number(S, ar_nth(args, 1));
	printf("today %s is %.0f %s older than when we last saw them.\n", p->name, age, (age == 1 ? "year" : "years"));
	p->age += age;
	return NULL;
}


static ar_Value *person_gc(ar_State *S, ar_Value* args) {
	person_t *p = ar_check_udata(S, ar_car(args));
	printf("%s died at the age of %.0f.\n", p->name, p->age);
	free(p->name); free(p); return NULL;
}


ar_Value *ar_open_person(ar_State *S, ar_Value* args) {
  /* list of functions to register */
  struct { const char *name; ar_CFunc fn; } funcs[] = {
		{ "person", person_new  },
		{ "name",  	person_name },
		{ "age",  	person_age  },
    { NULL, NULL }
  };
  /* register functions */
  for (int i = 0; funcs[i].name; i++) {
    ar_bind_global(S, funcs[i].name, ar_new_cfunc(S, funcs[i].fn));
  }
	return NULL;
}
