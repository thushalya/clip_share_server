#!/bin/bash

. init.sh

sample='Sample text for v2 send_text'

proto=$(printf '\x02' | bin2hex)
method=$(printf '\x02' | bin2hex)
length=$(printf '%016x' "${#sample}")
sampleDump=$(echo -n "${sample}" | bin2hex)

clear_clipboard

responseDump=$(echo -n "${proto}${method}${length}${sampleDump}" | hex2bin | client_tool | bin2hex | tr -d '\n')

protoAck=$(printf '\x01' | bin2hex)
methodAck=$(printf '\x01' | bin2hex)

expected="${protoAck}${methodAck}"

if [ "${responseDump}" != "${expected}" ]; then
    showStatus info 'Incorrect server response.'
    echo 'Expected:' "$expected"
    echo 'Received:' "$responseDump"
    exit 1
fi

clip="$(get_copied_text || echo fail)"

if [ "${clip}" != "${sampleDump}" ]; then
    showStatus info 'Clipcoard content not matching.'
    echo 'Expected:' "$sampleDump"
    echo 'Received:' "$clip"
    exit 1
fi
