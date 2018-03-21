/**
 * Copyright (c) 2017 emekoi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

 /*===========================================================================
  * OS related funcs
  *===========================================================================*/

#if _WIN32
  #include <windows.h>
#elif __linux__
  #include <unistd.h>
#elif __APPLE__
  #include <mach-o/dyld.h>
#endif

#define UNUSED(x) ((void) x)
#define AR_GET_ARG(idx, type) S, ar_check(S, ar_nth(args, idx), type)
#define AR_GET_STRING(idx) (char *)ar_to_string(AR_GET_ARG(idx, AR_TSTRING))
#define AR_GET_STRINGL(idx, len) (char *)ar_to_stringl(AR_GET_ARG(idx, AR_TSTRING), &len)

#define ASSERT(x)\
  do {\
    if (!(x)) {\
      fprintf(stderr, "%s:%d: %s(): assertion '%s' failed\n",\
              __FILE__, __LINE__, __func__, #x);\
      abort();\
    }\
  } while (0)


static ar_Value *f_system(ar_State *S, ar_Value *args) {
  char *command = AR_GET_STRING(0);
  ar_Value *res = ar_new_number(S, (double)system(command));
  return res;
}


/* doesn't work on windows and has error on other platforms
static void *read_stream(FILE *fp, size_t *len) {
  size_t len_ = 0;
  if (!len) len = &len_;
  if (!fp) goto end;
  / * Get file size * /
  fseek(fp, 0, SEEK_END);
  *len = ftell(fp);
  / * Load file * /
  fseek(fp, 0, SEEK_SET);
  char *res = malloc(*len + 1);
  if (!res) return NULL;
  res[*len] = '\0';
  if (fread(res, 1, *len, fp) != *len) {
    free(res); return NULL;
  } else return res;
  end:
    return NULL;
}
*/

/*
static ar_Value *f_popen(ar_State *S, ar_Value *args) {
  char *command = AR_GET_STRING(0);
  char *mode = AR_GET_STRING(1);
  if (!(strcmp(mode, "w") == 0 || strcmp(mode, "r") == 0))
    ar_error_str(S, "unknown mode %s", mode);

  FILE *fp = popen(command, mode);
  if (!fp) ar_error_str(S, "could not open pipe for %s", command);

  if (!strcmp(mode, "r")) {
    size_t len = 0; char *data = read_stream(stdout, &len);
    ar_Value *res = ar_new_string(S, data);
    pclose(fp);
    return res;
  } else {
    size_t len = 0;
    char *data = (char *)ar_to_stringl(S, ar_check(S, ar_nth(args, 2), AR_TSTRING), &len);
    int res = fwrite(data, strlen(data), 1, fp); pclose(fp);
    if (res == -1) ar_error_str(S, "error writing to pipe");
    return ar_new_number(S, res);
  }
}
*/


static char *dirname(char *str) {
  char *p = str + strlen(str);
  while (p != str) {
    if (*p == '/' || *p == '\\') {
      *p = '\0';
      break;
    }
    p--;
  }
  return str;
}

static ar_Value *f_info(ar_State *S, ar_Value *args) {
  char *str = AR_GET_STRING(0);

  if (!strcmp(str, "os")) {
#if _WIN32
    return ar_new_string(S, "windows");
#elif __linux__
    return ar_new_string(S, "linux");
#elif __FreeBSD__
    return ar_new_string(S, "bsd");
#elif __APPLE__
    return ar_new_string(S, "osx");
#else
    return ar_new_string(S, "?");
#endif

  } else if (!strcmp(str, "exedir")) {
    UNUSED(dirname);
#if _WIN32
    char buf[1024];
    int len = GetModuleFileName(NULL, buf, sizeof(buf) - 1);
    buf[len] = '\0';
    dirname(buf);
    return ar_new_stringf(S, "%s", buf);
#elif __linux__
    char path[128];
    char buf[1024];
    sprintf(path, "/proc/%d/exe", getpid());
    int len = readlink(path, buf, sizeof(buf) - 1);
    ASSERT( len != -1 );
    buf[len] = '\0';
    dirname(buf);
    return ar_new_stringf(S, "%s", buf);
#elif __FreeBSD__
    /* TODO : Implement this */
    return ar_new_stringf(S, ".");
#elif __APPLE__
    char buf[1024];
    uint32_t size = sizeof(buf);
    ASSERT( _NSGetExecutablePath(buf, &size) == 0 );
    dirname(buf);
    return ar_new_stringf(S, "%s", buf);
#else
    return ar_new_string(S, ".");
#endif

  } else if (!strcmp(str, "appdata")) {
#if _WIN32
    return ar_new_stringf(S, "%s", getenv("APPDATA"));
#elif __APPLE__
    return ar_new_stringf(S, "%s/Library/Application Support", getenv("HOME"));
#else
    return ar_new_stringf(S, "%s/.local/share", getenv("HOME"));
#endif
  } else {
    ar_error_str(S, "invalid string '%s'", str);
  }
  return NULL;
}


void register_os(ar_State *S) {
  /* list of functions to register */
  struct { const char *name; ar_CFunc fn; } funcs[] = {
    { "os-system", f_system },
    /* { "os-popen",  f_popen  }, / * not working */
    { "os-info",   f_info   },
    { NULL, NULL }
  };

  /* register functions */
  for (int i = 0; funcs[i].name; i++) {
    ar_bind_global(S, funcs[i].name, ar_new_cfunc(S, funcs[i].fn));
  }
}
