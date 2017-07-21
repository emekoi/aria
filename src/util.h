/**
 * Copyright (c) 2017 emekoi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#define UNUSED(x) ((void) x)

char *concat(const char *str, ...);
char *basename(char *str);
int isSeparator(int chr);
const char *skipDotSlash(const char *filename);


#endif
