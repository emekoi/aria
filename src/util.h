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

static inline char *concat(const char *str, ...) {
  va_list args;
  const char *s;
  /* Get len */
  int len = strlen(str);
  va_start(args, str);
  while ((s = va_arg(args, char*))) {
    len += strlen(s);
  }
  va_end(args);
  /* Build string */
  char *res = malloc(len + 1);
  if (!res) return NULL;
  strcpy(res, str);
  va_start(args, str);
  while ((s = va_arg(args, char*))) {
    strcat(res, s);
  }
  va_end(args);
  return res;
}


static inline char *basename(char *str) {
  char *s = concat("", str, NULL);
  char *p = s + strlen(s);
  char *file = "";
  while (p != s) {
    if (*p == '/' || *p == '\\') {
      UNUSED(*p++);
      file = p;
      break;
    }
    p--;
  }
  return file;
}


static inline int isSeparator(int chr) {
  return (chr == '/' || chr == '\\');
}


static inline const char *skipDotSlash(const char *filename) {
  if (filename[0] == '.' && isSeparator(filename[1])) {
    return filename + 2;
  }
  return filename;
}

#endif
