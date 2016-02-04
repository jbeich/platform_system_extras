#!/bin/bash

fname=$1

# We want to generate two device tree blob (.dtb) files by combining 
# the "base" and "add" device tree source (.dts) files in two 
# different ways.
#
# 1) /include/ the "add" at the end of the "base" file and
#   compile with dtc to make the "gold standard" .dtb
#
# 2) Compile them separately (modifying the "add" file to
#   be an overlay file) with dtc, then join them with the
#   ov_test program
#
# To do this, we need to generate a lot of files from the .base_dts
#    and .add_dts files:
# .base_inc_dts - Has the /include/ "$fname.add_dts" at the end. 
# .base_inc_dtb - The dtc-generated "gold standard"
# .add_ov_dts - Has the /plugin/ at the start
# .base_dtb - Compiled version of just the base
# .add_ov_dtbo - Compiled version of the overlay
# .base_ov_dtb - overlay-test-joined version of the dtb
#
# Then, compare .base_inc_dtb and .base_ov_dtb with dtdiff 
# (or maybe diff?) and return 0 iff they are identical.

#set -x

test_gen=$OUT/gen/test
mkdir $test_gen >& /dev/null

cp "$fname.base_dts" "$test_gen/$fname.base_dts"
cp "$fname.add_dts" "$test_gen/$fname.add_dts"

# Add the "include" to make .base_inc_dts
cp "$fname.base_dts" "$test_gen/$fname.base_inc_dts"
echo "/include/ \"$fname.add_dts\"" >> "$test_gen/$fname.base_inc_dts"

# Generate .base_inc_dtb
dtc -O dtb -b 0 -o "$test_gen/$fname.base_inc.dtb" \
  "$test_gen/$fname.base_inc_dts"

#/ {
#	fragment@0 {
#		target = <&i2c_3>;
#		__overlay__ {
# Prepend the "plugin" to make .add_ov_dts
sed "1{N;N;s/\/ *\([{]\)[ \t\n\r]*\([^ \t\n\r]\{1,\}\)/\/dts-v1\/ \/plugin\/;\n\/ \1\n fragment@0 \1\n target = <\&\2>;\n __overlay__ /}" "$fname.add_dts" > "$test_gen/$fname.add_ov_dts"
echo "};" >> "$test_gen/$fname.add_ov_dts"

# Make a standalone of the "add" to help the user find syntax errors
echo "/dts-v1/;" > "$test_gen/$fname.add_sa_dts"
cat "$fname.add_dts" >> "$test_gen/$fname.add_sa_dts"

# Compile the base to make .base_dtb
dtc -O dtb -b 0 -@ -o "$test_gen/$fname.base_dtb" \
  "$test_gen/$fname.base_dts"

# Compile the standalone of the add (syntax error checking)
dtc -O dtb -b 0 -@ -o "$test_gen/$fname.add_sa_dtb" \
  "$test_gen/$fname.add_sa_dts"

# Compile the overlay to make .add_ov_dtbo
dtc -O dtb -b 0 -@ -o "$test_gen/$fname.add_ov_dtbo" \
  "$test_gen/$fname.add_ov_dts"

# Run ov_test to combine .base_dtb and .add_ov_dtbo
#   into .base_ov_dtb
dtoverlay_test_app "$test_gen/$fname.base_dtb" \
  "$test_gen/$fname.add_ov_dtbo" \
  "$test_gen/$fname.base_ov.dtb"

dt_source () {
  dtc -O dts -qq -f -s -o - $1
}

# Run the diff
diff -u <(dt_source "$test_gen/$fname.base_inc.dtb") <((dt_source "$test_gen/$fname.base_ov.dtb") | sed "/__symbols__/,/[}];/d" | sed "/\(^[ \t]*phandle\)/d" | sed "/\(^[ \t]*linux,phandle\)/d" | sed '$!N; /^\(.*\)\n\1$/!P; D')

set +x
