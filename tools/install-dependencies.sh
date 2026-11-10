#!/bin/sh

set -e

case "$(lsb_release -si 2>/dev/null)" in
    Debian|Ubuntu)
        apt update
        apt install libglib2.0-dev libgio-2.0-dev \
            libx11-xcb-dev libxcb-icccm4-dev libxcb-xrm-dev
        ;;
esac
