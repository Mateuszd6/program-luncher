for i in $(echo $PATH | tr ':' '\n'); do ls -1 $i; done | sort | \
    /home/mateusz/work/xlib-test/bin/program 2> ~/foo.al | ${SHELL:-"/bin/sh"} &
