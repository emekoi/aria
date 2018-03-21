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
#include "dmt/dmt.h"

#define UNUSED(x) ((void) x)

static char *concat(const char *str, ...) {
  int len;
  char *res;
  va_list args;
  const char *s;
  /* Get len */
  len = strlen(str);
  va_start(args, str);
  while ((s = va_arg(args, char*))) {
    len += strlen(s);
  }
  va_end(args);
  /* Build string */
  res = dmt_malloc(len + 1);
  if (!res) return NULL;
  strcpy(res, str);
  va_start(args, str);
  while ((s = va_arg(args, char*))) {
    strcat(res, s);
  }
  va_end(args);
  return res;
}


static char *baseName(const char *str) {
  char *s, *p, *file;
  s = concat("", str, NULL);
  p = s + strlen(s);
  file = "";
  while (p != s) {
    if (*p == '/' || *p == '\\') {
      UNUSED(*p++);
      file = p;
      break;
    }
    p--;
  }
  dmt_free(s);
  return file;
}


static int isSeparator(int chr) {
  return (chr == '/' || chr == '\\');
}


static const char *skipDotSlash(const char *filename) {
  if (filename[0] == '.' && isSeparator(filename[1])) {
    return filename + 2;
  }
  return filename;
}


static int is_alpha(char c) {
  return (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') ||
         (c == '_');
}

#endif
