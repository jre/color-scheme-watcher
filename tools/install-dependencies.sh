#!/bin/sh

set -e
set -x

build_xcb_xrm() {
    xrm_make="$1"
    xrm_prefix="$2"
    shift 2
    rm -rf /tmp/xcb-util-xrm
    "$@" "cd /tmp && git clone --recursive --depth=1 https://github.com/Airblader/xcb-util-xrm"
    "$@" "cd /tmp/xcb-util-xrm && ./autogen.sh --prefix=$xrm_prefix && $xrm_make"
    $xrm_make -C /tmp/xcb-util-xrm install
}

sunos_pkg_install() {
    # sign, pkg install is documented as failing with 4 to mean "nothing to do"
    set +e
    pkg install --accept "$@"
    pkg_err=$?
    if [ "$pkg_err" -gt 0 -a "$pkg_err" -ne 4 ]; then
        return "$pkg_err"
    fi
    return 0
}

os="$(uname)"
case "$os" in
    Linux)
        if which apt >/dev/null 2>&1; then
            apt update
            apt install --yes --no-install-recommends build-essential \
                libglib2.0-dev libgio-2.0-dev \
                libx11-xcb-dev libxcb-icccm4-dev libxcb-xrm-dev
        elif which yum >/dev/null 2>&1; then
            dnf config-manager --set-enabled crb || \
                dnf config-manager --set-enabled powertools || true
            yum makecache
            yum install -y epel-release
            yum groupinstall -y "Development Tools"
            yum install -y git glib2-devel libxcb-devel xcb-util-wm-devel
            if ! yum install xcb-util-xrm-devel &&
                    [ ! -r /usr/include/xcb/xcb_xrm.h ]; then
                yum install -y libX11-devel xcb-util-devel xorg-x11-util-macros
                build_xcb_xrm make /usr su -s /bin/sh nobody -c
            fi
        elif which apk >/dev/null 2>&1; then
            apk update
            apk add build-base glib-dev \
                libxcb-dev xcb-util-wm-dev xcb-util-xrm-dev
        else
            echo "error: unsupported Linux distribution" >&2
        fi
        ;;

    FreeBSD)
        pkg update
        pkg install --yes git glib libxcb xcb-util-wm xcb-util-xrm
        if ! which pkg-config >/dev/null 2>&1; then
            pkg install --yes pkgconf
        fi
        ;;

    NetBSD)
        url="https://cdn.netbsd.org/pub/NetBSD/NetBSD-$(uname -r)/$(uname -m)"
        if [ ! -d /usr/X11R7 ]; then
            cd / && ftp -o - "$url/binary/sets/xbase.tar.xz" | tar -xyf -
        fi
        if [ ! -r /usr/X11R7/include/xcb/xcb.h ]; then
            cd / && ftp -o - "$url/binary/sets/xcomp.tar.xz" | tar -xyf -
        fi
        pkg_add pkg-config git glib2 xcb xcb-util-wm xcb-util-xrm
        ;;

    OpenBSD)
        pkg_add -I git glib2
        ;;

    SunOS)
        variant="$(head -n 1 /etc/release | awk '{print $1}')"
        case "$variant" in
            OpenIndiana)
                pkg refresh
                sunos_pkg_install build-essential glib2 \
                    libxcb xcb-util-wm xcb-util-xrm
                ;;
            Oracle)
                pkg refresh
                sunos_pkg_install gcc git glib2 pkg-config libxcb xcb-util \
                    autoconf automake libtool xorg-macros
                if [ ! -r /usr/include/xcb/xcb_xrm.h ]; then
                    build_xcb_xrm gmake /usr su nobody -c
                fi
                ;;
            *)
                echo "error: unsupported SunOS variant: $variant" >&2
                ;;
        esac
        ;;

    *)
        echo "error: unsupported operating system: $os" >&2
        exit 1
esac
