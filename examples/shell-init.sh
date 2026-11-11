# save xterm tty for use by color-scheme-watcher
case "$TERM" in
    xterm*|rxvt*)
        if [ -n "$WINDOWID" ] && tty -s; then
            xprop -id "$WINDOWID" -f COLOR_SCHEME_WATCHER_TTY 8s \
                -set COLOR_SCHEME_WATCHER_TTY "`tty`"
        fi
        ;;
esac
