#!/usr/bin/env sh
set -e

DFU_UTIL="dfu-util"

$DFU_UTIL -d 0483:df11 -a 0 -s 0x08004000 -D obj/panda_jungle.bin
$DFU_UTIL -d 0483:df11 -a 0 -s 0x08000000:leave -D obj/bootstub.panda_jungle.bin
