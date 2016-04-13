#!/bin/bash

# dt_check.sh vendor.dts odm.dts
#
# The script will generate .dtb files via two different approaches and compare them.
# Approach 1: Compile input .dts files separately and combine the resulting .dtb files
# with libdboverlay.
# Approach 2: Combine input .dts files first and then compile it with dtc.
#
# Note that the odm.dts is expected to be in the add-on format as in:
# /dts-v1/ /plugin/;
# &node_label {
#   ...
# };
#
# The script does not work with the following fragment format:
# /dts-v1/ /plugin/;
# / {
#   fragment@0 {
#     target = <&node_label>;
#     __overlay__ {
#       ...
#     };
#   };
# };

source tests/base.sh

OUTDIR=$OUT/gen/test/dt_check
ALL_OV_DTB=$OUTDIR/all_ov.dtb
ALL_INC_DTB=$OUTDIR/all_inc.dtb

rm -rf $OUTDIR/* >& /dev/null
mkdir $OUTDIR >& /dev/null

# Approach 1: via libdtoverlay
TMP1=$OUTDIR/1.dtb
TMP2=$OUTDIR/2.dtb
dts_to_dtb $1 > $TMP1
dts_to_dtb $2 > $TMP2
dtoverlay_test_app $TMP1 $TMP2 $ALL_OV_DTB

# Approach 2: via dtc
TMP=$OUTDIR/c.dts
cat $1 > $TMP
grep -v -e /dts-v\d+/\; -e /plugin/\; $2 >> $TMP  # remove /dts-v1/ /plugin/;
dts_to_dtb $TMP > $ALL_INC_DTB

# Run the diff
RESULT=$OUTDIR/result.txt
dt_diff $ALL_INC_DTB $ALL_OV_DTB > $RESULT
if [ -s $RESULT ]; then
  cat $RESULT
else
  echo 'All good.'
fi
