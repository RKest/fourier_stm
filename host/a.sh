#!/bin/bash

set -exu

DEVICE=$1
BUILD_DIR=${2:-build}

cat $1 | $BUILD_DIR/recv 2>err.txt | while read line ; do
    a=`echo $line | $BUILD_DIR/vis -l 20 --max 400`
    clear
    echo "$a"
done

