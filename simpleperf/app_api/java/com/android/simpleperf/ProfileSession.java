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

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

// A Java API used to control simpleperf recording.
// To see simpleperf logs in logcat, filter logcat with "simpleperf".
public class ProfileSession {

    // appDataDir is the same as android.content.Context.getDataDir().
    // ProfileSession will store profiling data in appDataDir/simpleperf_data/.
    public ProfileSession(String appDataDir) {
        this.appDataDir = appDataDir;
        simpleperfDataDir = appDataDir + "/simpleperf_data";
    }

    // Assume appDataDir is /data/data/<app_package_name>.
    public ProfileSession() {
        String packageName = "";
        try {
            String s = readInputStream(new FileInputStream("/proc/self/cmdline"));
            for (int i = 0; i < s.length(); i++) {
                if (s.charAt(i) == '\0') {
                    s = s.substring(0, i);
                    break;
                }
            }
            packageName = s;
        } catch (IOException e) {
            throw new Error("failed to find packageName: " + e.getMessage());
        }
        if (packageName.isEmpty()) {
            throw new Error("failed to find packageName");
        }
        appDataDir = "/data/data/" + packageName;
        simpleperfDataDir = appDataDir + "/simpleperf_data";
    }

    public void startRecording(RecordOptions options) {
        startRecording(options.toRecordArgs());
    }

    public synchronized void startRecording(List<String> args) {
        if (state != STATE_NOT_YET_STARTED) {
            throw new AssertionError("startRecording: session in wrong state " + state);
        }
        String simpleperfPath = findSimpleperf();
        checkIfPerfEnabled();
        createSimpleperfDataDir();
        createSimpleperfProcess(simpleperfPath, args);
        state = STATE_STARTED;
    }

    public synchronized void pauseRecording() {
        if (state != STATE_STARTED) {
            throw new AssertionError("pauseRecording: session in wrong state " + state);
        }
        sendCmd("pause");
        state = STATE_PAUSED;
    }

    public synchronized void resumeRecording() {
        if (state != STATE_PAUSED) {
            throw new AssertionError("resumeRecording: session in wrong state " + state);
        }
        sendCmd("resume");
        state = STATE_STARTED;
    }

    public synchronized void stopRecording() {
        if (state != STATE_STARTED && state != STATE_PAUSED) {
            throw new AssertionError("stopRecording: session in wrong state " + state);
        }
        simpleperfProcess.destroy();
        try {
            int exitCode = simpleperfProcess.waitFor();
            if (exitCode != 0) {
                throw new AssertionError("simpleperf exited with error: " + exitCode);
            }
        } catch (InterruptedException e) {
        }
        simpleperfProcess = null;
        state = STATE_STOPPED;
    }

    private String readInputStream(InputStream in) {
        byte[] buf = new byte[4096];
        String result = "";
        try {
            while (true) {
                int n = in.read(buf);
                if (n <= 0) {
                    break;
                }
                result += new String(buf, 0, n);
            }
            in.close();
        } catch (IOException e) {
        }
        return result;
    }

    private String findSimpleperf() {
        String[] candidates = new String[]{
                // For debuggable apps, simpleperf is put to the appDir by api_app_profiler.py.
                appDataDir + "/simpleperf",
                // For profileable apps on Android >= Q, use simpleperf in system image.
                "/system/bin/simpleperf"
        };
        for (String path : candidates) {
            File file = new File(path);
            if (file.isFile()) {
                return path;
            }
        }
        throw new Error("can't find simpleperf on device. Please run api_app_profiler.py.");
    }

    private void checkIfPerfEnabled() {
        String value = "";
        Process process;
        try {
            process = new ProcessBuilder()
                    .command("/system/bin/getprop", "security.perf_harden").start();
        } catch (IOException e) {
            // Omit check if getprop doesn't exist.
            return;
        }
        try {
            process.waitFor();
        } catch (InterruptedException e) {
        }
        value = readInputStream(process.getInputStream());
        if (value.startsWith("1")) {
            throw new Error("linux perf events aren't enabled on the device." +
                            " Please run api_app_profiler.py.");
        }
    }

    private void createSimpleperfDataDir() {
        File file = new File(simpleperfDataDir);
        if (!file.isDirectory()) {
            file.mkdir();
        }
    }

    private void createSimpleperfProcess(String simpleperfPath, List<String> recordArgs) {
        // 1. Prepare simpleperf arguments.
        ArrayList<String> args = new ArrayList<>();
        args.add(simpleperfPath);
        args.add("record");
        args.add("--log-to-android-buffer");
        args.add("--log");
        args.add("debug");
        args.add("--stdio-controls-profiling");
        args.add("--in-app");
        args.add("--tracepoint-events");
        args.add("/data/local/tmp/tracepoint_events");
        args.addAll(recordArgs);

        // 2. Create the simpleperf process.
        ProcessBuilder pb = new ProcessBuilder(args).directory(new File(simpleperfDataDir));
        try {
            simpleperfProcess = pb.start();
        } catch (IOException e) {
            throw new Error("failed to create simpleperf process: " + e.getMessage());
        }

        // 3. Wait until simpleperf starts recording.
        String startFlag = readReply();
        if (!startFlag.equals("started")) {
            throw new Error("failed to receive simpleperf start flag");
        }
    }

    private void sendCmd(String cmd) {
        cmd += "\n";
        try {
            simpleperfProcess.getOutputStream().write(cmd.getBytes());
            simpleperfProcess.getOutputStream().flush();
        } catch (IOException e) {
            throw new Error("failed to send cmd to simpleperf: " + e.getMessage());
        }
        if (!readReply().equals("ok")) {
            throw new Error("failed to run cmd in simpleperf: " + cmd);
        }
    }

    private String readReply() {
        String s = "";
        while (true) {
            int c = -1;
            try {
                c = simpleperfProcess.getInputStream().read();
            } catch (IOException e) {
            }
            if (c == -1 || c == '\n') {
                break;
            }
            s += (char)c;
        }
        return s;
    }

    private static final int STATE_NOT_YET_STARTED = 0;
    private static final int STATE_STARTED = 1;
    private static final int STATE_PAUSED = 2;
    private static final int STATE_STOPPED = 3;

    private int state = STATE_NOT_YET_STARTED;
    private String appDataDir;
    private String simpleperfDataDir;
    private Process simpleperfProcess;
}
