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

use log::debug;

use trace_provider::TraceProvider;
use std::time::Duration;
use std::path::PathBuf;

use crate::trace_provider;

pub struct DummyTraceProvider {
}

impl TraceProvider for DummyTraceProvider {
    fn get_name(&self) -> &str {
        "dummy_trace_provider"
    }

    fn trace(&self, output_path: &PathBuf, tag: &str, sampling_period : &Duration) {
        debug!("Trace event: tag {}, output path {}, sampling period {}ms", tag, output_path.to_str().unwrap(), sampling_period.as_micros());
    }
}

impl DummyTraceProvider {
    pub fn supported() -> bool {
        true
    }
}