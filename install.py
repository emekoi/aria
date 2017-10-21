#!/usr/bin/python2.7
import os, sys, shutil, platform, time

OUTPUT = "bin/aria"
INSTALL = "install"

# EXECUTABLE 755
# REGULAR 644

INSTALLS = [
  ["bin/aria",   755, "/usr/local/bin"],
  ["src/aria.h", 644, "/usr/local/include/aria"],
]

if platform.system() == "Windows":
  sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 0)
  OUTPUT += ".exe"
  # INSTALL = "cp"
  INSTALLS = [
    ["bin/aria",   755, "/c/MinGW/bin"],
    ["src/aria.h", 644, "/c/MinGW/include/aria"],
  ]


def fmt(fmt, dic):
  for k in dic:
    fmt = fmt.replace("{" + k + "}", str(dic[k]))
  return fmt


def main():
  global INSTALL, INSTALLS

  print "initing..."
  starttime = time.time()

  verbose = "verbose" in sys.argv

  res = 0

  for FILE in INSTALLS:
    print "installing " + FILE[0]
    cmd = fmt(
      "mkdir -p \"{dir}\" && {install} -m{mode} {file} \"{dir}\"",
      {
        "dir"     : FILE[2],
        "file"    : FILE[0],
        "mode"    : FILE[1],
        "install" : INSTALL,
        "output"  : OUTPUT,
      })

    if verbose:
      print cmd

    res = os.system(cmd)


  if res == 0:
    print "done (%.2fs)" % (time.time() - starttime)
  else:
    print "done with errors"
  sys.exit(res)


if __name__ == "__main__":
  main()
