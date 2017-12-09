#!/usr/bin/python2.7
import os, sys, shutil, platform, time

SUFFIX = ".so"
COMPILER = "gcc"
INCLUDE = [  ]
LINK = [ "aria" ]
DEFINE = [  ]
CFLAGS = [ "-Wall", "-Wextra", "-c", "-fPIC", "-fno-strict-aliasing" "--std=c99", "-pedantic", "-O3" ]
LFLAGS = [ "-shared", "-fPIC" ]
EXTRA = [  ]

SOURCE = "src"


# def fmt(fmt, dic):
#   for k in dic:
#     v = " ".join(dic[k]) if type(dic[k]) is list else dic[k]
#     fmt = fmt.replace("{" + k + "}", str(v))
#   return fmt
#
#
# def clearup(file):
#   if os.path.isfile(file + SUFFIX):
#     os.remove(file + SUFFIX)

def walk(dir):
  FILES = [ ]
  for root, dir, files in os.walk(dir):
    for f in files:
      if f[len(f)-2:] == ".c":
          FILES += [ "%s/%s" % (root.replace("\\", "/"), f) ]
  return FILES



def main():
  global SOURCE
  # FILES = [ d for d in os.listdir(SOURCE) if os.path.isdir(d) and d[len(d)-2:] == '.c' ]
  # FILES = [ d for d in os.listdir(SOURCE) if os.path.isdir(d) and d[0] != '.' ]

  FILES = walk(SOURCE)

  for f in FILES:
      print f





if __name__ == "__main__":
  main()
