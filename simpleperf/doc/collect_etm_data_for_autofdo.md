# Collect etm data for AutoFDO

ETM is a hardware on arm64 devices. It collects instruction stream running on each cpu.
Android uses ETM as an alternative for LBR (last branch record) on x86.
Simpleperf supports collecting etm data, and convert it to input files for AutoFDO, which can
then be used for profiling guided optimization in compilation.

On ARMv8, ETM is considered as an external debug hardware. So it needs to be enabled in
bootloader, and isn't available on user devices. For Pixel devices, it's available on
EVT and DVT devices on Pixel 4, Pixel 4a (5G) and Pixel 5.

Below are examples collecting etm data for AutoFDO. It has two steps: first recording
etm data, second converting etm data to AutoFDO input files.

Record etm data:

```sh
# preparation: we need to be root to record etm data
$ adb root
$ adb shell
redfin:/ \# cd data/local/tmp
redfin:/data/local/tmp \#

# Do a system wide collection, it writes output to perf.data.
# If only want etm data for kernel, use `-e cs-etm:k`.
# If only want etm data for userspace, use `-e cs-etm:u`.
redfin:/data/local/tmp \# simpleperf record -e cs-etm --duration 3 -a

# To reduce file size and injection time, we recommend converting etm data into a protobuf format.
redfin:/data/local/tmp \# simpleperf inject --output branch-list -o branch_list.data
```

Converting etm data to AutoFDO input files needs to read binaries.
So for userspace libraries, they can be converted on device. For kernel, it needs
to be converted on host, with vmlinux and kernel modules available.

Convert etm data for userspace libraries:

```sh
# Injecting etm data on device. It writes output to perf_inject.data.
# perf_inject.data is a text file, containing branch counts for each library.
redfin:/data/local/tmp \# simpleperf inject -i branch_list.data
```

Convert etm data for kernel:

```sh
# pull etm data to host.
host $ adb pull /data/local/tmp/branch_list.data
# download vmlinux and kernel modules to <binary_dir>
# host simpleperf can get from <aosp-master>/system/extras/simpleperf/scripts/bin/linux/x86_64/simpleperf,
# or build simpleperf by `mmma system/extras/simpleperf`.
host $ simpleperf inject --symdir <binary_dir> -i branch_list.data
```

The generated perf_inject.data may contain branch info for multiple binaries. But AutoFDO only
accepts one at a time. So we need to split perf_inject.data.
The format of perf_inject.data is below:

```perf_inject.data format

executed sections with count info for binary1
branch with count info for binary1
// name for binary1

executed sections with count info for binary2
branch with count info for binary2
// name for binary2

...
```

We need to split perf_inject.data, and make sure one file only contains info for one binary.

Then we can use AutoFDO to create profile like below:

```sh
# perf_inject_kernel.data is split from perf_inject.data, and only contains branch info for [kernel.kallsyms].
host $ autofdo/create_llvm_prof -profile perf_inject_kernel.data -profiler text -binary vmlinux -out a.prof -format binary
```
