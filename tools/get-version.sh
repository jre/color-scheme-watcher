#!/bin/sh

# Copyright (c) 2026 Joshua R. Elsasser.

# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.

# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

dflag=
case "$1" in
    --clean) ;;
    --dirty) dflag=--dirty ;;
    *)
        echo "usage: $0 { --clean | --dirty } version-file" >&2
        exit 1
        ;;
esac
vfile="$2"

if ! [ -d .git ]; then
   cat "$vfile"
   exit $?
fi

tag="$(git describe $dflag --tags --exact-match 2>/dev/null)"
if [ -n "$tag" ]; then
    echo "${tag#v}"
    exit 0
fi

sha="$(git describe $dflag --always 2>/dev/null)"
if [ -n "$sha" ]; then
    name="$(git rev-parse --symbolic-full-name HEAD 2>/dev/null)"
    if [ "x$name" = xHEAD ]; then
        echo "$sha"
    else
        echo "${name#refs/heads/}-$sha"
    fi
    exit 0
fi

exit 1
