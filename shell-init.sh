# save xterm tty and window ID for use by color-scheme-watcher
case "$TERM" in
    xterm*|rxvt*)
    if [ -n "$WINDOWID" ] && tty -s; then
        mkdir -p ~/.saved-xterm-ttys
        (echo "win=$WINDOWID"; echo "tty=$(tty)") > ~/.saved-xterm-ttys/.$$
        mv ~/.saved-xterm-ttys/.$$ ~/.saved-xterm-ttys/$$
        trap "rm -f ~/.saved-xterm-ttys/$$" EXIT
    fi
    ;;
esac
