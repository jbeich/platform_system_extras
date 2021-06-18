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

//! Dummy provider for development and testing purposes.

use anyhow::Result;
use std::path::{Path, PathBuf};
use std::time::Duration;
use trace_provider::TraceProvider;

use crate::trace_provider;

static DUMMY_TRACEFILE_EXTENSION: &str = "dummytrace";

pub struct DummyTraceProvider {}

impl TraceProvider for DummyTraceProvider {
    fn get_name(&self) -> &'static str {
        "dummy"
    }

    fn trace(&self, trace_dir: &Path, tag: &str, sampling_period: &Duration) {
        let mut trace_file = PathBuf::from(trace_dir);
        trace_file.push(trace_provider::construct_file_name(tag));
        trace_file.set_extension(DUMMY_TRACEFILE_EXTENSION);
        log::info!(
            "Trace event triggered, tag {}, sampling for {}ms, saving to {}",
            tag,
            sampling_period.as_millis(),
            trace_file.display()
        );
    }

    fn process(&self, _trace_dir: &Path, _profile_dir: &Path) -> Result<()> {
        log::info!("Process event triggered");
        Ok(())
    }
}

impl DummyTraceProvider {
    pub fn supported() -> bool {
        true
    }
}
