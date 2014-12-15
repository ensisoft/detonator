
#!/bin/bash

clear

# allow unlimited core files to be written
ulimit -c unlimited

# set core file notation
#echo "%e_core" > /proc/sys/kernel/core_pattern

if [ "$0" == "./invaders.sh" ]; then
    export LD_LIBRARY_PATH=`pwd`
    exec "./invaders" "$@"
else
    cwd=`pwd`
    path=`echo $0 | sed s/invaders.sh//`
    cd "$path"
    export LD_LIBRARY_PATH=`pwd`
    exec "./invaders" "$@"
fi
