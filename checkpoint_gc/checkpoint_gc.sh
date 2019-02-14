#!/system/bin/sh

#
# Copyright (C) 2019 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# This script will run as an pre-checkpointing cleanup for mounting f2fs
# with checkpoint=disable, so that the first mount after the reboot will
# be faster. It is unnecessary to run if the device does not use userdata
# checkpointing on F2FS.

TARGET_SLOT="$1"
STATUS_FD="$2"
GC_TIME=120

DEVICE=`while read dev dir type opt; do
[ /data != ${dir} -o f2fs != ${type} ] || echo ${dev}
done < /proc/mounts`
NAME=${DEVICE##*/}
if [ -z "$NAME" ]; then
  exit 0
fi
log -pi -t checkpoint_gc Turning on GC for $NAME
PROGRESS=0
echo 1 > /sys/fs/f2fs/$NAME/gc_urgent

COUNT=0
STEP=5
while [ $COUNT -lt $GC_TIME ]; do
  print -u${STATUS_FD} "global_progress `echo $COUNT/$MAX|bc -l`"
  sleep $STEP
  COUNT=$(($COUNT+$STEP))
done

log -pi -t checkpoint_gc Turning off GC for $NAME
echo 0 > /sys/fs/f2fs/$NAME/gc_urgent
sync

print -u${STATUS_FD} "global_progress 1.0"
exit 0
