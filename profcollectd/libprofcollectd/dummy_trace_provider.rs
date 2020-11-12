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

use std::path::Path;
use std::time::Duration;
use trace_provider::TraceProvider;

use crate::trace_provider;

static ENABLE_DUMMY_TRACE_PROVIDER: bool = false;

pub struct DummyTraceProvider {}

impl TraceProvider for DummyTraceProvider {
    fn get_name(&self) -> &'static str {
        "dummy"
    }

    fn trace(&self, trace_dir: &Path, tag: &str, sampling_period: &Duration) {
        log::warn!(
            "Trace event: output path {}, output file {}, sampling period {}ms",
            trace_dir.to_str().unwrap(),
            trace_provider::construct_file_name(tag),
            sampling_period.as_millis()
        );
    }

    fn process(&self, trace_dir: &Path, profile_dir: &Path) {
        log::warn!(
            "Process event: trace path {}, profile path {}",
            trace_dir.to_str().unwrap(),
            profile_dir.to_str().unwrap()
        );
    }
}

impl DummyTraceProvider {
    pub fn supported() -> bool {
        ENABLE_DUMMY_TRACE_PROVIDER
    }
}
