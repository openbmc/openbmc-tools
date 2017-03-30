#!/bin/bash -x

set -euo pipefail

CONNECT="$@"

i=0

while true;
do
    echo Boot $i

    ssh ${CONNECT} /usr/sbin/obmcutil poweron
    time expect petitboot.exp -- ${CONNECT}
    ssh ${CONNECT} /usr/sbin/obmcutil poweroff
    sleep 20 # mboxd reload period. Remove when protocol v2 daemon is integrated

    i=$(($i + 1))

    echo
done

