#!/usr/bin/env zsh

set -ue
DEV=$1
mkdir -p build/
clang -o build/test-ttytermios src/main.c -Wall -Wextra -pedantic && ./build/test-ttytermios "$DEV"
