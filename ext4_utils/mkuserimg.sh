#!/bin/bash
#
# To call this script, make sure make_ext4fs is somewhere in PATH

function usage() {
cat<<EOT
Usage:
mkuserimg.sh [-s] SRC_DIR OUTPUT_FILE EXT_VARIANT MOUNT_POINT SIZE [-j <journal_size>]
             [-T TIMESTAMP] [-C FS_CONFIG] [-D PRODUCT_OUT] [-B BLOCK_LIST_FILE]
             [-d BASE_ALLOC_FILE_IN ] [-A BASE_ALLOC_FILE_OUT ] [-L LABEL]
             [ -i INODES ] [FILE_CONTEXTS]
EOT
}

OPTS=""
EXTENDED_OPTS=""

function add_extended_opt() {
    if [ -z "$EXTENDED_OPTS" ]; then
        EXTENDED_OPTS="-E $1"
    else
        EXTENDED_OPTS+=",$1"
    fi
    if [ ! -z "$2" ]; then
        EXTENDED_OPTS+="=$2"
    fi
}

if [ "$1" = "-s" ]; then
    add_extended_opt "sparse_file"
  shift
fi

if [ $# -lt 5 ]; then
  usage
  exit 1
fi

SRC_DIR=$1
if [ ! -d $SRC_DIR ]; then
  echo "Can not find directory $SRC_DIR!"
  exit 2
fi

OUTPUT_FILE=$2
EXT_VARIANT=$3
MOUNT_POINT=$4
SIZE=$5
shift; shift; shift; shift; shift

if [ "$1" = "-j" ]; then
  if [ "$2" = "0" ]; then
    OPTS+="-O ^has_journal"
  else
    OPTS+="-J size=$2"
  fi
  shift; shift
fi

if [[ "$1" == "-T" ]]; then
  add_extended_opt "timestamp" "$2"
  shift; shift
fi

if [[ "$1" == "-C" ]]; then
  add_extended_opt "fs_config" "$2"
  shift; shift
fi

if [[ "$1" == "-D" ]]; then
  add_extended_opt "target_out" "$2"
  shift; shift
fi

if [[ "$1" == "-B" ]]; then
  add_extended_opt "block_list" "$2"
  shift; shift
fi

if [[ "$1" == "-d" ]]; then
  add_extended_opt "base_fs_in" "$2"
  shift; shift
fi

if [[ "$1" == "-A" ]]; then
  add_extended_opt "base_fs_out" "$2"
  shift; shift
fi

if [[ "$1" == "-L" ]]; then
  OPTS+=" -L $2"
  shift; shift
fi

if [[ "$1" == "-i" ]]; then
  OPTS+=" -N $2"
  shift; shift
fi

add_extended_opt "file_contexts" "$1"

case $EXT_VARIANT in
  ext4) ;;
  *) echo "Only ext4 is supported!"; exit 3 ;;
esac

if [ -z $MOUNT_POINT ]; then
  echo "Mount point is required"
  exit 2
fi

if [ -z $SIZE ]; then
  echo "Need size of filesystem"
  exit 2
fi

BLOCKSIZE=4096
OPTS+=" -d $SRC_DIR"
OPTS+=" -t $EXT_VARIANT"
OPTS+=" -b $BLOCKSIZE"
add_extended_opt "mountpoint" "/$MOUNT_POINT"
# Round down the filesystem length to be a multiple of the block size
SIZE=$((SIZE / BLOCKSIZE))

MAKE_EXT4FS_CMD="mke2fs $OPTS $EXTENDED_OPTS $OUTPUT_FILE $SIZE"
echo $MAKE_EXT4FS_CMD
$MAKE_EXT4FS_CMD
if [ $? -ne 0 ]; then
  exit 4
fi
