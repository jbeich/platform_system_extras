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

//! ProfCollect configurations.

use serde::{Deserialize, Serialize};
use std::time::Duration;
use std::{path::PathBuf, str::FromStr};

static PROFCOLLECT_CONFIG_NAMESPACE: &str = "profcollect_native_boot";

#[derive(Clone, PartialEq, Eq, Serialize, Deserialize, Debug)]
pub struct Config {
    pub build_fingerprint: String,
    pub collection_interval: Duration,
    pub sampling_period: Duration,
    pub trace_output_dir: PathBuf,
    pub profile_output_dir: PathBuf,
    pub binary_filter: String,
}

impl Config {
    pub fn read_config() -> Self {
        let config = Config {
            build_fingerprint: get_build_fingerprint(),
            collection_interval: Duration::from_secs(get_device_config("collection_interval", 600)),
            sampling_period: Duration::from_millis(get_device_config("sampling_period", 500)),
            trace_output_dir: PathBuf::from("/data/misc/profcollectd/trace/"),
            profile_output_dir: PathBuf::from("/data/misc/profcollectd/output/"),
            binary_filter: String::new(),
        };
        println!("serialized = {}", serde_json::to_string(&config).unwrap());
        config
    }
}

fn get_build_fingerprint() -> String {
    "".to_string()
}

fn get_device_config<T>(key: &str, default: T) -> T
where
    T: FromStr + ToString,
    T::Err: std::fmt::Debug,
{
    let default = default.to_string();
    let config = profcollect_libflags_rust::get_server_configurable_flag(
        &PROFCOLLECT_CONFIG_NAMESPACE,
        &key,
        &default,
    );
    T::from_str(&config).unwrap()
}
