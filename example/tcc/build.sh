# ./build.sh

# build the shared object
gcc -shared -o tcc.so tcc.c ../../src/*.c

# remove .o file
rm -rf tcc.o
