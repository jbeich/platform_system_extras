//
// Copyright (C) 2020 The Android Open Source Project
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

//! ProfCollect Binder client interface.

mod config;
mod report;
mod scheduler;
mod service;
mod simpleperf_etm_trace_provider;
mod trace_provider;

use profcollectd_aidl_interface::aidl::com::android::server::profcollect::IProfCollectd::{
    self, BnProfCollectd,
};
use profcollectd_aidl_interface::binder;
use service::ProfcollectdBinderService;

static PROFCOLLECTD_SERVICE_NAME: &str = "profcollectd";

/// Initialise profcollectd service.
/// * `schedule_now` - Immediately schedule collection after service is initialised.
pub fn init_service(schedule_now: bool) {
    binder::ProcessState::start_thread_pool();

    let profcollect_binder_service = ProfcollectdBinderService::new().unwrap();
    binder::add_service(
        &PROFCOLLECTD_SERVICE_NAME,
        BnProfCollectd::new_binder(profcollect_binder_service).as_binder(),
    )
    .expect("Could not register service.");

    if schedule_now {
        trace_once("boot");
        schedule();
    }

    binder::ProcessState::join_thread_pool();
}

fn get_profcollectd_service() -> binder::Strong<dyn IProfCollectd::IProfCollectd> {
    binder::get_interface(&PROFCOLLECTD_SERVICE_NAME)
        .expect("Could not get profcollectd binder service")
}

/// Schedule periodic profile collection.
pub fn schedule() {
    get_profcollectd_service().schedule().expect("Schedule failed.");
}

/// Terminate periodic profile collection.
pub fn terminate() {
    get_profcollectd_service().terminate().expect("Terminate failed.");
}

/// Immediately schedule a one-off trace.
pub fn trace_once(tag: &str) {
    get_profcollectd_service().trace_once(tag).expect("Trace failed");
}

/// Process traces.
pub fn process() {
    get_profcollectd_service().process(true).expect("Failed to process profiles.");
}

/// Process traces and report profile.
pub fn report() {
    get_profcollectd_service().report().expect("Failed to generate report.");
}

/// Inits logging for Android
pub fn init_logging() {
    android_logger::init_once(
        android_logger::Config::default()
            .with_tag("profcollectd")
            .with_min_level(log::Level::Error),
    );
}
