<img style="float:left;" src="inferno.png" width="140"><div style="display:inline-block; font-size:120px;">INFERNO</div> 

## Description

Inferno is a flamegrapher for native (C/C++) Android apps. It was originally written to profile and improve
surfaceflinger performances (Android compositior) but it can be used for any native Android application (since ART does not generate frame pointers, Inferno cannot be used for Java/Kotlin apps). You can see a sample report generated with Inferno [here](../report.html).

Notice there is no concept of time in a flamegraph since callstacks are collapsed together. As a result, the width of a flamegraph represent 100% of samples and the height is related to the number of function on the stack when sampling occured.


<img style="display:inline-block;" src="main_thread_flamegraph.png">

## How it works
Inferno relies on simpleperf to record the callstack of a native application thousand of times per second. When recording stops, `simpleperf` dumps all the data it collected to `perf.data`. This file is pulled from the Android device and processed on the host. The callstacks are merged together to visualize in which part of an app the CPU cycles are spent. 



## How to use it

Open a terminal and from `simpleperf` directory type:
```
./inferno.sh  (on Linux/Mac)
./inferno.bat (on Windows)
```

Inferno will collect data, process them and automatically open your webbrowser to display the HTML report.

## Parameters

You can select for how long to sample, the color of the node and many other things. Use `-h` flag to get a list of all supported parameters.

```
inferno -h
```

## Troubleshooting

### Messy flamegraph
An healthy flamegraph features a single callsite at its base.
If you don't see an unique callsite like `_start` or `_start_thread` at the base from which originate all flames, it is likely that stack unwinding failed while recording callstacks. To solve this proble, double check you compiled without omitting frame pointers.

### run-as: package not debuggable
If you cannot run as root, make sure the app is debuggable otherwise simpleperf will not be able to profile it.