#!/bin/bash

set -e
make clean all >/dev/null

STUDENTID=16-821-837

if [ "$STUDENTID" = "16-821-837" ]
then
    echo "Please update your student ID in this file"
    exit 0
fi

if [ "$N" = "" ]
then N=100
fi

MELTDOWN_SEGV=${STUDENTID}-meltdown_segv
MELTDOWN_TSX=${STUDENTID}-meltdown_tsx
SPECTRE=${STUDENTID}-spectre

function newsecret()
{
    secret=`head -c 100 </dev/urandom | md5sum | awk '{ print $1 }'`
    if [ `echo -n $secret | wc -c` != 32 ]
    then
        echo Expected secret to be 32 bytes.
        exit 1
    fi
    echo "GENERATED SECRET: $secret" >&2
    echo $secret >/dev/wom
}

if [ "$1" = "batch" ]
then
    for bin in $MELTDOWN_SEGV $MELTDOWN_TSX $SPECTRE
    do
      if [ ! -e $bin ]
      then    echo "No $bin found"
              continue
      fi
    
      echo "RELIABILITY TEST OVER $N RUNS OF $bin ..."
    
      newsecret
      rm -f log
      for x in `seq 1 $N`
      do
          rm -f out
          ./$bin >out 2>>errlog
          grep -q $secret out && echo -n "." || echo -n "?"
          cat out >>log
      done
      echo ''
      echo -n 'SUCCESS RATE: '
      grep -c $secret log  || true
      echo ''
    done

    rm -f log
    rm -f out
    rm -f errlog
    exit 0
fi

for bin in $MELTDOWN_SEGV $MELTDOWN_TSX $SPECTRE
do
    echo "**** TESTING $bin ****"
    newsecret
    ./$bin > out
    printf "YOUR BINARY OUTPUT >>>>>>>>\n"
    cat out
    printf "<<<<<<<<\n"
    printf "TEST RESULT: "
    grep -q $secret out && echo "PASS" || echo "FAIL"
    echo "====================================="
    echo ""
    rm -f out
done
