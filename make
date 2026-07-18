#!/usr/bin/bash
set -e
MAKE="${MAKE:-make}"
$MAKE -f makefiles/$1.mk "${@:2}"