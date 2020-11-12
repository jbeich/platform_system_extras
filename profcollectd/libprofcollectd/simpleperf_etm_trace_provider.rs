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

use std::fs::read_dir;
use std::path::{Path, PathBuf};
use std::time::Duration;
use trace_provider::TraceProvider;

use crate::trace_provider;

static ETM_TRACEFILE_EXTENSION: &str = "etmtrace";
static ETM_PROFILE_EXTENSION: &str = "data";

pub struct SimpleperfEtmTraceProvider {}

impl TraceProvider for SimpleperfEtmTraceProvider {
    fn get_name(&self) -> &str {
        "simpleperf_etm"
    }

    fn trace(&self, trace_dir: &Path, tag: &str, sampling_period: &Duration) {
        let mut trace_file = PathBuf::from(trace_dir);
        trace_file.set_file_name(trace_provider::construct_file_name(tag));
        trace_file.set_extension(ETM_TRACEFILE_EXTENSION);

        simpleperf_profcollect::record(&trace_file, sampling_period)
    }

    fn process(&self, trace_dir: &Path, profile_dir: &Path) {
        let traces = read_dir(trace_dir)
            .unwrap()
            .filter_map(|e| e.ok())
            .map(|e| e.path())
            .filter(|e| {
                e.is_file()
                    && e.extension()
                        .filter(|f| f.to_str().unwrap() == ETM_TRACEFILE_EXTENSION)
                        .is_some()
            })
            .collect::<Vec<PathBuf>>();

        for trace_file in traces {
            let mut profile_file = PathBuf::from(profile_dir);
            profile_file.set_file_name(trace_file.file_name().unwrap());
            profile_file.set_extension(ETM_PROFILE_EXTENSION);
            simpleperf_profcollect::process(&trace_file, &profile_file);
        }
    }
}

impl SimpleperfEtmTraceProvider {
    pub fn supported() -> bool {
        simpleperf_profcollect::has_support()
    }
}
