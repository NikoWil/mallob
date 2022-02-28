#!/bin/bash

set -e

# Fetch a clean state of pandaPIparser
if [ ! -d pandaPIparser ]; then
    echo "Fetching pandaPIparser ..."
    git clone https://github.com/panda-planner-dev/pandaPIparser.git
    cd pandaPIparser
else
    cd pandaPIparser
    git clean -f
fi

# Checkout correct commit (can be updated but must be manually checked to build cleanly)
git config advice.detachedHead false
git checkout 95bbe291c5bdb9fb517c1ad55f5136d45450c644

# Patch pandaPIparser with adapted makefile and "library" header
cp ../panda_makefile makefile
cp ../libpanda.hpp src/

# Replace the literal used for compiling out method preconditions
sed -i 's/__method_precondition_/<method_prec>/g' src/domain.hpp

# Build library (internally does a patch of pandaPIparser's main.cpp)
make library
mkdir ../../lib/pandaPIparser
cp build/libpandaPIparser.a ../../lib/pandaPIparser
echo "Copied libpandaPIparser.a into lib/pandaPIparser directory."

mkdir ../../lib/pandaPIparser/include
cp src/*.hpp ../../lib/pandaPIparser/include
echo "Copied header files into lib/pandaPIparser/include directory"

# Build standalone executable for debugging purposes
make executable
cp pandaPIparser ../../
echo "Copied pandaPIparser executable into mallob root directory."
