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

//! ProfCollect Binder service implementation.

use binder::{public_api::Result, Status};
use profcollectd_aidl_interface::aidl::com::android::server::profcollect::IProfCollectd::IProfCollectd;
use std::fs::{create_dir, read_to_string, remove_dir_all, write};
use std::{
    str::FromStr,
    sync::{Mutex, MutexGuard},
};

use crate::config::{Config, CONFIG_FILE, PROFILE_OUTPUT_DIR, REPORT_OUTPUT_DIR, TRACE_OUTPUT_DIR};
use crate::report::pack_report;
use crate::scheduler::Scheduler;

fn err_msg_to_binder_status(_msg: &str) -> Status {
    // TODO: Forward the error message.
    Status::new_service_specific_error(1, None)
}

pub struct ProfcollectdBinderService {
    lock: Mutex<Lock>,
}

struct Lock {
    config: Config,
    scheduler: Scheduler,
}

impl binder::Interface for ProfcollectdBinderService {}

impl IProfCollectd for ProfcollectdBinderService {
    fn schedule(&self) -> Result<()> {
        let lock = &mut *self.lock();
        lock.scheduler.schedule_periodic(&lock.config).map_err(err_msg_to_binder_status)
    }
    fn terminate(&self) -> Result<()> {
        self.lock().scheduler.terminate_periodic().map_err(err_msg_to_binder_status)
    }
    fn trace_once(&self, tag: &str) -> Result<()> {
        let lock = &mut *self.lock();
        lock.scheduler.one_shot(&lock.config, tag).map_err(err_msg_to_binder_status)
    }
    fn process(&self, blocking: bool) -> Result<()> {
        let lock = &mut *self.lock();
        lock.scheduler.process(blocking);
        Ok(())
    }
    fn report(&self) -> Result<()> {
        self.process(true)?;
        pack_report(&PROFILE_OUTPUT_DIR, &REPORT_OUTPUT_DIR);
        Ok(())
    }
    fn get_supported_provider(&self) -> Result<String> {
        Ok(self.lock().scheduler.get_trace_provider_name().to_string())
    }
}

impl ProfcollectdBinderService {
    pub fn new() -> std::result::Result<Self, &'static str> {
        let new_scheduler = Scheduler::new()?;
        let new_config = Config::from_env();

        // Replace with std::bool::then_some once feature(bool_to_option) becomes stable.
        let then_some = |b| match b {
            true => Some(()),
            false => None,
        };

        read_to_string(*CONFIG_FILE)
            .ok()
            .and_then(|s| Config::from_str(&s).ok())
            .and_then(|c| then_some(c == new_config))
            .unwrap_or_else(|| {
                // Config changed.
                log::info!("Config change detected, clearing traces.");
                remove_dir_all(*PROFILE_OUTPUT_DIR).ok();
                remove_dir_all(*TRACE_OUTPUT_DIR).ok();
                create_dir(*PROFILE_OUTPUT_DIR).unwrap();
                create_dir(*TRACE_OUTPUT_DIR).unwrap();
                write(*CONFIG_FILE, &new_config.to_string()).unwrap();
            });

        Ok(ProfcollectdBinderService {
            lock: Mutex::new(Lock { scheduler: new_scheduler, config: new_config }),
        })
    }

    fn lock(&self) -> MutexGuard<Lock> {
        self.lock.lock().unwrap()
    }
}
