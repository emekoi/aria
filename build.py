#!/usr/bin/python2.7
import os, sys, shutil, platform, time
import cembed

OUTPUT = "bin/aria"
EMBED_DIR = "src/embed"
TEMPSRC_DIR = ".tempsrc"
COMPILER = "gcc"
INCLUDE = [ TEMPSRC_DIR, "src/lib" ]
SOURCE = [
  "src/*.c",
  "src/lib/dmt/*.c",
]
FLAGS = [ "-Wall", "-Wextra", "--std=c99", "-pedantic" ]
LINK = [ "m" ]
DEFINE = [ "AR_STANDALONE" ]
EXTRA = [  ]

if platform.system() == "Windows":
  sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 0)
  OUTPUT += ".exe"
  LINK += [ "mingw32" ]
  DEFINE += [ "AR_DL_DLL" ]
  # DEFINE += [ "AR_DL_DLOPEN" ]

if platform.system() == "Darwin":
  SOURCE += [ "src/lib/linenoise/*.c" ]
  DEFINE += [ "AR_DL_DLOPEN" ]
  LINK += [ "dl" ]

if platform.system() == "Linux":
  SOURCE += [ "src/lib/linenoise/*.c" ]
  DEFINE += [ "AR_DL_DLOPEN", "DMT_STACK_TRACE" ]
  LINK += [ "dl" ]


def fmt(fmt, dic):
  for k in dic:
    v = " ".join(dic[k]) if type(dic[k]) is list else dic[k]
    fmt = fmt.replace("{" + k + "}", str(v))
  return fmt


def clearup():
  if os.path.exists(TEMPSRC_DIR):
    shutil.rmtree(TEMPSRC_DIR)


def main():
  global FLAGS, SOURCE, LINK, DEFINE

  print "initing..."
  starttime = time.time()

  # Handle args

  if "release" in sys.argv: build = "release"
  elif "unsafe" in sys.argv: build = "unsafe"
  elif "size" in sys.argv: build = "size"
  elif "release2" in sys.argv: build = "release 2"
  else: build = "debug"

  verbose = "verbose" in sys.argv

  # Handle build type
  if build == "release":
    FLAGS += [ "-funroll-loops", "-O3", "-ftracer" ]
  elif build == "unsafe":
    FLAGS += [ "-funroll-loops", "-Ofast", "-ftracer" ]
  elif build == "release2":
    FLAGS += [ "-funroll-loops", "-02", "-ftracer" ]
  elif build == "size":
    FLAGS += [ "-funroll-loops", "-Os" ]
  else:
    FLAGS += [ "-Og", "-pg", "-g" ]
    DEFINE += [ "AR_DEBUG" ]

  print "building (" + build + ")..."

  # Make sure there arn't any temp files left over from a previous build
  clearup()

  # Create directories
  os.makedirs(TEMPSRC_DIR)
  outdir = os.path.dirname(OUTPUT)
  if not os.path.exists(outdir):
    os.makedirs(outdir)

  # Create embedded-file header files
  for filename in os.listdir(EMBED_DIR):
    fullname = EMBED_DIR + "/" + filename
    res = cembed.process(fullname)
    open(TEMPSRC_DIR + "/" + cembed.safename(fullname) + ".h", "wb").write(res)

  # Build
  cmd = fmt(
    "{compiler} -o {output} {flags} {source} {include} {link} {define} " +
    "{extra}",
    {
      "compiler"  : COMPILER,
      "output"    : OUTPUT,
      "source"    : " ".join(SOURCE),
      "include"   : " ".join(map(lambda x:"-I" + x, INCLUDE)),
      "link"      : " ".join(map(lambda x:"-l" + x, LINK)),
      "define"    : " ".join(map(lambda x:"-D" + x, DEFINE)),
      "flags"     : " ".join(FLAGS),
      "extra"     : " ".join(EXTRA),
    })

  if verbose:
    print cmd

  print "compiling..."
  res = os.system(cmd)

  if build != "debug":
    if os.path.isfile(OUTPUT):
      print "stripping..."
      os.system("strip %s" % OUTPUT)

  print "clearing up..."
  clearup()

  if res == 0:
    print "done (%.2fs)" % (time.time() - starttime)
  else:
    print "done with errors"
  sys.exit(res)


if __name__ == "__main__":
  main()
