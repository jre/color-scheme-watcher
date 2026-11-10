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

version=
outfile=
while getopts ho:v: opt; do
    case "$opt" in
        o) outfile="$OPTARG" ;;
        v) version="$OPTARG" ;;
        *)
            echo "usage: $0 -o output-file -v version trs-file..."
            exit 1
            ;;
    esac
done
shift $(($OPTIND - 1))

outfile_abs="$(realpath "$outfile" 2>/dev/null)"
if [ -z "$outfile_abs" ]; then
    outfile_abs="$(cd "$(dirname "$outfile")" && pwd)/$(basename "$outfile")"
fi

types="PASS SKIP XFAIL FAIL XPASS ERROR"
separator='========================================================================'

trs_count_result_type() {
    grep '[[:space:]]*:test-result:[[:space:]]*'"$1" | wc -l
}

trs_get_copy_global() {
    sed -n 's/^[[:space:]]*:copy-in-global-log:[[:space:]]*\([^[:space:]]*\)[[:space:]]*$/\1/p'
}

trs_get_global_result() {
    sed -n 's/^[[:space:]]*:global-test-result:[[:space:]]*\([^[:space:]]*\)[[:space:]]*$/\1/p'
}

total_tests=
for type in $types; do
    eval "total_$type=0"
done

for trs in "$@"; do
    tests="$(grep :test-result: "$trs" | wc -l)"
    total_tests=$(($total_tests + $tests))
    for type in $types; do
        count="$(trs_count_result_type "$type" < "$trs")"
        eval "total_$type=\$((\$total_$type + \$count))"
    done
done

output_summary() {
    echo "$separator"
    if [ -n "$version" ]; then
        echo "Test summary: (version $version)"
    else
        echo "Test summary:"
    fi
    echo "$separator"
    echo "# TOTAL: $total_tests"
    for type in $types; do
        eval "count=\"\$total_$type\""
        printf "# %-6s %d\n" "$type:" "$count"
    done
}

if [ -n "$outfile" ]; then
    tmp="${outfile}-$$.new"
    rm -f "$tmp"
    output_summary > "$tmp"
    for trs in "$@"; do
        log="${trs%.trs}.log"
        name="${trs%.trs}"
        name="${name##*/}"
        name="${name#test-}"
        case "$(trs_get_copy_global < "$trs")" in
            no|No|nO|NO) ;;
            *)
                result="$(trs_get_global_result < "$trs")"
                echo
                echo "${result:-RUN}: $name"
                echo "$separator"
                cat "$log"
                ;;
        esac
    done >> "$tmp"
    mv "$tmp" "$outfile"
fi

output_summary
if [ $total_FAIL != 0 -o $total_XPASS != 0 -o $total_ERROR != 0 ]; then
    echo "$separator"
    echo "For details, see"
    echo "  $outfile_abs"
    echo "$separator"
    exit 1
fi
