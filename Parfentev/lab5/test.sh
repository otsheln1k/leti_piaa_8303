#!/bin/sh

if [ "$#" -lt 2 ]
then
        echo "usage: $0 EXENAME TESTDIR [OPTS ...]"
        exit 1
fi

exe="$1"
tdir="$2"
shift 2

# Treat `foo' as `./foo' instead of using foo from the $PATH
exe_file="$(dirname "$exe")/$(basename "$exe")"

find "$tdir" -name "*.in" -print | \
    {
        i=1
        while read infile
        do
            outfile="$(echo "$infile" | sed 's/in$/out/')"
            [ -r "$outfile" ] || continue
            cat "$infile" \
                | "$exe_file" "$@" \
                | diff - "$outfile" \
                || { echo "Test #$i ($infile) failed" >&2; exit 1; }
            i="$(expr $i + 1)"
        done
        echo "All $(expr $i - 1) tests passed"
    }
