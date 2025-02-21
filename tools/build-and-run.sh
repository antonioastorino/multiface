#!/usr/bin/env zsh

set -ue
DEV=$1
mkdir -p build/
clang -o build/multiface src/main.c -Wall -Wextra -pedantic && ./build/multiface "$DEV"
