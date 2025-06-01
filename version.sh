#!/bin/sh

INSTR="$1"

case "$INSTR" in
    get-vcs)
        git -C "$MESON_SOURCE_ROOT" describe --always --dirty
        ;;
    *)
        echo invalid arguments 1>&2
        exit 1
        ;;
esac
