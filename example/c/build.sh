# ./build.sh

# compile the .o file
gcc -c -o person.o person.c

# build dynaminc library
gcc -shared -o person.so person.o ../../src/*.c

# remove .o file
rm -rf person.o
