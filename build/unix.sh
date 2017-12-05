#! /bin/sh -e
mkdir -p .tempsrc
python2.7 cembed.py src/embed/class.lsp > .tempsrc/class_lsp.h
python2.7 cembed.py src/embed/core.lsp > .tempsrc/core_lsp.h
python2.7 cembed.py src/embed/repl.lsp > .tempsrc/repl_lsp.h
gcc -c -Wall -Wextra --std=c99 -ansi -pedantic -fno-strict-aliasing -finline-functions -funroll-loops -g src/aria.c -o aria.o -I.tempsrc -DAR_DL_DLOPEN -DAR_STANDALONE
gcc -c -Wall -Wextra --std=c99 -ansi -pedantic -fno-strict-aliasing -finline-functions -funroll-loops -g src/lib/linenoise/linenoise.c -o linenoise.o
gcc aria.o linenoise.o -o bin/aria -lm -ldl
gcc -c -Wall -Wextra --std=c99 -ansi -pedantic -fno-strict-aliasing -finline-functions -funroll-loops -g src/aria.c -o libaria.o -DAR_DL_DLOPEN -fPIC
gcc -shared -fPIC -Wl,-soname,libaria.so libaria.o -o bin/libaria.so -lm -ldl
mkdir -p /usr/local/bin/ /usr/local/lib/ /usr/local/include/aria/
install -m644 src/aria.h /usr/local/include/aria/
install -m644 src/util.h /usr/local/include/aria/
install -m644 bin/libaria.so /usr/local/lib/
install -m755 bin/aria /usr/local/bin/
