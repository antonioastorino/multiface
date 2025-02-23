#!/usr/bin/env zsh

set -ue
DEV=$1
FLAGS="-Wall -Wextra -std=c2x -pedantic"
if [ "$(uname -s)" = "Linux" ]; then
    FLAGS="${FLAGS} -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_GNU_SOURCE"
fi
mkdir -p build/
clang -o build/multiface src/main.c `echo ${FLAGS}` && ./build/multiface "$DEV"
