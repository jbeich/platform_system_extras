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

//! ProfCollect trace provider trait.

use std::path::Path;
use std::sync::Arc;
use std::time::{Duration, SystemTime};

use crate::dummy_trace_provider::DummyTraceProvider;
use crate::simpleperf_etm_trace_provider::SimpleperfEtmTraceProvider;

pub trait TraceProvider {
    fn get_name(&self) -> &str;
    fn trace(&self, trace_path: &Path, tag: &str, sampling_period: &Duration);
    //fn process(&self, trace_path: &Path, profile_path: &Path);
}

pub fn get_trace_provider() -> Result<Arc<dyn TraceProvider + Send + Sync>, &'static str> {
    if SimpleperfEtmTraceProvider::supported() {
        return Ok(Arc::new(SimpleperfEtmTraceProvider {}));
    }

    if DummyTraceProvider::supported() {
        return Ok(Arc::new(DummyTraceProvider {}));
    }

    Err("No trace provider available for this device.")
}

pub fn construct_file_name(tag: &str) -> String {
    // TODO: Format datetime.
    let since_epoch = SystemTime::now().duration_since(SystemTime::UNIX_EPOCH).unwrap().as_secs();
    format!("{}-{}", tag, since_epoch.to_string())
}
