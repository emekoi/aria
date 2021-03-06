#!/bin/sh

# build the shared object
gcc -Wall -Wextra -c -std=c99 -fPIC -fno-strict-aliasing -I../../src     person.c -o person.o
gcc -Wall -Wextra -c -std=c99 -fPIC -fno-strict-aliasing -I../../src/lib ../../src/aria.c -o aria.o
gcc -Wall -Wextra -c -std=c99 -fPIC -fno-strict-aliasing -I../../src/lib ../../src/lib/dmt/dmt.c -o dmt.o

suffix=so

if [[ "$OSTYPE" == "cygwin" ]] || [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "win32" ]]; then
suffix=dll
fi

gcc -shared aria.o dmt.o person.o -o person.$suffix

# remove .o file
rm -rf person.o
rm -rf dmt.o
rm -rf aria.o
