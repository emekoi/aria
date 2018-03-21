/**
 * Copyright (c) 2017 emekoi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

 /*===========================================================================
  * IO funcs
  *===========================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <assert.h>
#include <stdarg.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

typedef struct fs_FileListNode {
  char *name;
  struct fs_FileListNode *next;
} fs_FileListNode;

enum {
  FS_ESUCCESS     = 0,
  FS_EFAILURE     = -1,
  FS_EOUTOFMEM    = -2,
  FS_EBADPATH     = -3,
  FS_EBADFILENAME = -4,
  FS_ENOWRITEPATH = -5,
  FS_ECANTOPEN    = -6,
  FS_ECANTREAD    = -7,
  FS_ECANTWRITE   = -8,
  FS_ECANTDELETE  = -9,
  FS_ECANTMKDIR   = -10,
  FS_ENOTEXIST    = -11,
};

const char *fs_errorStr(int err);
/* void fs_deinit(ar_State *S); */
int fs_mount(ar_State *S, const char *path);
int fs_unmount(ar_State *S, const char *path);
int fs_setWritePath(ar_State *S, const char *path);
int fs_exists(ar_State *S, const char *filename);
int fs_modified(ar_State *S, const char *filename, unsigned *mtime);
int fs_size(ar_State *S, const char *filename, size_t *size);
void *fs_read(ar_State *S, const char *filename, size_t *size);
int fs_isDir(ar_State *S, const char *filename);
int fs_isFile(ar_State *S, const char *filename);
fs_FileListNode *fs_listDir(ar_State *S, const char *path);
void fs_freeFileList(ar_State *S, fs_FileListNode *list);
int fs_write(ar_State *S, const char *filename, const void *data, int size);
int fs_append(ar_State *S, const char *filename, const void *data, int size);
int fs_delete(ar_State *S, const char *filename);
int fs_makeDirs(ar_State *S, const char *path);

#if _WIN32
  #define mkdir(path, mode) mkdir(path)
#endif

typedef struct PathNode {
  struct PathNode *next;
  char path[1];
} PathNode;

static PathNode *mounts;
static PathNode *writePath;

static int isDir(const char *path) {
  struct stat s;
  int res = stat(path, &s);
  return (res == FS_ESUCCESS) && S_ISDIR(s.st_mode);
}


static int makeDirs(ar_State *S, const char *path) {
  int err = FS_ESUCCESS;
  char *str = concat(S, path, "/", NULL);
  char *p = str;
  if (!str) {
    err = FS_EOUTOFMEM;
    goto end;
  }
  if (p[0] == '/') p++;
  if (p[0] && p[1] == ':' && p[2] == '\\') p += 3;
  while (*p) {
    if (isSeparator(*p)) {
      *p = '\0';
      if (!isDir(str)) {
        if (mkdir(str, S_IRWXU) == -1) {
          err = FS_ECANTMKDIR;
          goto end;
        }
      }
      *p = '/';
    }
    p++;
  }
end:
  ar_free(S, str);
  return err;
}


static PathNode *newNode(ar_State *S, const char *path) {
  int len = strlen(path);
  PathNode *p = ar_alloc(S, NULL, sizeof(*p) + len + 1);
  if (!p) return NULL;
  strcpy(p->path, path);
  /* Trim trailing path seperator */
  if (isSeparator(p->path[len - 1])) {
    p->path[len - 1] = '\0';
  }
  return p;
}


static void destroyNode(ar_State *S, PathNode *p) {
  ar_free(S, p);
}


static int checkFilename(const char *filename) {
  if (*filename == '/' || strstr(filename, "..") || strstr(filename, ":\\")) {
    return FS_EFAILURE;
  }
  return FS_ESUCCESS;
}


const char *fs_errorStr(int err) {
  switch(err) {
    case FS_ESUCCESS      : return "success";
    case FS_EFAILURE      : return "failure";
    case FS_EOUTOFMEM     : return "out of memory";
    case FS_EBADPATH      : return "bad path";
    case FS_EBADFILENAME  : return "bad filename";
    case FS_ENOWRITEPATH  : return "no write path set";
    case FS_ECANTOPEN     : return "could not open file";
    case FS_ECANTREAD     : return "could not read file";
    case FS_ECANTWRITE    : return "could not write file";
    case FS_ECANTDELETE   : return "could not delete file";
    case FS_ECANTMKDIR    : return "could not make directory";
    case FS_ENOTEXIST     : return "file or directory does not exist";
    default               : return "unknown error";
  }
}


/* the GC frees all resources at shutdown
 * so this sholdn't be needed
void fs_deinit(ar_State *S) {
  while (mounts) {
    PathNode *p = mounts->next;
    destroyNode(S, mounts);
    mounts = p;
  }
  if (writePath) {
    destroyNode(S, writePath);
    writePath = NULL;
  }
}
*/

int fs_mount(ar_State *S, const char *path) {
  /* Check if path is valid directory */
  if (!isDir(path)) {
    return FS_EBADPATH;
  }
  /* Check path isn't already mounted*/
  PathNode *p = mounts;
  while (p) {
    if (!strcmp(p->path, path)) {
      return FS_ESUCCESS;
    }
    p = p->next;
  }
  /* Construct node */
  p = newNode(S, path);
  if (!p) return FS_EOUTOFMEM;
  /* Add to start of list (highest priority) */
  p->next = mounts;
  mounts = p;
  return FS_ESUCCESS;
}


int fs_unmount(ar_State *S, const char *path) {
  PathNode **next = &mounts;
  while (*next) {
    if (!strcmp((*next)->path, path)) {
      PathNode *p = *next;
      *next = (*next)->next;
      destroyNode(S, p);
      break;
    }
    next = &(*next)->next;
  }
  return FS_ESUCCESS;
}


int fs_setWritePath(ar_State *S, const char *path) {
  int res = makeDirs(S, path);
  if (!isDir(path)) {
    return (res != FS_ESUCCESS) ? FS_ECANTMKDIR : FS_EBADPATH;
  }
  PathNode *p = newNode(S, path);
  if (!p) return FS_EOUTOFMEM;
  if (writePath) {
    destroyNode(S, writePath);
  }
  writePath = p;
  return FS_ESUCCESS;
}


static int fileInfo(
  ar_State *S, const char *filename, unsigned *mtime, size_t *size, int *isdir, int *isreg
) {
  if (checkFilename(filename) != FS_ESUCCESS) return FS_EBADFILENAME;
  filename = skipDotSlash(filename);
  PathNode *p = mounts;
  while (p) {
    struct stat s;
    char *r = concat(S, p->path, "/", filename, NULL);
    if (!r) return FS_EOUTOFMEM;
    int res = stat(r, &s);
    ar_free(S, r);
    if (res == 0) {
      if (mtime) *mtime = s.st_mtime;
      if (size) *size = s.st_size;
      if (isdir) *isdir = S_ISDIR(s.st_mode);
      if (isreg) *isreg = S_ISREG(s.st_mode);
      return 0;
    }
    p = p->next;
  }
  return FS_ENOTEXIST;
}



int fs_exists(ar_State *S, const char *filename) {
  return fileInfo(S, filename, NULL, NULL, NULL, NULL) == FS_ESUCCESS;
}


int fs_modified(ar_State *S, const char *filename, unsigned *mtime) {
  return fileInfo(S, filename, mtime, NULL, NULL, NULL);
}


int fs_size(ar_State *S, const char *filename, size_t *size) {
  return fileInfo(S, filename, NULL, size, NULL, NULL);
}


void *fs_read(ar_State *S, const char *filename, size_t *len) {
  size_t len_ = 0;
  if (!len) len = &len_;
  if (checkFilename(filename) != FS_ESUCCESS) return NULL;
  filename = skipDotSlash(filename);
  PathNode *p = mounts;
  while (p) {
    char *r = concat(S, p->path, "/", filename, NULL);
    if (!r) return NULL;
    FILE *fp = fopen(r, "rb");
    ar_free(S, r);
    if (!fp) goto next;
    /* Get file size */
    fseek(fp, 0, SEEK_END);
    *len = ftell(fp);
    /* Load file */
    fseek(fp, 0, SEEK_SET);
    char *res = ar_alloc(S, NULL, *len + 1);
    if (!res) return NULL;
    res[*len] = '\0';
    if (fread(res, 1, *len, fp) != *len) {
      ar_free(S, res);
      fclose(fp);
      return NULL;
    }
    fclose(fp);
    return res;
next:
    p = p->next;
  }
  return NULL;
}


int fs_isDir(ar_State *S, const char *filename) {
  int res;
  int err = fileInfo(S, filename, NULL, NULL, &res, NULL);
  if (err) return 0;
  return res;
}


int fs_isFile(ar_State *S, const char *filename) {
  int res;
  int err = fileInfo(S, filename, NULL, NULL, NULL, &res);
  if (err) return 0;
  return res;
}


static fs_FileListNode *newFileListNode(ar_State *S, const char *path) {
  fs_FileListNode *n = ar_alloc(S, NULL, sizeof(*n) + strlen(path) + 1);
  if (!n) return NULL;
  n->next = NULL;
  n->name = (void*) (n + 1);
  strcpy(n->name, path);
  return n;
}


static int appendFileListNode(ar_State *S, fs_FileListNode **list, const char *path) {
  fs_FileListNode **n = list;
  while (*n) {
    if (!strcmp(path, (*n)->name)) return FS_ESUCCESS;
    n = &(*n)->next;
  }
  *n = newFileListNode(S, path);
  if (!*n) return FS_EOUTOFMEM;
  return FS_ESUCCESS;
}


fs_FileListNode *fs_listDir(ar_State *S, const char *path) {
  char *pathTrimmed = NULL;
  int pathTrimmedLen;
  fs_FileListNode *res = NULL;
  PathNode *p = mounts;
  if (checkFilename(path) != FS_ESUCCESS) return NULL;
  /* Copy path string, trim separator from end if it exists */
  pathTrimmed = concat(S, path, NULL);
  if (!pathTrimmed) goto outOfMem;
  pathTrimmedLen = strlen(pathTrimmed);
  if (isSeparator(pathTrimmed[pathTrimmedLen - 1])) {
    pathTrimmed[--pathTrimmedLen] = '\0';
  }
  if (!strcmp(pathTrimmed, ".")) {
    pathTrimmed[0] = '\0';
    pathTrimmedLen = 0;
  }
  /* Fill result list */
  while (p) {
    DIR *dp;
    struct dirent *ep;
    char *r = concat(S, p->path, "/", pathTrimmed, NULL);
    if (!r) goto outOfMem;
    dp = opendir(r);
    ar_free(S, r);
    if (!dp) goto next;
    while ((ep = readdir(dp))) {
      /* Skip ".." and "." */
      if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, "..")) {
        continue;
      }
      if (appendFileListNode(S, &res, ep->d_name) != FS_ESUCCESS) {
        closedir(dp);
        goto outOfMem;
      }
    }
    closedir(dp);
next:
    p = p->next;
  }
  ar_free(S, pathTrimmed);
  return res;
outOfMem:
  ar_free(S, pathTrimmed);
  fs_freeFileList(S, res);
  return NULL;
}


void fs_freeFileList(ar_State *S, fs_FileListNode *list) {
  fs_FileListNode *next;
  while (list) {
    next = list->next;
    ar_free(S, list);
    list = next;
  }
}


static int writeUsingMode(
  ar_State *S, const char *filename, const char *mode, const void *data, int size
) {
  if (!writePath) return FS_ENOWRITEPATH;
  if (checkFilename(filename) != FS_ESUCCESS) return FS_EBADFILENAME;
  char *name = concat(S, writePath->path, "/", filename, NULL);
  if (!name) return FS_EOUTOFMEM;
  FILE *fp = fopen(name, mode);
  ar_free(S, name);
  if (!fp) return FS_ECANTOPEN;
  int res = fwrite(data, size, 1, fp);
  fclose(fp);
  return (res == 1) ? FS_ESUCCESS : FS_ECANTWRITE;
}

int fs_write(ar_State *S, const char *filename, const void *data, int size) {
  return writeUsingMode(S, filename, "wb", data, size);
}


int fs_append(ar_State *S, const char *filename, const void *data, int size) {
  return writeUsingMode(S, filename, "ab", data, size);
}


int fs_delete(ar_State *S, const char *filename) {
  if (!writePath) return FS_ENOWRITEPATH;
  if (checkFilename(filename) != FS_ESUCCESS) return FS_EBADFILENAME;
  char *name = concat(S, writePath->path, "/", filename, NULL);
  if (!name) return FS_EOUTOFMEM;
  int res = remove(name);
  ar_free(S, name);
  return (res == 0) ? FS_ESUCCESS : FS_ECANTDELETE;
}


int fs_makeDirs(ar_State *S, const char *path) {
  if (!writePath) return FS_ENOWRITEPATH;
  if (checkFilename(path) != FS_ESUCCESS) return FS_EBADFILENAME;
  char *name = concat(S, writePath->path, "/", path, NULL);
  if (!name) return FS_EOUTOFMEM;
  int res = makeDirs(S, name);
  ar_free(S, name);
  return res;
}


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
  int res = fs_mount(S, path);
  if (res != FS_ESUCCESS) {
    ar_error_str(S, "%s '%s'", fs_errorStr(res), path);
  }
  return S->t;
}


static ar_Value *f_unmount(ar_State *S, ar_Value *args) {
  const char *path = AR_GET_STRING(0);
  fs_unmount(S, path);
  return S->t;
}


static ar_Value *f_setWritePath(ar_State *S, ar_Value *args) {
  const char *path = AR_GET_STRING(0);
  int res = fs_setWritePath(S, path);
  checkError(S, res, path);
  return S->t;
}


static ar_Value *f_exists(ar_State *S, ar_Value *args) {
  const char *filename = AR_GET_STRING(0);
  return fs_exists(S, filename) ? S->t : NULL;
}


static ar_Value *f_getSize(ar_State *S, ar_Value *args) {
  const char *filename = AR_GET_STRING(0);
  size_t sz;
  int res = fs_size(S, filename, &sz);
  checkError(S, res, filename);
  return ar_new_number(S, sz);
}


static ar_Value *f_getModified(ar_State *S, ar_Value *args) {
  const char *filename = AR_GET_STRING(0);
  unsigned t;
  int res = fs_modified(S, filename, &t);
  checkError(S, res, filename);
  return ar_new_number(S, t);
}


static ar_Value *f_io_read(ar_State *S, ar_Value *args) {
  const char *filename = AR_GET_STRING(0);
  size_t len;
  char *data = fs_read(S, filename, &len);
  if (!data) {
    ar_error_str(S, "could not read file '%s'", filename);
  }
  ar_Value *res = ar_new_stringl(S, data, len);
  ar_free(S, data); return res;
}


static ar_Value *f_isDir(ar_State *S, ar_Value *args) {
  const char *filename = AR_GET_STRING(0);
  return fs_isDir(S, filename) ? S->t : NULL;
}


static ar_Value *f_isFile(ar_State *S, ar_Value *args) {
  const char *filename = AR_GET_STRING(0);
  return fs_isFile(S, filename) ? S->t : NULL;
}


static ar_Value *f_listDir(ar_State *S, ar_Value *args) {
  const char *path = AR_GET_STRING(0);
  fs_FileListNode *list = fs_listDir(S, path);
  ar_Value *res = NULL, **last = &res;
  fs_FileListNode *n = list;
  while (n) {
    last = ar_append_tail(S, last, ar_new_string(S, n->name));
    n = n->next;
  }
  fs_freeFileList(S, list);
  return res;
}


static ar_Value *f_write(ar_State *S, ar_Value *args) {
  const char *filename = AR_GET_STRING(0);
  size_t len;
  const char *data = AR_GET_STRINGL(1, len);
  int res = fs_write(S, filename, data, len);
  checkError(S, res, filename);
  return S->t;
}


static ar_Value *f_append(ar_State *S, ar_Value *args) {
  const char *filename = AR_GET_STRING(0);
  size_t len;
  const char *data = AR_GET_STRINGL(1, len);
  int res = fs_append(S, filename, data, len);
  checkError(S, res, filename);
  return S->t;
}


static ar_Value *f_delete(ar_State *S, ar_Value *args) {
  const char *filename = AR_GET_STRING(0);
  int res = fs_delete(S, filename);
  if (res != FS_ESUCCESS) {
    ar_error_str(S, "%s", fs_errorStr(res));
  }
  return S->t;
}


static ar_Value *f_makeDirs(ar_State *S, ar_Value *args) {
  const char *path = AR_GET_STRING(0);
  int res = fs_makeDirs(S, path);
  if (res != FS_ESUCCESS) {
    ar_error_str(S, "%s '%s'", fs_errorStr(res), path);
  }
  return S->t;
}


void register_io(ar_State *S) {
  /* list of functions to register */
  fs_mount(S, ".");
  fs_setWritePath(S, ".");
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
  /* atexit(fs_deinit); */
}
