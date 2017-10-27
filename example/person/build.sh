# ./build.sh

# build the shared object
gcc -Wall -Wextra -c -fPIC -fno-strict-aliasing person.c -o person.o                                                                                
gcc -Wall -Wextra -c -fPIC -fno-strict-aliasing ../../src/aria.c -o aria.o                                                                                
gcc -shared aria.o person.o -o person.so     

# remove .o file
rm -rf person.o
rm -rf aria.o
