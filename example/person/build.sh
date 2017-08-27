# ./build.sh

# build the shared object
gcc -shared -o person.so person.c ../../src/*.c

# remove .o file
rm -rf person.o
