#!/bin/sh

simpleperf record -e avc:selinux_audited -g -a -o /data/misc/selinux_debug/perf.data --size-limit 10M
