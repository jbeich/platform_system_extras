//
// Copyright (C) 2021 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

//! This module implements safe wrappers for simpleperf etm operations required
//! by profcollect.

use std::ffi::CString;
use std::time::Duration;

/// Returns whether the system has support for simpleperf etm.
pub fn has_support() -> bool {
    unsafe {
        simpleperf_profcollect_bindgen::HasSupport()
    }
}

/// Trigger an ETM trace event.
pub fn record(output_path: &str, duration: &Duration) {
    let output_path = CString::new(output_path).unwrap();
    let duration = duration.as_secs_f32();

    unsafe {
        simpleperf_profcollect_bindgen::Record(output_path.as_ptr(), duration);
    }
}
