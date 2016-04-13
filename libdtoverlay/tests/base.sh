#!/bin/bash

dtb_to_dts () {
  dtc -O dts -qq -f -s $1
  if [ $? -ne 0 ]; then
    exit 1
  fi
}

dts_to_dtb () {
  dtc -O dtb -qq -f -s -@ $1
  if [ $? -ne 0 ]; then
    exit 1
  fi
}

remove_local_fixups() {
  sed '/__local_fixups__/ {s/^\s*__local_fixups__\s*//; :again;N; s/{[^{}]*};//; /^$/ !b again; d}' $1
}

remove_overlay_stuff() {
  # remove __symbols__, phandle, "linux,phandle" and __local_fixups__
  sed "/__symbols__/,/[}];/d" $1 | sed "/\(^[ \t]*phandle\)/d" | sed "/\(^[ \t]*linux,phandle\)/d" | sed '/^\s*$/d' | remove_local_fixups
}

dt_diff () {
  diff -u <(dtb_to_dts "$1" | remove_overlay_stuff) <(dtb_to_dts "$2" | remove_overlay_stuff)
}
