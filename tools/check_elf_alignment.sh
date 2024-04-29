#!/bin/bash
progname="${0##*/}"
progname="${progname%.sh}"

# usage: check_elf_alignment.sh [path to *.so files|path to *.apk]

cleanup_trap() {
  if [ -n "${tmp}" -a -d "${tmp}" ]; then
    rm -rf ${tmp}
  fi
  exit $1
}

usage() {
  echo "Host side script to check the ELF alignment of shared libraries."
  echo "Shared libraries are reported ALIGNED when their ELF regions are"
  echo "16 KB or 64 KB aligned. Otherwise they are reported as UNALIGNED."
  echo
  echo "Usage: ${progname} [input-path|input-APK]"
}

if [ ${#} -ne 1 ]; then
  usage
  exit
fi

while [ ${#} -gt 0 ]; do
  case ${1} in
    --help | -h | -\?)
      usage
      exit
      ;;

    *)
      dir="${1}"
      ;;

  esac
  shift
done

if [[ $dir == *.apk ]]; then
  trap 'cleanup_trap' EXIT
  tmp=$(mktemp -d -t ${dir%.apk}_out_XXXXX)
  unzip $dir lib/arm64-v8a/* lib/x86_64/* -d ${tmp} >/dev/null 2>&1
  dir=${tmp}
fi

RED="\e[31m"
GREEN="\e[32m"
ENDCOLOR="\e[0m"

matches="$(find $dir -name "*.so" -type f)"
IFS=$'\n'
for match in $matches; do
  res="$(objdump -p ${match} | grep LOAD | awk '{ print $NF }' | head -1)"
  if [[ $res =~ "2**14" ]] || [[ $res =~ "2**16" ]]; then
    echo -e "${match}: ${GREEN}ALIGNED${ENDCOLOR} ($res)"
  else
    echo -e "${match}: ${RED}UNALIGNED${ENDCOLOR} ($res)"
  fi
done
