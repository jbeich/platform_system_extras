#!/bin/bash
#
# To call this script, make sure mke2fs is somewhere in PATH

function usage() {
cat<<EOT
Usage:
mkuserimg.sh [-s] SRC_DIR OUTPUT_FILE EXT_VARIANT MOUNT_POINT SIZE [-j <journal_size>]
             [-T TIMESTAMP] [-C FS_CONFIG] [-D PRODUCT_OUT] [-B BLOCK_LIST_FILE]
             [-d BASE_ALLOC_FILE_IN ] [-A BASE_ALLOC_FILE_OUT ] [-L LABEL]
             [-i INODES ] [-e ERASE_BLOCK_SIZE] [-o FLASH_BLOCK_SIZE] [FILE_CONTEXTS]
EOT
}

BLOCKSIZE=4096

MKE2FS_OPTS=""
MKE2FS_EXTENDED_OPTS=""
E2FSDROID_OPTS=""

if [ "$1" = "-s" ]; then
  MKE2FS_EXTENDED_OPTS+="android_sparse"
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
    MKE2FS_OPTS+="-O ^has_journal"
  else
    MKE2FS_OPTS+="-J size=$2"
  fi
  shift; shift
fi

if [[ "$1" == "-T" ]]; then
  E2FSDROID_OPTS+=" -T $2"
  shift; shift
fi

if [[ "$1" == "-C" ]]; then
  E2FSDROID_OPTS+=" -C $2"
  shift; shift
fi

if [[ "$1" == "-D" ]]; then
  E2FSDROID_OPTS+=" -p $2"
  shift; shift
fi

if [[ "$1" == "-B" ]]; then
  E2FSDROID_OPTS+=" -B $2"
  shift; shift
fi

if [[ "$1" == "-d" ]]; then
  E2FSDROID_OPTS+=" -d $2"
  shift; shift
fi

if [[ "$1" == "-A" ]]; then
  E2FSDROID_OPTS+=" -D $2"
  shift; shift
fi

if [[ "$1" == "-L" ]]; then
  MKE2FS_OPTS+=" -L $2"
  shift; shift
fi

if [[ "$1" == "-i" ]]; then
  MKE2FS_OPTS+=" -N $2"
  shift; shift
fi

if [[ "$1" == "-e" ]]; then
  if [[ MKE2FS_EXTENDED_OPTS ]]; then
    MKE2FS_EXTENDED_OPTS+=","
  fi
  MKE2FS_EXTENDED_OPTS+="stripe_width=$(($2/BLOCKSIZE))"
  shift; shift
fi

if [[ "$1" == "-o" ]]; then
  if [[ MKE2FS_EXTENDED_OPTS ]]; then
    MKE2FS_EXTENDED_OPTS+=","
  fi
  # stride should be the max of 8kb and the logical block size
  MKE2FS_EXTENDED_OPTS+="stride=$((($2 > 8192 ? $2 : 8192) / BLOCKSIZE))"
  shift; shift
fi

if [[ MKE2FS_EXTENDED_OPTS ]]; then
  MKE2FS_OPTS+=" -E $MKE2FS_EXTENDED_OPTS"
fi

E2FSDROID_OPTS+=" -S $1"

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

# Round down the filesystem length to be a multiple of the block size
SIZE=$((SIZE / BLOCKSIZE))

MAKE_EXT4FS_CMD="mke2fs $MKE2FS_OPTS -t $EXT_VARIANT -b $BLOCKSIZE $OUTPUT_FILE $SIZE"
echo $MAKE_EXT4FS_CMD
$MAKE_EXT4FS_CMD
if [ $? -ne 0 ]; then
  exit 4
fi

E2FSDROID_CMD="e2fsdroid $E2FSDROID_OPTS -f $SRC_DIR -a /$MOUNT_POINT $OUTPUT_FILE"
echo $E2FSDROID_CMD
$E2FSDROID_CMD
if [ $? -ne 0 ]; then
  rm -f $OUTPUT_FILE
  exit 4
fi
