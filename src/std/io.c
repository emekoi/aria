/**
 * Copyright (c) 2017 emekoi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

 /*===========================================================================
  * IO funcs
  *===========================================================================*/

#include "fs/fs.c"

#define UNUSED(x) ((void) x)
#define AR_GET_ARG(idx, type) S, ar_check(S, ar_nth(args, idx), type)
#define AR_GET_STRING(idx) (char *)ar_to_string(AR_GET_ARG(idx, AR_TSTRING))
#define AR_GET_STRINGL(idx, len) (char *)ar_to_stringl(AR_GET_ARG(idx, AR_TSTRING), &len)

static void checkError(ar_State *S, int err, const char *str) {
  if (!err) return;
  if (err == FS_ENOWRITEPATH || !str) {
    ar_error_str(S, "%s", fs_errorStr(err));
  }
  ar_error_str(S, "%s '%s'", fs_errorStr(err), str);
}


static ar_Value *f_mount(ar_State *S, ar_Value *args) {
  const char *path = AR_GET_STRING(0);
  int res = fs_mount(path);
  if (res != FS_ESUCCESS) {
    ar_error_str(S, "%s '%s'", fs_errorStr(res), path);
  }
  return S->t;
}


static ar_Value *f_unmount(ar_State *S, ar_Value *args) {
  const char *path = AR_GET_STRING(0);
  fs_unmount(path);
  return S->t;
}


static ar_Value *f_setWritePath(ar_State *S, ar_Value *args) {
  const char *path = AR_GET_STRING(0);
  int res = fs_setWritePath(path);
  checkError(S, res, path);
  return S->t;
}


static ar_Value *f_exists(ar_State *S, ar_Value *args) {
  const char *filename = AR_GET_STRING(0);
  return fs_exists(filename) ? S->t : NULL;
}


static ar_Value *f_getSize(ar_State *S, ar_Value *args) {
  const char *filename = AR_GET_STRING(0);
  size_t sz;
  int res = fs_size(filename, &sz);
  checkError(S, res, filename);
  return ar_new_number(S, sz);
}


static ar_Value *f_getModified(ar_State *S, ar_Value *args) {
  const char *filename = AR_GET_STRING(0);
  unsigned t;
  int res = fs_modified(filename, &t);
  checkError(S, res, filename);
  return ar_new_number(S, t);
}


static ar_Value *f_io_read(ar_State *S, ar_Value *args) {
  const char *filename = AR_GET_STRING(0);
  size_t len;
  char *data = fs_read(filename, &len);
  if (!data) {
    ar_error_str(S, "could not read file '%s'", filename);
  }
  ar_Value *res = ar_new_stringl(S, data, len);
  // ar_free(S, data); return res;
  free(data); return res;
}


static ar_Value *f_isDir(ar_State *S, ar_Value *args) {
  const char *filename = AR_GET_STRING(0);
  return fs_isDir(filename) ? S->t : NULL;
}


static ar_Value *f_isFile(ar_State *S, ar_Value *args) {
  const char *filename = AR_GET_STRING(0);
  return fs_isFile(filename) ? S->t : NULL;
}


static ar_Value *f_listDir(ar_State *S, ar_Value *args) {
  const char *path = AR_GET_STRING(0);
  fs_FileListNode *list = fs_listDir(path);
  ar_Value *res = NULL, **last = &res;
  fs_FileListNode *n = list;
  while (n) {
    last = ar_append_tail(S, last, ar_new_string(S, n->name));
    n = n->next;
  }
  fs_freeFileList(list);
  return res;
}


static ar_Value *f_write(ar_State *S, ar_Value *args) {
  const char *filename = AR_GET_STRING(0);
  size_t len;
  const char *data = AR_GET_STRINGL(1, len);
  int res = fs_write(filename, data, len);
  checkError(S, res, filename);
  return S->t;
}


static ar_Value *f_append(ar_State *S, ar_Value *args) {
  const char *filename = AR_GET_STRING(0);
  size_t len;
  const char *data = AR_GET_STRINGL(1, len);
  int res = fs_append(filename, data, len);
  checkError(S, res, filename);
  return S->t;
}


static ar_Value *f_delete(ar_State *S, ar_Value *args) {
  const char *filename = AR_GET_STRING(0);
  int res = fs_delete(filename);
  if (res != FS_ESUCCESS) {
    ar_error_str(S, "%s", fs_errorStr(res));
  }
  return S->t;
}


static ar_Value *f_makeDirs(ar_State *S, ar_Value *args) {
  const char *path = AR_GET_STRING(0);
  int res = fs_makeDirs(path);
  if (res != FS_ESUCCESS) {
    ar_error_str(S, "%s '%s'", fs_errorStr(res), path);
  }
  return S->t;
}


void register_io(ar_State *S) {
  /* list of functions to register */
  fs_mount(".");
  fs_setWritePath(".");
  struct { const char *name; ar_CFunc fn; } funcs[] = {
    { "io-mount",        f_mount        },
    { "io-unmount",      f_unmount      },
    { "io-setWritePath", f_setWritePath },
    { "io-exists",       f_exists       },
    { "io-getSize",      f_getSize      },
    { "io-getModified",  f_getModified  },
    { "io-read",         f_io_read      },
    { "io-isDir",        f_isDir        },
    { "io-isFile",       f_isFile       },
    { "io-listDir",      f_listDir      },
    { "io-write",        f_write        },
    { "io-append",       f_append       },
    { "io-delete",       f_delete       },
    { "io-makeDirs",     f_makeDirs     },
    { NULL, NULL }
  };

  /* register functions */
  for (int i = 0; funcs[i].name; i++) {
    ar_bind_global(S, funcs[i].name, ar_new_cfunc(S, funcs[i].fn));
  }
  atexit(fs_deinit);
}
