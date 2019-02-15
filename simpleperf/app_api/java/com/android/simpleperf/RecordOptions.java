/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.simpleperf;

import android.system.Os;
import java.util.ArrayList;
import java.util.List;

public class RecordOptions {

    // Default is perf.data.
    public RecordOptions setOutputFilename(String filename) {
        outputFilename = filename;
        return this;
    }

    // Default is cpu-cycles.
    public RecordOptions setEvent(String event) {
        this.event = event;
        return this;
    }

    // Default is 4000.
    public RecordOptions setSampleFrequency(int freq) {
        this.freq = freq;
        return this;
    }

    // Default is no limit, namely record until stopped.
    public RecordOptions setDuration(double durationInSecond) {
        this.durationInSecond = durationInSecond;
        return this;
    }

    // Default is to record the whole process.
    public RecordOptions setSampleThreads(List<Integer> threads) {
        this.threads.addAll(threads);
        return this;
    }

    public RecordOptions recordDwarfCallGraph() {
        this.dwarfCallGraph = true;
        this.fpCallGraph = false;
        return this;
    }

    public RecordOptions recordFramePointerCallGraph() {
        this.fpCallGraph = true;
        this.dwarfCallGraph = false;
        return this;
    }

    public RecordOptions traceOffCpu() {
        this.traceOffCpu = true;
        return this;
    }

    public List<String> toRecordArgs() {
        ArrayList<String> args = new ArrayList<>();
        args.add("-o");
        args.add(outputFilename);
        args.add("-e");
        args.add(event);
        args.add("-f");
        args.add(String.valueOf(freq));
        if (durationInSecond != 0.0) {
            args.add("--duration");
            args.add(String.valueOf(durationInSecond));
        }
        if (threads.isEmpty()) {
            args.add("-p");
            args.add(String.valueOf(Os.getpid()));
        } else {
            String s = "";
            for (int i = 0; i < threads.size(); i++) {
                if (i > 0) {
                    s += ",";
                }
                s += threads.get(i).toString();
            }
            args.add("-t");
            args.add(s);
        }
        if (dwarfCallGraph) {
            args.add("-g");
        } else if (fpCallGraph) {
            args.add("--call-graph");
            args.add("fp");
        }
        if (traceOffCpu) {
            args.add("--trace-offcpu");
        }
        return args;
    }

    private String outputFilename = "perf.data";
    private String event = "cpu-cycles";
    private int freq = 4000;
    private double durationInSecond = 0.0;
    private ArrayList<Integer> threads = new ArrayList<>();
    private boolean dwarfCallGraph = false;
    private boolean fpCallGraph = false;
    private boolean traceOffCpu = false;
}
