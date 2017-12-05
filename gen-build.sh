#! /bin/sh -e

files=(*.config)
#SUDO = sudo
mkdir -p build
for i in "${files[@]}"
do
	echo "creating build script for $i"
    #$SUDO tup generate --config `realpath $i` build/${i%.config}.sh
    $SUDO tup generate --config $i build/${i%.config}.sh
done 