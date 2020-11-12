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

//! A dummy trace provider for debugging.

use trace_provider::TraceProvider;
use std::time::Duration;
use std::path::{Path, PathBuf};

use crate::trace_provider;

static ETM_TRACEFILE_EXTENSION: &str = "etmtrace";

pub struct SimpleperfEtmTraceProvider {
}

impl TraceProvider for SimpleperfEtmTraceProvider {
    fn get_name(&self) -> &str {
        "simpleperf_etm"
    }

    fn trace(&self, trace_path: &Path, tag: &str, sampling_period : &Duration) {
        let mut trace_file = PathBuf::from(trace_path);
        trace_file.set_file_name(trace_provider::construct_file_name(tag));
        trace_file.set_extension(ETM_TRACEFILE_EXTENSION);

        simpleperf_profcollect::record(trace_file.to_str().unwrap(), sampling_period)
    }
}

impl SimpleperfEtmTraceProvider {
    pub fn supported() -> bool {
        simpleperf_profcollect::has_support()
    }
}