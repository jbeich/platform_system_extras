#!/bin/bash

set -euox pipefail

adb push $ANDROID_PRODUCT_OUT/system/bin/mtectrl /data/local/tmp/mtectrl
adb shell 'echo > /data/local/tmp/misc_memtag'

function finish {
    adb shell rm /data/local/tmp/mtectrl /data/local/tmp/misc_memtag
}

trap finish EXIT

adb shell /data/local/tmp/mtectrl -t /data/local/tmp/misc_memtag memtag-once
adb shell 'xxd /data/local/tmp/misc_memtag' | diff - <(cat <<EOF
00000000: 015a fefe 5a02 0000 0000 0000 0000 0000  .Z..Z...........
00000010: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000020: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000030: 0000 0000 0000 0000 0000 0000 0000 0000  ................
EOF
)
adb shell /data/local/tmp/mtectrl -t /data/local/tmp/misc_memtag memtag-once,memtag-kernel
adb shell 'xxd /data/local/tmp/misc_memtag' | diff - <(cat <<EOF
00000000: 015a fefe 5a06 0000 0000 0000 0000 0000  .Z..Z...........
00000010: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000020: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000030: 0000 0000 0000 0000 0000 0000 0000 0000  ................
EOF
)
adb shell /data/local/tmp/mtectrl -t /data/local/tmp/misc_memtag memtag 
adb shell 'xxd /data/local/tmp/misc_memtag' | diff - <(cat <<EOF
00000000: 015a fefe 5a01 0000 0000 0000 0000 0000  .Z..Z...........
00000010: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000020: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000030: 0000 0000 0000 0000 0000 0000 0000 0000  ................
EOF
)
adb shell /data/local/tmp/mtectrl -t /data/local/tmp/misc_memtag -s arm64.memtag.test_bootctl
[ $(adb shell getprop arm64.memtag.test_bootctl) = "memtag" ]
adb shell /data/local/tmp/mtectrl -t /data/local/tmp/misc_memtag -s arm64.memtag.test_bootctl memtag force_off
[ $(adb shell getprop arm64.memtag.test_bootctl) = "memtag-off" ]
adb shell /data/local/tmp/mtectrl -t /data/local/tmp/misc_memtag -s arm64.memtag.test_bootctl memtag,memtag-once force_off
[ $(adb shell getprop arm64.memtag.test_bootctl) = "memtag-once,memtag-off" ]