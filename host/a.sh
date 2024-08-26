#!/bin/bash

cat /dev/ttyACM0 | ./cmake-build-debug/recv 2>err.txt | while read line ; do
    a=`echo $line | ./cmake-build-debug/vis -l 20 --max 400`
    clear
    echo "$a"
done

