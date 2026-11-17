#!/bin/sh

set -e
set -x

cd "$(dirname "$0")"
aclocal
autoconf
rm -rf autom4te.cache configure~
