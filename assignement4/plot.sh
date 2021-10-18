#!/bin/bash

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
NORMAL='\033[0m'
STUDENTID=16-821-837

MELTDOWN_SEGV=/data/gthivolet/${STUDENTID}-meltdown_segv
MELTDOWN_TSX=/data/gthivolet/${STUDENTID}-meltdown_tsx
SPECTRE=/data/gthivolet/${STUDENTID}-spectre

if [ "$N" = "" ]
then N=20
fi


function newsecret()
{
    secret=`head -c 100 </dev/urandom | md5sum | awk '{ print $1 }'`
    if [ `echo -n $secret | wc -c` != 32 ]
    then
        echo Expected secret to be 32 bytes.
        exit 1
    fi
    echo "GENERATED SECRET: $secret" >&2
    echo $secret > /dev/wom
}

runtime=0

for bin in $SPECTRE # $MELTDOWN_SEGV  $MELTDOWN_TSX $SPECTRE
do 
    if [ ! -e $bin ]
    then    echo "No $bin found"
            continue
    fi

    echo "RELIABILITY TEST OVER $N RUNS OF $bin ..."
    runtime=0
    accuracy=0
    newsecret
    rm -f log
    for x in `seq 1 $N`
    do
    
        rm -f out
        start=`date +%s%N`
        /$bin -r >out 2>>errlog
        end=`date +%s%N`
        
        leaked=$(< out)
        for (( i=0; i<${#secret}; i++ )); do
            if [ "${secret:$i:1}" == "${leaked:$i:1}" ] 
            then
                accuracy=$((accuracy+1))
            fi
        done
        
        runtime=$((runtime + end-start))
       

        cat out >>log
    done
    accuracy=$(( (accuracy)))
    echo "$accuracy / $((N*32))"
    echo "$((runtime/1000000/N))"

    
done

