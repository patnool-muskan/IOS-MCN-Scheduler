#!/bin/bash

files=(
    # alarms
    "$O1_ADAPTER_SRC/alarms/alarms.c"

    # common
    "$O1_ADAPTER_SRC/common/config.c"
    "$O1_ADAPTER_SRC/common/utils.c"

    # netconf
    "$O1_ADAPTER_SRC/netconf/netconf.c"
    "$O1_ADAPTER_SRC/netconf/netconf_session.c"
    "$O1_ADAPTER_SRC/netconf/netconf_rpc.c"
    "$O1_ADAPTER_SRC/netconf/netconf_data.c"
    
    # oai
    "$O1_ADAPTER_SRC/oai/oai.c"
    "$O1_ADAPTER_SRC/oai/oai_data.c"

    # pm_data
    "$O1_ADAPTER_SRC/pm_data/pm_data.c"

    # telnet
    "$O1_ADAPTER_SRC/telnet/telnet.c"

    # ves
    "$O1_ADAPTER_SRC/ves/ves.c"
    "$O1_ADAPTER_SRC/ves/ves_internal.c"

    "$O1_ADAPTER_SRC/main.c"
)

include=(
    "."
)

libs=(
    "pthread"
    "yang"
    "sysrepo"
    "curl"
    "telnet"
    "cjson"
)

sources=""
for i in ${files[@]}
do
    sources="$sources $i"
done

libraries=""
for i in ${libs[@]}
do
    libraries="$libraries -l$i"
done

includes=""
for i in ${other_include[@]}
do
    includes="$includes -I$i"
done

for i in ${include[@]}
do
    includes="$includes -I$i"
done

output="$ROOT/build/bin/gnb-adapter"
build="gcc -g -Wall -pedantic -I$O1_ADAPTER_SRC $includes $sources $libraries -o$output"

echo "Building with command: $build"
$build
if [ "$?" -ne "0" ]; then
  echo "Build failed"
  exit 1
fi
