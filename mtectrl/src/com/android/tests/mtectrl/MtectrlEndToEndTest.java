/*
 * Copyright (C) 2022 The Android Open Source Project
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

package com.android.tests.mtectrl;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assume.assumeThat;

import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

// Test the protocol described in
// https://source.android.com/docs/security/test/memory-safety/bootloader-support.
// This will reboot the device multiple times, which is perfectly normal.

@RunWith(DeviceJUnit4ClassRunner.class)
public class MtectrlEndToEndTest extends BaseHostJUnit4Test {
    private String mPreviousState = null;

    @Before
    public void setUp() throws Exception {
        assumeThat(getDevice().getProperty("ro.arm64.memtag.bootctl_supported"), equalTo("1"));

        mPreviousState = getDevice().getProperty("arm64.memtag.bootctl");
        // MTE is currently on, so we should turn it off.
        if (getDevice().executeShellV2Command("cat /proc/cpuinfo | grep -q mte").getExitCode()
                == 0) {
            getDevice().setProperty("arm64.memtag.bootctl", "");
            java.lang.Thread.sleep(1000);  // TODO(b/256042002): Remove once race is fixed.
            getDevice().reboot();
        }

        // Make sure it's successfully turned off. This is assume and not assert
        // because the device could unconditionally turn on MTE.
        assumeThat(
                getDevice().executeShellV2Command("cat /proc/cpuinfo | grep -q mte").getExitCode(),
                not(equalTo(0)));
    }

    @After
    public void tearDown() throws Exception {
        if (mPreviousState != null) {
            getDevice().setProperty("arm64.memtag.bootctl", mPreviousState);
            java.lang.Thread.sleep(1000);  // TODO(b/256042002): Remove once race is fixed.
        }
    }

    @Test
    public void testMemtagOnce() throws Exception {
        getDevice().setProperty("arm64.memtag.bootctl", "memtag-once");
        java.lang.Thread.sleep(1000);  // TODO(b/256042002): Remove once race is fixed.
        getDevice().reboot();
        assertThat(
                        getDevice()
                                .executeShellV2Command("cat /proc/cpuinfo | grep -q mte")
                                .getExitCode())
                .isEqualTo(0);
        getDevice().reboot();
        assertThat(
                        getDevice()
                                .executeShellV2Command("cat /proc/cpuinfo | grep -q mte")
                                .getExitCode())
                .isNotEqualTo(0);
    }

    @Test
    public void testMemtag() throws Exception {
        getDevice().setProperty("arm64.memtag.bootctl", "memtag");
        java.lang.Thread.sleep(1000);  // TODO(b/256042002): Remove once race is fixed.
        getDevice().reboot();
        assertThat(
                        getDevice()
                                .executeShellV2Command("cat /proc/cpuinfo | grep -q mte")
                                .getExitCode())
                .isEqualTo(0);
        getDevice().reboot();
        assertThat(
                        getDevice()
                                .executeShellV2Command("cat /proc/cpuinfo | grep -q mte")
                                .getExitCode())
                .isEqualTo(0);
    }
}
